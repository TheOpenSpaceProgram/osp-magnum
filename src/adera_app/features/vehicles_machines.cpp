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
#include "vehicles_machines.h"
#include "misc.h"

#include "../feature_interfaces.h"

#include <adera/activescene/vehicles_vb_fn.h>
#include <adera/drawing/CameraController.h>
#include <adera/machines/links.h>

#include <osp/activescene/basic.h>
#include <osp/activescene/physics.h>
#include <osp/activescene/prefab_fn.h>
#include <osp/core/Resources.h>
#include <osp/drawing/drawing.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/util/logging.h>
#include <osp/util/UserInputHandler.h>

using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp::link;
using namespace osp;

namespace adera
{

FeatureDef const ftrMachMagicRockets = feature_def("MachMagicRockets", [] (
        FeatureBuilder              &rFB,
        DependOn<FIScene>           scn,
        DependOn<FIParts>           parts,
        DependOn<FISignalsFloat>    sigFloat)
{
    rFB.task()
        .name       ("Allocate Machine update bitset for MagicRocket")
        .run_on     ({scn.pl.update(Run)})
        .sync_with  ({parts.pl.machIds(Ready), parts.pl.machUpdExtIn(New)})
        .args       ({parts.di.scnParts, parts.di.updMach})
        .func       ([] (ACtxParts& rScnParts, MachineUpdater& rUpdMach)
    {
        rUpdMach.localDirty[gc_mtMagicRocket].resize(rScnParts.machines.perType[gc_mtMagicRocket].localIds.capacity());
    });
}); // setup_mach_rocket


struct ThrustIndicator
{
    MaterialId      material;
    Magnum::Color4  color;
    MeshIdOwner_t   mesh;

    KeyedVec<MachLocalId, DrawEnt> rktToDrawEnt;

