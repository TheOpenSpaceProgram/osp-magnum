/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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
#include <iostream>

#include <osp/Active/ActiveScene.h>
#include <osp/Active/physics.h>
#include <osp/Active/SysDebugRender.h>

#include "Rocket.h"
#include "osp/Resource/AssetImporter.h"
#include "adera/Shaders/Phong.h"
#include "adera/SysExhaustPlume.h"
#include "adera/Plume.h"
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Math/Color.h>

using namespace adera::active::machines;
using namespace osp::active;
using namespace osp;

const std::string SysMachineRocket::smc_name = "Rocket";

void MachineRocket::propagate_output(WireOutput* output)
{

}

WireInput* MachineRocket::request_input(WireInPort port)
{
    return existing_inputs()[port];
}

WireOutput* MachineRocket::request_output(WireOutPort port)
{
    return existing_outputs()[port];
}

std::vector<WireInput*> MachineRocket::existing_inputs()
{
    return {&m_wiGimbal, &m_wiIgnition, &m_wiThrottle};
}

std::vector<WireOutput*> MachineRocket::existing_outputs()
{
    return {};
}

SysMachineRocket::SysMachineRocket(ActiveScene &scene) :
    SysMachine<SysMachineRocket, MachineRocket>(scene),
    m_updatePhysics(scene.get_update_order(), "mach_rocket", "wire", "physics",
                    std::bind(&SysMachineRocket::update_physics, this))
{

}

//void SysMachineRocket::update_sensor()
//{
//}

void SysMachineRocket::update_physics()
{
    auto view = m_scene.get_registry().view<MachineRocket>();

    for (ActiveEnt ent : view)
    {
        auto &machine = view.get<MachineRocket>(ent);

        //if (!machine.m_enable)
        //{
        //    continue;
        //}

        //std::cout << "updating a rocket\n";

        ACompRigidBody_t *compRb;
        ACompTransform *compTf;

        if (m_scene.get_registry().valid(machine.m_rigidBody))
        {
            // Try to get the ACompRigidBody if valid
            compRb = m_scene.get_registry()
                            .try_get<ACompRigidBody_t>(machine.m_rigidBody);
            compTf = m_scene.get_registry()
                            .try_get<ACompTransform>(machine.m_rigidBody);
            if (!compRb || !compTf)
            {
                machine.m_rigidBody = entt::null;
                continue;
            }
        }
        else
        {
            // rocket's rigid body not set yet
            auto const& [bodyEnt, pBody]
                    = SysPhysics_t::find_rigidbody_ancestor(m_scene, ent);

            if (pBody == nullptr)
            {
                std::cout << "no rigid body!\n";
                continue;
            }

            machine.m_rigidBody = bodyEnt;
            compRb = pBody;
            compTf = m_scene.get_registry().try_get<ACompTransform>(bodyEnt);
        }

        if (WireData *ignition = machine.m_wiIgnition.connected_value())
        {

        }

        using wiretype::Percent;

        if (WireData *throttle = machine.m_wiThrottle.connected_value())
        {
            Percent *percent = std::get_if<Percent>(throttle);

            float thrust = 10.0f; // temporary

            //std::cout << percent->m_value << "\n";

            Vector3 thrustVec = compTf->m_transform.backward()
                                    * (percent->m_value * thrust);

            SysPhysics_t::body_apply_force(*compRb, thrustVec);
        }



        // this is suppose to be gimbal, but for now it applies torque

        using wiretype::AttitudeControl;

        if (WireData *gimbal = machine.m_wiGimbal.connected_value())
        {
            AttitudeControl *attCtrl = std::get_if<AttitudeControl>(gimbal);

            Vector3 localTorque = compTf->m_transform
                    .transformVector(attCtrl->m_attitude);

            localTorque *= 3.0f; // arbitrary

            SysPhysics_t::body_apply_torque(*compRb, localTorque);
        }


    }
}

void SysMachineRocket::attach_plume_effect(ActiveEnt ent)
{
    using Magnum::GL::Mesh;
    using Magnum::GL::Texture2D;
    using Magnum::Trade::ImageData2D;
    using namespace Magnum::Math::Literals;

    ActiveEnt plumeNode = entt::null;

    auto findPlumeHandle = [this, &plumeNode](ActiveEnt ent)
    {
        auto const& node = m_scene.reg_get<ACompHierarchy>(ent);
        static constexpr std::string_view nodePrefix = "fx_plume_";
        if (0 == node.m_name.compare(0, nodePrefix.size(), nodePrefix))
        {
            plumeNode = ent;
            return EHierarchyTraverseStatus::Stop;  // terminate search
        }
        return EHierarchyTraverseStatus::Continue;
    };

    m_scene.hierarchy_traverse(ent, findPlumeHandle);

    if (plumeNode == entt::null)
    {
        std::cout << "ERROR: could not find plume anchor for MachineRocket "
            << ent << "\n";
        return;
    }
    std::cout << "MachineRocket " << ent << "'s associated plume: "
        << plumeNode << "\n";

    // Get plume effect
    Package& pkg = m_scene.get_application().debug_find_package("lzdb");
    std::string_view plumeAnchorName = m_scene.reg_get<ACompHierarchy>(plumeNode).m_name;
    std::string_view effectName = plumeAnchorName.substr(3, plumeAnchorName.length() - 3);
    DependRes<PlumeEffectData> plumeEffect = pkg.get<PlumeEffectData>(effectName);
    if (plumeEffect.empty())
    {
        using Magnum::Trade::MeshData;

        DependRes<MeshData> meshData = pkg.get<MeshData>(plumeEffect->meshName);
        if (meshData.empty())
        {
            std::cout << "ERROR: couldn't find mesh " << plumeEffect->meshName
                << " for plume effect\n";
        }

        plumeMesh = AssetImporter::compile_mesh(meshData, glResources);
    }

    // Get plume tex (TEMPORARY: just grab noise1024.png)
    const std::string texName = "OSPData/adera/noise1024.png";
    DependRes<Texture2D> plumeTex = glResources.get<Texture2D>(texName);
    if (plumeTex.empty())
    {
        using Magnum::Trade::ImageData;

        DependRes<ImageData2D> imageData = pkg.get<ImageData2D>(texName);
        if (imageData.empty())
        {
            std::cout << "ERROR: couldn't find texture " << texName
                << " for plume effect\n";
        }

        plumeTex = AssetImporter::compile_tex(imageData, glResources);
    }

    // by now, the mesh and texture should both exist
    using adera::shader::Phong;
    Phong::ACompPhongInstance shader;
    shader.m_shaderProgram = glResources.get<Phong>("phong_shader");
    shader.m_textures = std::vector<DependRes<Texture2D>>{plumeTex};
    shader.m_lightPosition = Vector3{10.0f, 15.0f, 5.0f};
    shader.m_ambientColor = 0x111111_rgbf;
    shader.m_specularColor = 0x330000_rgbf;
    m_scene.reg_emplace<Phong::ACompPhongInstance>(plumeNode, std::move(shader));
    m_scene.reg_emplace<CompDrawableDebug>(plumeNode, plumeMesh,
        &adera::shader::Phong::draw_entity);
    m_scene.reg_emplace<CompVisibleDebug>(plumeNode, false);
}

Machine& SysMachineRocket::instantiate(ActiveEnt ent)
{
    attach_plume_effect(ent);
    return m_scene.reg_emplace<MachineRocket>(ent);
}

Machine& SysMachineRocket::get(ActiveEnt ent)
{
    return m_scene.reg_get<MachineRocket>(ent);//emplace(ent);
}
