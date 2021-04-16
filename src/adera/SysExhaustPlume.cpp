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
#include "SysExhaustPlume.h"
#include "osp/Active/ActiveScene.h"
#include "adera/Machines/Rocket.h"
#include "osp/Resource/AssetImporter.h"
#include <osp/Active/SysRender.h>

#include <Magnum/Trade/MeshData.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/Sampler.h>
#include <Magnum/GL/TextureFormat.h>

using namespace osp::active;

void SysExhaustPlume::add_functions(ActiveScene& rScene)
{
    rScene.debug_update_add(rScene.get_update_order(), "exhaust_plume", "mach_rocket", "",
                            &SysExhaustPlume::update_plumes);

}

void SysExhaustPlume::initialize_plume(ActiveScene& rScene, ActiveEnt node)
{
    using Magnum::GL::Mesh;
    using Magnum::GL::Texture2D;
    using Magnum::Trade::ImageData2D;
    using namespace Magnum::Math::Literals;
    using adera::shader::PlumeShader;

    auto const& plume = rScene.reg_get<ACompExhaustPlume>(node);

    if (rScene.get_registry().all_of<ACompShader>(node))
    {
        return;  // plume already initialized
    }

    DependRes<PlumeEffectData> plumeEffect = plume.m_effect;

    Package& pkg = rScene.get_application().debug_find_package("lzdb");

    // Get plume mesh
    Package& glResources = rScene.get_context_resources();
    DependRes<Mesh> plumeMesh = glResources.get<Mesh>(plumeEffect->m_meshName);
    if (plumeMesh.empty())
    {
        plumeMesh = AssetImporter::compile_mesh(plumeEffect->m_meshName, pkg, glResources);
    }

    // Emplace plume shader instance render data
    rScene.reg_emplace<ACompMesh>(node, plumeMesh);
    rScene.reg_emplace<ACompVisible>(node);
    rScene.reg_emplace<ACompTransparent>(node);
    rScene.reg_emplace<ACompShader>(node, &adera::shader::PlumeShader::draw_plume);
}

// TODO: workaround. add an actual way to keep time accessible from ActiveScene
float g_time = 0;

void SysExhaustPlume::update_plumes(ActiveScene& rScene)
{
    g_time += rScene.get_time_delta_fixed();

    using adera::active::machines::MachineRocket;

    auto& reg = rScene.get_registry();

    // Initialize any uninitialized plumes
    auto uninitializedComps =
        reg.view<ACompExhaustPlume>(entt::exclude<ACompShader>);
    for (ActiveEnt plumeEnt : uninitializedComps)
    {
        initialize_plume(rScene, plumeEnt);
    }

    // Process plumes
    auto plumeView = reg.view<ACompExhaustPlume>();
    for (ActiveEnt plumeEnt : plumeView)
    {
        auto& plume = plumeView.get<ACompExhaustPlume>(plumeEnt);
        plume.m_time = g_time;

        auto& machine = rScene.reg_get<MachineRocket>(plume.m_parentMachineRocket);
        float powerLevel = machine.current_output_power();

        if (powerLevel > 0.0f)
        {
            plume.m_powerLevel = powerLevel;
            rScene.get_registry().emplace_or_replace<ACompVisible>(plumeEnt);
        }
        else
        {
            rScene.get_registry().remove_if_exists<ACompVisible>(plumeEnt);
        }
    }
}