    float           indicatorScale {0.0001f};
};

FeatureDef const ftrMagicRocketThrustIndicator = feature_def("MagicRocketThrustIndicator", [] (
        FeatureBuilder              &rFB,
        Implement<FIRktIndicator>   rktIndicate,
        DependOn<FICleanupContext>  cleanup,
        DependOn<FIMainApp>         mainApp,
        DependOn<FICommonScene>     comScn,
        DependOn<FIParts>           parts,
        DependOn<FISignalsFloat>    sigFloat,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        entt::any                   userData)
{
    auto const [pkg, material] = entt::any_cast<TplPkgIdMaterialId>(userData);

    auto &rResources        = rFB.data_get< Resources >      (mainApp.di.resources);
    auto &rBasic            = rFB.data_get< ACtxBasic >      (comScn.di.basic);
    auto &rDrawing          = rFB.data_get< ACtxDrawing >    (comScn.di.drawing);
    auto &rDrawingRes       = rFB.data_get< ACtxDrawingRes > (comScn.di.drawingRes);
    auto &rScnRender        = rFB.data_get< ACtxSceneRender >(scnRender.di.scnRender);
    auto &rDrawTfObservers  = rFB.data_get< DrawTfObservers >(scnRender.di.drawTfObservers);
    auto &rScnParts         = rFB.data_get< ACtxParts >      (parts.di.scnParts);
    auto &rSigValFloat      = rFB.data_get< SignalValues_t<float> > (sigFloat.di.sigValFloat);
    auto &rThrustIndicator  = rFB.data_emplace<ThrustIndicator>(rktIndicate.di.indicator);

    rThrustIndicator.material   = material;
    rThrustIndicator.color      = { 1.0f, 0.2f, 0.8f, 1.0f };
    rThrustIndicator.mesh       = SysRender::add_drawable_mesh(rDrawing, rDrawingRes, rResources, pkg, "cone");

    rFB.task()
        .name       ("Create DrawEnts for Thrust indicators")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({scnRender.pl.drawEntResized(ModifyOrSignal), scnRender.pl.drawEnt(New), parts.pl.machIds(Ready)})
        .args       ({               scnRender.di.scnRender,                  parts.di.scnParts,                 rktIndicate.di.indicator})
        .func([]    (ACtxSceneRender &rScnRender,  ACtxParts const& rScnParts, ThrustIndicator& rThrustIndicator) noexcept
    {
        PerMachType const& rockets = rScnParts.machines.perType[gc_mtMagicRocket];

        rThrustIndicator.rktToDrawEnt.resize(rockets.localIds.capacity());

        for (MachLocalId const localId : rockets.localIds)
        {
            DrawEnt& rDrawEnt = rThrustIndicator.rktToDrawEnt[localId];
            if (rDrawEnt == lgrn::id_null<DrawEnt>())
            {
                rDrawEnt = rScnRender.m_drawIds.create();
            }
        }
    });

    rFB.task()
        .name       ("Add mesh and materials to Thrust indicators")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({scnRender.pl.drawEntResized(Done), scnRender.pl.drawEnt(Ready), scnRender.pl.entMesh(New), scnRender.pl.material(New), scnRender.pl.materialDirty(Modify_), scnRender.pl.entMeshDirty(Modify_)})
        .args       ({         comScn.di.basic,                 scnRender.di.scnRender,             comScn.di.drawing,                      comScn.di.drawingRes,                 parts.di.scnParts,                             sigFloat.di.sigValFloat,                 rktIndicate.di.indicator})
        .func([]    (ACtxBasic& rBasic, ACtxSceneRender &rScnRender, ACtxDrawing& rDrawing, ACtxDrawingRes const& rDrawingRes, ACtxParts const& rScnParts, SignalValues_t<float> const& rSigValFloat, ThrustIndicator& rThrustIndicator) noexcept
    {
        Material            &rMat           = rScnRender.m_materials[rThrustIndicator.material];
        PerMachType const   &rockets        = rScnParts.machines.perType[gc_mtMagicRocket];
        Nodes const         &floats         = rScnParts.nodePerType[gc_ntSigFloat];

        for (MachLocalId const localId : rockets.localIds)
        {
            DrawEnt const   drawEnt         = rThrustIndicator.rktToDrawEnt[localId];

            MachAnyId const anyId           = rockets.localToAny[localId];
            PartId const    part            = rScnParts.machineToPart[anyId];
            ActiveEnt const partEnt         = rScnParts.partToActive[part];

            auto const&     portSpan        = floats.machToNode[anyId];
            NodeId const    throttleIn      = connected_node(portSpan, ports_magicrocket::gc_throttleIn.port);
            NodeId const    multiplierIn    = connected_node(portSpan, ports_magicrocket::gc_multiplierIn.port);

            float const     throttle        = std::clamp(rSigValFloat[throttleIn], 0.0f, 1.0f);
            float const     multiplier      = rSigValFloat[multiplierIn];
            float const     thrustMag       = throttle * multiplier;

            if (thrustMag == 0.0f)
            {
                rScnRender.m_visible.erase(drawEnt);
                continue;
            }

            if (!rMat.m_ents.contains(drawEnt))
            {
                rMat.m_ents.insert(drawEnt);
                rMat.m_dirty.push_back(drawEnt);
            }

            MeshIdOwner_t &rMeshOwner = rScnRender.m_mesh[drawEnt];
            if ( ! rMeshOwner.has_value() )
            {
                rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rThrustIndicator.mesh.value());
                rScnRender.m_meshDirty.push_back(drawEnt);
            }

            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque .insert(drawEnt);

            rScnRender.m_color              [drawEnt] = rThrustIndicator.color;
            rScnRender.drawTfObserverEnable [partEnt] = 1;

            SysRender::needs_draw_transforms(rBasic.m_scnGraph, rScnRender.m_needDrawTf, partEnt);
        }
    });

    using UserData_t = DrawTfObservers::UserData_t;

    DrawTfObservers::Observer &rObserver = rDrawTfObservers.observers[0];

    rObserver.data = { &rThrustIndicator, &rScnParts, &rSigValFloat };
    rObserver.func = [] (ACtxSceneRender& rCtxScnRdr, Matrix4 const& drawTf, active::ActiveEnt ent, int depth, UserData_t data) noexcept
    {
        auto &rThrustIndicator          = *static_cast< ThrustIndicator* >          (data[0]);
        auto &rScnParts                 = *static_cast< ACtxParts* >                (data[1]);
        auto &rSigValFloat              = *static_cast< SignalValues_t<float>* >    (data[2]);

        PerMachType const   &rockets    = rScnParts.machines.perType[gc_mtMagicRocket];
        Nodes const         &floats     = rScnParts.nodePerType[gc_ntSigFloat];

        PartId const        part        = rScnParts.activeToPart[ent];
        ActiveEnt const     partEnt     = rScnParts.partToActive[part];

        for (MachinePair const pair : rScnParts.partToMachines[part])
        {
            if (pair.type == gc_mtMagicRocket)
            {
                DrawEnt const   drawEnt         = rThrustIndicator.rktToDrawEnt[pair.local];
                MachAnyId const anyId           = rockets.localToAny[pair.local];

                auto const&     portSpan        = floats.machToNode[anyId];
                NodeId const    throttleIn      = connected_node(portSpan, ports_magicrocket::gc_throttleIn.port);
                NodeId const    multiplierIn    = connected_node(portSpan, ports_magicrocket::gc_multiplierIn.port);

                float const     throttle        = std::clamp(rSigValFloat[throttleIn], 0.0f, 1.0f);
                float const     multiplier      = rSigValFloat[multiplierIn];
                float const     thrustMag       = throttle * multiplier;

                rCtxScnRdr.m_drawTransform[drawEnt]
                        = drawTf
                        * Matrix4::scaling({1.0f, 1.0f, thrustMag * rThrustIndicator.indicatorScale})
                        * Matrix4::translation({0.0f, 0.0f, -1.0f})
                        * Matrix4::scaling({0.2f, 0.2f, 1.0f}); }
        }
    };

    rFB.task()
        .name       ("Clean up ThrustIndicator")
        .run_on     ({cleanup.pl.cleanup(Run_)})
        .args       ({      mainApp.di.resources,             comScn.di.drawing,                 rktIndicate.di.indicator})
        .func([] (Resources& rResources, ACtxDrawing& rDrawing, ThrustIndicator& rThrustIndicator) noexcept
    {
        rDrawing.m_meshRefCounts.ref_release(std::move(rThrustIndicator.mesh));
    });
}); // ftrMagicRocketThrustIndicator



