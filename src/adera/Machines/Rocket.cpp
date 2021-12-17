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
#include "Rocket.h"                      // IWYU pragma: associated

#include <adera/ShipResources.h>         // for ShipResourceType

#include <osp/Active/basic.h>            // for ACompHierarchy, ACompTransform
#include <osp/Active/physics.h>
#include <osp/Active/SysWire.h>          // for ACtxWireNodes, MCompWirePanel
#include <osp/Active/SysSignal.h>        // for Signal<>::NodeState
#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/machines.h>         // for ACompMachines

#include <osp/Resource/Package.h>         // for Path, decompose_path
#include <osp/Resource/PackageRegistry.h> // for PackageRegistry
#include <osp/Resource/Resource.h>        // for DependRes
#include <osp/Resource/machines.h>        // for mach_id, machine_id_t
#include <osp/Resource/blueprints.h>      // for BlueprintMachine
#include <osp/Resource/PrototypePart.h>   // for NodeMap_t, PCompMachine

#include <osp/types.h>                    // for Vector3, Matrix4

#include <vector>                         // for std::vector
#include <utility>                        // for std::forward
#include <variant>                        // for std::get
#include <string_view>                    // for std::string_view

// IWYU pragma: no_include "adera/wiretypes.h"

// IWYU pragma: no_include <map>
// IWYU pragma: no_include <cstddef>
// IWYU pragma: no_include <stdint.h>
// IWYU pragma: no_include <type_traits>

#if 0

namespace osp::active { class SysHierarchy; }

using adera::active::machines::SysMCompRocket;
using adera::wire::Percent;
using adera::active::machines::MCompRocket;

using osp::active::ActiveScene;
using osp::active::ActiveEnt;
using osp::active::ACompHierarchy;
using osp::active::ACompName;
using osp::active::EHierarchyTraverseStatus;
using osp::active::ACompMachines;
using osp::active::ACompTransform;
using osp::active::SysHierarchy;

using osp::active::ACtxPhysics;
using osp::active::ACompPhysNetForce;
using osp::active::ACompPhysNetTorque;
using osp::active::SysPhysics;

using osp::active::MCompWirePanel;
using osp::active::ACtxWireNodes;
using osp::active::SysWire;
using osp::active::WireNode;
using osp::active::WirePort;

using osp::BlueprintMachine;
using osp::BlueprintPart;
using osp::BlueprintVehicle;

using osp::Package;
using osp::DependRes;
using osp::Path;

using osp::machine_id_t;
using osp::NodeMap_t;
using osp::Matrix4;
using osp::Vector3;


void SysMCompRocket::update_construct(ActiveScene& rScene)
{
    auto view = rScene.get_registry()
            .view<osp::active::ACompVehicle,
                  osp::active::ACompVehicleInConstruction>();

    machine_id_t const id = osp::mach_id<MCompRocket>();

    for (auto [vehEnt, rVeh, rVehConstr] : view.each())
    {
        // Check if the vehicle blueprint might store MCompRCSControllers
        if (rVehConstr.m_blueprint->m_machines.size() <= id)
        {
            continue;
        }

        BlueprintVehicle const& vehBp = *rVehConstr.m_blueprint;

        // Initialize all MCompRockets in the vehicle
        for (BlueprintMachine const &mach : vehBp.m_machines[id])
        {
            // Get part
            ActiveEnt partEnt = rVeh.m_parts[mach.m_partIndex];

            // Get machine entity previously reserved by SysVehicle
            auto& machines = rScene.reg_get<ACompMachines>(partEnt);
            ActiveEnt machEnt = machines.m_machines[mach.m_protoMachineIndex];

            BlueprintPart const& partBp = vehBp.m_blueprints[mach.m_partIndex];
            instantiate(rScene, machEnt,
                        vehBp.m_prototypes[partBp.m_protoIndex]
                                ->m_protoMachines[mach.m_protoMachineIndex],
                        mach);
        }
    }
}

void SysMCompRocket::update_calculate(ActiveScene& rScene)
{
    auto view = rScene.get_registry().view<MCompRocket>();
    std::vector<ActiveEnt>& rToUpdate = SysWire::to_update<MCompRocket>(rScene);

    for (ActiveEnt ent : rToUpdate)
    {
        auto &machine = view.get<MCompRocket>(ent);

        machine.m_powerOutput = 0;

        // Get the Percent Panel which contains the Throttle Port
        auto const *pPanelPercent = rScene.get_registry()
                .try_get< MCompWirePanel<Percent> >(ent);

        if (pPanelPercent != nullptr)
        {
            WirePort<Percent> const *pPortThrottle
                    = pPanelPercent->port(MCompRocket::smc_wiThrottle);

            if (pPortThrottle != nullptr)
            {
                // Get the connected node and its value
                auto const &nodesPercent
                        = rScene.reg_get< ACtxWireNodes<Percent> >(
                            rScene.hier_get_root());
                WireNode<Percent> const &nodeThrottle
                        = nodesPercent.get_node(pPortThrottle->m_nodeIndex);
                float throttle = nodeThrottle.m_state.m_percent;
                machine.m_powerOutput = throttle;
            }
        }
    }

    rToUpdate.clear();

}

