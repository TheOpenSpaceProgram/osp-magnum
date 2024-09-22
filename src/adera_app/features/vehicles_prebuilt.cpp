/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "vehicles_prebuilt.h"

#include "../feature_interfaces.h"

#include <adera/machines/links.h>

#include <osp/core/Resources.h>

using namespace Magnum::Math::Literals;
using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::fw;
using namespace osp::link;
using namespace osp;

using osp::restypes::gc_importer;

namespace adera
{

Matrix4 quick_transform(Vector3 const pos, Quaternion const rot) noexcept
{
    return Matrix4::from(rot.toMatrix(), pos);
}

struct RCSInputs
{
    NodeId m_pitch  {lgrn::id_null<NodeId>()};
    NodeId m_yaw    {lgrn::id_null<NodeId>()};
    NodeId m_roll   {lgrn::id_null<NodeId>()};
};

static void add_rcs_machines(VehicleBuilder& rFB, RCSInputs const& inputs, PartId part, float thrustMul, Matrix4 const& tf)
{
    namespace ports_rcsdriver = adera::ports_rcsdriver;
    namespace ports_magicrocket = adera::ports_magicrocket;

    auto const [posX, posY, posZ, dirX, dirY, dirZ, driverOut, thrMul] = rFB.create_nodes<8>(gc_ntSigFloat);

    rFB.create_machine(part, gc_mtRcsDriver, {
        { ports_rcsdriver::gc_posXIn,       posX            },
        { ports_rcsdriver::gc_posYIn,       posY            },
        { ports_rcsdriver::gc_posZIn,       posZ            },
        { ports_rcsdriver::gc_dirXIn,       dirX            },
        { ports_rcsdriver::gc_dirYIn,       dirY            },
        { ports_rcsdriver::gc_dirZIn,       dirZ            },
        { ports_rcsdriver::gc_cmdAngXIn,    inputs.m_pitch  },
        { ports_rcsdriver::gc_cmdAngYIn,    inputs.m_yaw    },
        { ports_rcsdriver::gc_cmdAngZIn,    inputs.m_roll   },
        { ports_rcsdriver::gc_throttleOut,  driverOut       }
    } );

    rFB.create_machine(part, gc_mtMagicRocket, {
        { ports_magicrocket::gc_throttleIn, driverOut },
        { ports_magicrocket::gc_multiplierIn, thrMul }
    } );

    Vector3 const dir = tf.rotation() * gc_rocketForward;

    auto &rFloatValues = rFB.node_values< SignalValues_t<float> >(gc_ntSigFloat);

    rFloatValues[posX] = tf.translation().x();
    rFloatValues[posY] = tf.translation().y();
    rFloatValues[posZ] = tf.translation().z();
    rFloatValues[dirX] = dir.x();
    rFloatValues[dirY] = dir.y();
    rFloatValues[dirZ] = dir.z();
    rFloatValues[thrMul] = thrustMul;
}

static void add_rcs_block(VehicleBuilder& rFB, VehicleBuilder::WeldVec_t& rWeldTo, RCSInputs const& inputs, float thrustMul, Vector3 pos, Quaternion rot)
{
    constexpr Vector3 xAxis{1.0f, 0.0f, 0.0f};

    auto const [ nozzleA, nozzleB ] = rFB.create_parts<2>();
    rFB.set_prefabs({
        { nozzleA,  "phLinRCS" },
        { nozzleB,  "phLinRCS" }
    });

    Matrix4 const nozzleTfA = quick_transform(pos, rot * Quaternion::rotation(90.0_degf,  xAxis));
    Matrix4 const nozzleTfB = quick_transform(pos, rot * Quaternion::rotation(-90.0_degf, xAxis));

    add_rcs_machines(rFB, inputs, nozzleA, thrustMul, nozzleTfA);
    add_rcs_machines(rFB, inputs, nozzleB, thrustMul, nozzleTfB);

    rWeldTo.push_back({ nozzleA, nozzleTfA });
    rWeldTo.push_back({ nozzleB, nozzleTfB });
}

FeatureDef const ftrPrebuiltVehicles = feature_def("PrebuiltVehicles", [] (
        FeatureBuilder              &rFB,
        Implement<FITestVehicles>   testVhcls,
        DependOn<FICleanupContext>  cleanup,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIScene>           scn)
{
    auto &rResources = rFB.data_get<Resources>(mainApp.di.resources);

    auto &rPrebuiltVehicles = rFB.data_emplace<PrebuiltVehicles>(testVhcls.di.prebuiltVehicles);
    rPrebuiltVehicles.resize(PrebuiltVhIdReg_t::size());

    using namespace adera;

    // Build "PartVehicle"
    {
        VehicleBuilder vbuilder{&rResources};
        VehicleBuilder::WeldVec_t toWeld;

        auto const [ capsule, fueltank, engineA, engineB ] = vbuilder.create_parts<4>();
        vbuilder.set_prefabs({
            { capsule,  "phCapsule" },
            { fueltank, "phFuselage" },
            { engineA,  "phEngine" },
            { engineB,  "phEngine" },
        });

        toWeld.push_back( {capsule,  quick_transform({ 0.0f,  0.0f,  3.0f}, {})} );
        toWeld.push_back( {fueltank, quick_transform({ 0.0f,  0.0f,  0.0f}, {})} );
        toWeld.push_back( {engineA,  quick_transform({ 0.7f,  0.0f, -2.9f}, {})} );
        toWeld.push_back( {engineB,  quick_transform({-0.7f,  0.0f, -2.9f}, {})} );

        namespace ports_magicrocket = adera::ports_magicrocket;
        namespace ports_userctrl = adera::ports_userctrl;

        auto const [ pitch, yaw, roll, throttle, thrustMul ] = vbuilder.create_nodes<5>(gc_ntSigFloat);

        auto &rFloatValues = vbuilder.node_values< SignalValues_t<float> >(gc_ntSigFloat);
        rFloatValues[thrustMul] = 50000.0f;

        vbuilder.create_machine(capsule, gc_mtUserCtrl, {
            { ports_userctrl::gc_throttleOut,   throttle },
            { ports_userctrl::gc_pitchOut,      pitch    },
            { ports_userctrl::gc_yawOut,        yaw      },
            { ports_userctrl::gc_rollOut,       roll     }
        } );

        vbuilder.create_machine(engineA, gc_mtMagicRocket, {
            { ports_magicrocket::gc_throttleIn, throttle },
            { ports_magicrocket::gc_multiplierIn, thrustMul }
        } );

        vbuilder.create_machine(engineB, gc_mtMagicRocket, {
            { ports_magicrocket::gc_throttleIn, throttle },
            { ports_magicrocket::gc_multiplierIn, thrustMul }
        } );

        RCSInputs rcsInputs{pitch, yaw, roll};

        static constexpr int const   rcsRingBlocks   = 4;
        static constexpr int const   rcsRingCount    = 2;
        static constexpr float const rcsRingZ        = -2.0f;
        static constexpr float const rcsZStep        = 4.0f;
        static constexpr float const rcsRadius       = 1.1f;
        static constexpr float const rcsThrust       = 3000.0f;

        for (int ring = 0; ring < rcsRingCount; ++ring)
        {
            Vector3 const rcsOset{rcsRadius, 0.0f, rcsRingZ + float(ring)*rcsZStep };

            for (Rad ang = 0.0_degf; ang < Rad(360.0_degf); ang += Rad(360.0_degf)/rcsRingBlocks)
            {
               Quaternion const rotZ = Quaternion::rotation(ang, {0.0f, 0.0f, 1.0f});
               add_rcs_block(vbuilder, toWeld, rcsInputs, rcsThrust, rotZ.transformVector(rcsOset), rotZ);
            }
        }

        vbuilder.weld(toWeld);

        rPrebuiltVehicles[gc_pbvSimpleCommandServiceModule] = std::make_unique<VehicleData>(std::move(vbuilder.finalize_release()));
    }


    // Put more prebuilt vehicles here!


    rFB.task()
        .name       ("Clean up prebuilt vehicles")
        .run_on     ({cleanup.pl.cleanup(Run_)})
        .args       ({         testVhcls.di.prebuiltVehicles,  mainApp.di.resources})
        .func       ([] (PrebuiltVehicles &rPrebuildVehicles, Resources& rResources) noexcept
    {
        for (std::unique_ptr<VehicleData> &rpData : rPrebuildVehicles)
        {
            if (rpData != nullptr)
            {
                for (PrefabPair &rPrefabPair : rpData->m_partPrefabs)
                {
                    rResources.owner_destroy(gc_importer, std::move(rPrefabPair.m_importer));
                }
            }
        }
        rPrebuildVehicles.clear();
    });

}); // setup_prebuilt_vehicles


} // namespace adera
