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
#include "SysExhaustPlume.h"              // IWYU pragma: associated

#if 0

#include <adera/Plume.h>                  // for PlumeEffectData
#include <adera/Machines/Rocket.h>        // for MCompRocket

#include <osp/Resource/Package.h>         // for Package
#include <osp/Resource/machines.h>        // for mach_id, machine_id_t
#include <osp/Resource/Resource.h>        // for DependRes
#include <osp/Resource/blueprints.h>      // for BlueprintMachine, BlueprintV...
#include <osp/Resource/AssetImporter.h>   // for AssetImporter

#include <osp/Active/basic.h>             // for ACompName
#include <osp/Active/drawing.h>           // for ACompShader, ACompVisible
#include <osp/Active/SysRender.h>         // for ACompMesh
#include <osp/Active/machines.h>          // for ACompMachines
#include <osp/Active/SysVehicle.h>        // for ACompVehicle
#include <osp/Active/activetypes.h>       // for entt::basic_view, entt::basic_sparse_set
#include <osp/Active/SysHierarchy.h>      // for osp::active::SysHierarchy
#include <osp/logging.h>                  // for OSP_LOG_...

#include <osp/Resource/PackageRegistry.h> // for PackageRegistry

#include <Magnum/GL/Mesh.h>               // for Mesh

#include <entt/entity/entity.hpp>         // for entt::null_t, entt::null


#include <string_view>                    // for string_view

// IWYU pragma: no_include <vector>
// IWYU pragma: no_include <string>
// IWYU pragma: no_include <cstddef>
// IWYU pragma: no_include <stdint.h>
// IWYU pragma: no_include <type_traits>

using namespace adera::active;

using osp::active::ActiveScene;
using osp::active::ActiveEnt;

/**
 * Attach a visual exhaust plume effect to MCompRocket
 *
 * Searches the hierarchy under the specified MCompRocket entity and
 * attaches an ACompExhaustPlume to the rocket's plume node. A graphical
 * exhaust plume effect will be attached to the node by SysExhaustPlume
 * when it processes the component.
 *
 * @param rScene [ref] Scene containing the following entities
 * @param part [in] Entity containing a plume in its descendents
 * @param mach [in] Entity containing MCompRocket
 */
void attach_plume_effect(ActiveScene &rScene, ActiveEnt part, ActiveEnt mach)
{
    using osp::active::ACompName;
    using osp::active::EHierarchyTraverseStatus;
    using osp::active::SysHierarchy;

    ActiveEnt plumeNode = entt::null;

    auto findPlumeHandle = [&rScene, &plumeNode](ActiveEnt ent)
    {
        static constexpr std::string_view namePrefix = "fx_plume_";
        auto const* name = rScene.reg_try_get<ACompName>(ent);

        if (nullptr == name)
        {
            return EHierarchyTraverseStatus::Continue;
        }

        if (0 == name->m_name.compare(0, namePrefix.size(), namePrefix))
        {
            plumeNode = ent;
            return EHierarchyTraverseStatus::Stop;  // terminate search
        }
        return EHierarchyTraverseStatus::Continue;
    };

    SysHierarchy::traverse(rScene, part, findPlumeHandle);

    if (plumeNode == entt::null)
    {
        OSP_LOG_ERROR(
                "ERROR: could not find plume anchor for MCompRocket {}",
                part);
        return;
    }

    OSP_LOG_INFO(
            "MCompRocket {}'s associated plume: {}",
            part, plumeNode);

    // Get plume effect

    using osp::DependRes;
    using osp::Package;

    Package &rPkg = rScene.get_packages().find("lzdb");

    std::string_view effectName = rScene.reg_get<ACompName>(plumeNode).m_name;
    effectName.remove_prefix(3); // removes "fx_"

    DependRes<PlumeEffectData> plumeEffect = rPkg.get<PlumeEffectData>(effectName);
    if (plumeEffect.empty())
    {
        OSP_LOG_ERROR("ERROR: couldn't find plume effect  {}", effectName);
        return;
    }

    rScene.reg_emplace<ACompExhaustPlume>(plumeNode, mach, plumeEffect);

    // Add Plume mesh

    using osp::AssetImporter;
    using Magnum::GL::Mesh;

    using osp::active::ACompMesh;
    using osp::active::ACompTransparent;
    using osp::active::ACompVisible;

    Package &rGlResources = rScene.get_context_resources();
    DependRes<Mesh> plumeMesh = rGlResources.get<Mesh>(plumeEffect->m_meshName);

    // Compile the plume mesh if not done so yet
    if (plumeMesh.empty())
    {
        plumeMesh = AssetImporter::compile_mesh(
                        plumeEffect->m_meshName, rPkg, rGlResources);
    }

    rScene.reg_emplace<ACompMesh>(plumeNode, plumeMesh);
    rScene.reg_emplace<ACompVisible>(plumeNode);
    rScene.reg_emplace<ACompTransparent>(plumeNode);
    rScene.get_registry().ctx<osp::active::ACtxRenderGroups>().add<MaterialPlume>(plumeNode);
}

void SysExhaustPlume::update_construct(ActiveScene& rScene) noexcept
{
    // TODO: this is kind of a hacky function, Plumes should be made into
    //       their own Machines

    auto view = rScene.get_registry()
            .view<osp::active::ACompVehicle,
                  osp::active::ACompVehicleInConstruction>();

    osp::machine_id_t const id = osp::mach_id<machines::MCompRocket>();

    for (auto const& [vehEnt, rVeh, rVehConstr] : view.each())
    {
        // Check if the vehicle blueprint might store MCompRockets
        if (rVehConstr.m_blueprint->m_machines.size() <= id)
        {
            continue;
        }

        osp::BlueprintVehicle const& vehBp = *rVehConstr.m_blueprint;

        // Get all MCompRockets in the vehicle
        for (osp::BlueprintMachine const &mach : vehBp.m_machines[id])
        {
            // Get part
            ActiveEnt const partEnt = rVeh.m_parts[mach.m_partIndex];

            // Get machine entity previously reserved by SysVehicle
            auto const& machines = rScene.reg_get<osp::active::ACompMachines>(partEnt);
            ActiveEnt const machEnt = machines.m_machines[mach.m_protoMachineIndex];

            attach_plume_effect(rScene, partEnt, machEnt);
        }
    }
}

// TODO: workaround. add an actual way to keep time accessible from ActiveScene
float g_time = 0;

void SysExhaustPlume::update_plumes(ActiveScene& rScene) noexcept
{
    g_time += rScene.get_time_delta_fixed();

    using adera::active::machines::MCompRocket;
    using osp::active::ACompVisible;

    osp::active::ActiveReg_t& reg = rScene.get_registry();

    // Process plumes
    auto plumeView = reg.view<ACompExhaustPlume>();
    for (ActiveEnt plumeEnt : plumeView)
    {
        auto& plume = plumeView.get<ACompExhaustPlume>(plumeEnt);
        plume.m_time = g_time;

        auto& machine = rScene.reg_get<MCompRocket>(plume.m_parentMCompRocket);
        float powerLevel = machine.current_output_power();

        if (powerLevel > 0.0f)
        {
            plume.m_powerLevel = powerLevel;
            rScene.get_registry().emplace_or_replace<ACompVisible>(plumeEnt);
        }
        else
        {
            rScene.get_registry().remove<ACompVisible>(plumeEnt);
        }
    }
}

#endif