FeatureDef const ftrMachRCSDriver = feature_def("RCSDriver", [] (
        FeatureBuilder              &rFB,
        DependOn<FIScene>           scn,
        DependOn<FIParts>           parts,
        DependOn<FISignalsFloat>    sigFloat)
{
    rFB.task()
        .name       ("Allocate Machine update bitset for RcsDriver")
        .run_on     ({scn.pl.update(Run)})
        .sync_with  ({parts.pl.machIds(Ready), parts.pl.machUpdExtIn(New)})
        .args       ({parts.di.scnParts, parts.di.updMach})
        .func       ([] (ACtxParts& rScnParts, MachineUpdater& rUpdMach)
    {
        rUpdMach.localDirty[gc_mtRcsDriver].resize(rScnParts.machines.perType[gc_mtRcsDriver].localIds.capacity());
    });

    rFB.task()
        .name       ("RCS Drivers calculate new values")
        .run_on     ({parts.pl.linkLoop(MachUpd)})
        .sync_with  ({parts.pl.machUpdExtIn(Ready)})
        .args       ({      parts.di.scnParts,                parts.di.updMach,                       sigFloat.di.sigValFloat,                    sigFloat.di.sigUpdFloat})
        .func([] (ACtxParts& rScnParts, MachineUpdater& rUpdMach, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.nodePerType[gc_ntSigFloat];
        PerMachType &rRockets    = rScnParts.machines.perType[gc_mtRcsDriver];

        for (MachLocalId const local : rUpdMach.localDirty[gc_mtRcsDriver])
        {
            MachAnyId const mach     = rRockets.localToAny[local];
            auto const      portSpan = lgrn::Span<NodeId const>{rFloatNodes.machToNode[mach]};

            NodeId const thrNode = connected_node(portSpan, ports_rcsdriver::gc_throttleOut.port);
            if (thrNode == lgrn::id_null<NodeId>())
            {
                continue; // Throttle Output not connected, calculations below are useless
            }

            auto const rcs_read = [&rSigValFloat, portSpan] (float& rDstVar, PortEntry const& entry)
            {
                NodeId const node = connected_node(portSpan, entry.port);

                if (node != lgrn::id_null<NodeId>())
                {
                    rDstVar = rSigValFloat[node];
                }
            };

            Vector3 pos     {0.0f};
            Vector3 dir     {0.0f};
            Vector3 cmdLin  {0.0f};
            Vector3 cmdAng  {0.0f};

            rcs_read( pos.x(),    ports_rcsdriver::gc_posXIn    );
            rcs_read( pos.y(),    ports_rcsdriver::gc_posYIn    );
            rcs_read( pos.z(),    ports_rcsdriver::gc_posZIn    );
            rcs_read( dir.x(),    ports_rcsdriver::gc_dirXIn    );
            rcs_read( dir.y(),    ports_rcsdriver::gc_dirYIn    );
            rcs_read( dir.z(),    ports_rcsdriver::gc_dirZIn    );
            rcs_read( cmdLin.x(), ports_rcsdriver::gc_cmdLinXIn );
            rcs_read( cmdLin.y(), ports_rcsdriver::gc_cmdLinYIn );
            rcs_read( cmdLin.z(), ports_rcsdriver::gc_cmdLinZIn );
            rcs_read( cmdAng.x(), ports_rcsdriver::gc_cmdAngXIn );
            rcs_read( cmdAng.y(), ports_rcsdriver::gc_cmdAngYIn );
            rcs_read( cmdAng.z(), ports_rcsdriver::gc_cmdAngZIn );

            OSP_LOG_TRACE("RCS controller {} pitch = {}", local, cmdAng.x());
            OSP_LOG_TRACE("RCS controller {} yaw = {}", local, cmdAng.y());
            OSP_LOG_TRACE("RCS controller {} roll = {}", local, cmdAng.z());

            float const thrCurr = rSigValFloat[thrNode];
            float const thrNew = thruster_influence(pos, dir, cmdLin, cmdAng);

            if (thrCurr != thrNew)
            {
                rSigUpdFloat.assign(thrNode, thrNew);
                rUpdMach.requestMachineUpdateLoop = true;
            }
        }
    });
}); // ftrRCSDriver