void SysMCompRocket::update_physics(ActiveScene& rScene)
{
    osp::active::ActiveReg_t &rReg = rScene.get_registry();

    auto view = rScene.get_registry().view<MCompRocket>();

    for (ActiveEnt ent : view)
    {
        auto &machine = view.get<MCompRocket>(ent);

        // Check for nonzero throttle, continue otherwise
        if (0 >= machine.m_powerOutput)
        {
            continue;
        }

        /*
        // Check for adequate resource inputs
        bool fail = false;
        for (auto& resource : machine.m_resourceLines)
        {
            if (auto const* pipe = resource.m_source.get_if<wiretype::Pipe>();
                pipe != nullptr)
            {
                
                if (auto const* src = rScene.reg_try_get<MCompContainer>(pipe->m_source);
                    src != nullptr)
                {
                    uint64_t required = resource_units_required(rScene, machine,
                        pThrotPercent->m_value, resource);
                    if (src->check_contents().m_quantity > required)
                    {
                        continue;
                    }
                }
            }
            fail = true;
            break;
        }
        if (fail)
        {
            continue;
        }

        */

        // Perform physics calculation

        // Get rigidbody ancestor and its transformation component
        auto const* pRbAncestor =
            SysPhysics::try_get_or_find_rigidbody_ancestor(rScene, ent);
        auto const& rCompTf = rScene.reg_get<ACompTransform>(pRbAncestor->m_ancestor);

        Matrix4 relTransform = pRbAncestor->m_relTransform;

        /* Compute thrust force
         * Thrust force is defined to be along +Z by convention.
         * Obtains thrust vector in rigidbody space
         */
        Vector3 thrustDir = relTransform.transformVector(Vector3{0.0f, 0.0f, 1.0f});
        float thrustMag = machine.m_params.m_maxThrust * machine.m_powerOutput;
        // Take thrust in rigidbody space and apply to RB in world space
        Vector3 thrust = thrustMag * thrustDir;
        Vector3 worldThrust = rCompTf.m_transform.transformVector(thrust);
        auto &rNetForce = rReg.get_or_emplace<ACompPhysNetForce>(pRbAncestor->m_ancestor);
        rNetForce += worldThrust;

        // Obtain point where thrust is applied relative to RB CoM
        Vector3 location = relTransform.translation();
        // Compute worldspace torque from engine location, thrust vector
        Vector3 torque = Magnum::Math::cross(location, thrust);
        Vector3 worldTorque = rCompTf.m_transform.transformVector(torque);
        auto &rNetTorque = rReg.get_or_emplace<ACompPhysNetTorque>(pRbAncestor->m_ancestor);
        rNetTorque += worldTorque;

        // Perform resource consumption calculation
        /*
        for (MCompRocket::ResourceInput const& resource : machine.m_resourceLines)
        {
            // Pipe must be non-null since we checked earlier
            const auto& pipe = *resource.m_source.get_if<wiretype::Pipe>();
            auto& src = rScene.reg_get<MCompContainer>(pipe.m_source);

            uint64_t required = resource_units_required(rScene, machine,
                pThrotPercent->m_value, resource);
            uint64_t consumed = src.request_contents(required);

            SPDLOG_LOGGER_TRACE(rScene.get_application().get_logger(),
                                "Consumed {} units of fuel, {} remaining",
                consumed, src.check_contents().m_quantity);
        }*/

        // Set output power level (for plume effect)
        // TODO: later, take into account low fuel pressure, bad mixture, etc.
        //machine.m_powerOutput = pThrotPercent->m_value;
    }

}

// TODO: come up with a better solution to this, and also replace config maps
//       and variants with something else entirely
template<typename TYPE_T>
TYPE_T const& config_get_if(
        NodeMap_t const& nodeMap,
        std::string_view field, TYPE_T&& defaultValue)
{
    auto found = nodeMap.find(field);
    if (found == nodeMap.end())
    {
        return std::forward<TYPE_T>(defaultValue);
    }
    return std::get<TYPE_T>(found->second);
//    if (TYPE_T const* value = std::get_if<TYPE_T>(&found->second); value != nullptr)
//    {
//        return *value;
//    }
//    return defaultValue;
}

MCompRocket& SysMCompRocket::instantiate(
        osp::active::ActiveScene& rScene,
        osp::active::ActiveEnt ent,
        osp::PCompMachine const& config,
        osp::BlueprintMachine const& settings)
{
    auto& rocket = rScene.reg_emplace<MCompRocket>(ent);
    // Read engine config
    // cast to surpress narrowing conversion warnings.
    rocket.m_params.m_maxThrust   = static_cast<float>(config_get_if<double>(config.m_config, "thrust", 42.0));
    rocket.m_params.m_specImpulse = static_cast<float>(config_get_if<double>(config.m_config, "Isp",    42.0));

    std::string const& fuelIdent = config_get_if<std::string>(config.m_config, "fueltype", "");
    Path resPath = osp::decompose_path(fuelIdent);
    Package& pkg = rScene.get_packages().find(resPath.prefix);
    DependRes<ShipResourceType> fuel = pkg.get<ShipResourceType>(resPath.identifier);

    return rocket;
}

#endif