struct VehicleControls
{
    MachLocalId selectedUsrCtrl{lgrn::id_null<MachLocalId>()};

    input::EButtonControlIndex btnSwitch;
    input::EButtonControlIndex btnThrMax;
    input::EButtonControlIndex btnThrMin;
    input::EButtonControlIndex btnThrMore;
    input::EButtonControlIndex btnThrLess;
    input::EButtonControlIndex btnPitchUp;
    input::EButtonControlIndex btnPitchDn;
    input::EButtonControlIndex btnYawLf;
    input::EButtonControlIndex btnYawRt;
    input::EButtonControlIndex btnRollLf;
    input::EButtonControlIndex btnRollRt;
};

FeatureDef const ftrVehicleControl = feature_def("VehicleControl", [] (
        FeatureBuilder              &rFB,
        Implement<FIVehicleControl> vhclCtrl,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FIScene>           scn,
        DependOn<FIParts>           parts,
        DependOn<FISignalsFloat>    sigFloat)
{
    rFB.pipeline(vhclCtrl.pl.selectedVehicle).parent(scn.pl.update);

    auto &rUserInput = rFB.data_get< input::UserInputHandler >(windowApp.di.userInput);

    // TODO: add cleanup task
    rFB.data_emplace<VehicleControls>(vhclCtrl.di.vhControls, VehicleControls{
        .btnSwitch  = rUserInput.button_subscribe("game_switch"),
        .btnThrMax  = rUserInput.button_subscribe("vehicle_thr_max"),
        .btnThrMin  = rUserInput.button_subscribe("vehicle_thr_min"),
        .btnThrMore = rUserInput.button_subscribe("vehicle_thr_more"),
        .btnThrLess = rUserInput.button_subscribe("vehicle_thr_less"),
        .btnPitchUp = rUserInput.button_subscribe("vehicle_pitch_up"),
        .btnPitchDn = rUserInput.button_subscribe("vehicle_pitch_dn"),
        .btnYawLf   = rUserInput.button_subscribe("vehicle_yaw_lf"),
        .btnYawRt   = rUserInput.button_subscribe("vehicle_yaw_rt"),
        .btnRollLf  = rUserInput.button_subscribe("vehicle_roll_lf"),
        .btnRollRt  = rUserInput.button_subscribe("vehicle_roll_rt")
    });

    rFB.task()
        .name       ("Select vehicle")
        .run_on     ({windowApp.pl.inputs(Run)})
        .sync_with  ({vhclCtrl.pl.selectedVehicle(Modify)})
        .args       ({      parts.di.scnParts,                               windowApp.di.userInput,                 vhclCtrl.di.vhControls})
        .func([] (ACtxParts& rScnParts, input::UserInputHandler const &rUserInput, VehicleControls &rVhControls) noexcept
    {
        PerMachType &rUsrCtrl    = rScnParts.machines.perType[gc_mtUserCtrl];

        // Select a UsrCtrl machine when pressing the switch button
        if (rUserInput.button_state(rVhControls.btnSwitch).m_triggered)
        {
            ++rVhControls.selectedUsrCtrl;
            bool found = false;
            for (MachLocalId local = rVhControls.selectedUsrCtrl; local < rUsrCtrl.localIds.capacity(); ++local)
            {
                if (rUsrCtrl.localIds.exists(local))
                {
                    found = true;
                    rVhControls.selectedUsrCtrl = local;
                    break;
                }
            }

            if ( ! found )
            {
                rVhControls.selectedUsrCtrl = lgrn::id_null<MachLocalId>();
                OSP_LOG_INFO("Unselected vehicles");
            }
            else
            {
                OSP_LOG_INFO("Selected User Control: {}", rVhControls.selectedUsrCtrl);
            }
        }
    });

    rFB.task()
        .name       ("Write inputs to UserControl Machines")
        .run_on     ({scn.pl.update(Run)})
        .sync_with  ({windowApp.pl.inputs(Run), sigFloat.pl.sigFloatUpdExtIn(Modify)})
        .args       ({      parts.di.scnParts,                parts.di.updMach,                       sigFloat.di.sigValFloat,                    sigFloat.di.sigUpdFloat,                               windowApp.di.userInput,                 vhclCtrl.di.vhControls,           scn.di.deltaTimeIn})
        .func([] (ACtxParts& rScnParts, MachineUpdater& rUpdMach, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat, input::UserInputHandler const& rUserInput, VehicleControls& rVhControls, float const deltaTimeIn) noexcept
    {
        VehicleControls& rVC = rVhControls;
        auto const held = [&rUserInput] (input::EButtonControlIndex idx, float val) -> float
        {
            return rUserInput.button_state(idx).m_held ? val : 0.0f;
        };

        if (rVC.selectedUsrCtrl == lgrn::id_null<MachLocalId>())
        {
            return; // No vehicle selected
        }

        Nodes const &rFloatNodes = rScnParts.nodePerType[gc_ntSigFloat];
        float const thrRate = deltaTimeIn;

        float const thrChange
            = held(rVC.btnThrMore, thrRate) - held(rVC.btnThrLess, thrRate)
            + held(rVC.btnThrMax, 1.0f)     - held(rVC.btnThrMin, 1.0f);

        Vector3 const attitude
        {
            held(rVC.btnPitchDn, 1.0f) - held(rVC.btnPitchUp, 1.0f),
            held(rVC.btnYawLf,   1.0f) - held(rVC.btnYawRt,   1.0f),
            held(rVC.btnRollRt,  1.0f) - held(rVC.btnRollLf,  1.0f)
        };

        PerMachType     &rUsrCtrl   = rScnParts.machines.perType[gc_mtUserCtrl];
        MachAnyId const mach        = rUsrCtrl.localToAny[rVC.selectedUsrCtrl];
        auto const      portSpan    = lgrn::Span<NodeId const>{rFloatNodes.machToNode[mach]};

        bool changed = false;
        auto const write_control = [&rSigValFloat, &rSigUpdFloat, &changed, portSpan] (PortEntry const& entry, float write, bool replace = true, float min = 0.0f, float max = 1.0f)
        {
            NodeId const node = connected_node(portSpan, entry.port);
            if (node == lgrn::id_null<NodeId>())
            {
                return; // not connected
            }

            float const oldVal = rSigValFloat[node];
            float const newVal = replace ? write : Magnum::Math::clamp(oldVal + write, min, max);

            if (oldVal != newVal)
            {
                rSigUpdFloat.assign(node, newVal);
                changed = true;
            }
        };

        write_control(ports_userctrl::gc_throttleOut,   thrChange, false);
        write_control(ports_userctrl::gc_pitchOut,      attitude.x());
        write_control(ports_userctrl::gc_yawOut,        attitude.y());
        write_control(ports_userctrl::gc_rollOut,       attitude.z());

        if (changed)
        {
            rUpdMach.requestMachineUpdateLoop = true;
        }
    });
}); // ftrVehicleControl


FeatureDef const ftrVehicleCamera = feature_def("VehicleCamera", [] (
        FeatureBuilder              &rFB,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FIScene>           scn,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysics>         phys,
        DependOn<FIParts>           parts,
        DependOn<FICameraControl>   camCtrl,
        DependOn<FIVehicleControl>  vhclCtrl)
{
    // Don't add comScn.pl.transform(Modify) to sync_with, even though this uses transforms.
    // phys.pls.physUpdate(Done) assures physics transforms are done.
    //
    // camCtrl.pl.camCtrl(Ready) is needed by the shape thrower, which needs comScn.pl.transform(New),
    // causing a circular dependency. The transform pipeline probably needs to be split into
    // a few separate ones.
    rFB.task()
        .name       ("Update vehicle camera")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({camCtrl.pl.camCtrl(Modify), phys.pl.physUpdate(Done), parts.pl.mapWeldActive(Ready)})
        .args       ({                 camCtrl.di.camCtrl,           scn.di.deltaTimeIn,                 comScn.di.basic,                 vhclCtrl.di.vhControls,                 parts.di.scnParts})
        .func([] (ACtxCameraController& rCamCtrl, float const deltaTimeIn, ACtxBasic const& rBasic, VehicleControls& rVhControls, ACtxParts const& rScnParts) noexcept
    {
        if (rVhControls.selectedUsrCtrl != lgrn::id_null<MachLocalId>())
        {
            // Follow selected UserControl machine

            // Obtain associated ActiveEnt
            // MachLocalId -> MachAnyId -> PartId -> RigidGroup -> ActiveEnt
            PerMachType const&  rUsrCtrls       = rScnParts.machines.perType.at(adera::gc_mtUserCtrl);
            MachAnyId const     selectedMach    = rUsrCtrls.localToAny        .at(rVhControls.selectedUsrCtrl);
            PartId const        selectedPart    = rScnParts.machineToPart     .at(selectedMach);
            WeldId const        weld            = rScnParts.partToWeld        .at(selectedPart);
            ActiveEnt const     selectedEnt     = rScnParts.weldToActive        .at(weld);

            if (rBasic.m_transform.contains(selectedEnt))
            {
                rCamCtrl.m_target = rBasic.m_transform.get(selectedEnt).m_transform.translation();
            }
        }
        else
        {
            // Free cam when no vehicle selected
            SysCameraController::update_move(
                    rCamCtrl,
                    deltaTimeIn, true);
        }

        SysCameraController::update_view(rCamCtrl, deltaTimeIn);
    });
}); // setup_camera_vehicle


} // namespace adera
