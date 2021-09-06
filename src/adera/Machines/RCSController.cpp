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

#include "RCSController.h"               // IWYU pragma: associated

#include <osp/Active/scene.h>            // for ACompTransform
#include <osp/Active/SysWire.h>          // for ACompWire, ACompWireNodes
#include <osp/Active/physics.h>
#include <osp/Active/SysSignal.h>        // for Signal<>::NodeState, SysSignal
#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/SysMachine.h>       // for ACompMachines
#include <osp/Active/ActiveScene.h>
#include <osp/Active/activetypes.h>      // for ActiveEnt, ActiveReg_t

#include <osp/Resource/machines.h>
#include <osp/Resource/Resource.h>       // for DependRes
#include <osp/Resource/blueprints.h>     // for BlueprintMachine, BlueprintV...
#include <osp/Resource/PrototypePart.h>  // for NodeMap_t

#include <osp/types.h>                   // for Vector3, Matrix4

#include <vector>                        // for std::vector
#include <iterator>                      // for std::begin, std::end
#include <algorithm>                     // for std::clamp, std::sort
#include <functional>                    // for std::negate

// IWYU pragma: no_include "adera/wiretypes.h"

// IWYU pragma: no_include <cstddef>
// IWYU pragma: no_include <stdint.h>
// IWYU pragma: no_include <type_traits>

namespace osp { class Package; }
namespace osp { struct Path; }

using osp::active::ActiveScene;
using osp::active::ActiveEnt;
using osp::active::ACompMachines;
using osp::active::ACompPhysDynamic;
using osp::active::ACompTransform;
using osp::active::SysPhysics;

using osp::active::ACompWirePanel;
using osp::active::ACompWireNodes;
using osp::active::ACompWire;
using osp::active::SysWire;
using osp::active::SysSignal;
using osp::active::WireNode;
using osp::active::WirePort;
using osp::active::UpdNodes_t;
using osp::nodeindex_t;

using osp::BlueprintMachine;

using osp::Package;
using osp::DependRes;
using osp::Path;

using osp::machine_id_t;
using osp::NodeMap_t;
using osp::Matrix4;
using osp::Vector3;

namespace adera::active::machines
{

float SysMachineRCSController::thruster_influence(Vector3 posOset, Vector3 direction,
    Vector3 cmdTransl, Vector3 cmdRot)
{
    // Thrust is applied in opposite direction of thruster nozzle direction
    Vector3 thrust = std::negate{}(direction.normalized());

    float rotInfluence = 0.0f;
    if (cmdRot.length() > 0.0f)
    {
        Vector3 torque = Magnum::Math::cross(posOset, thrust).normalized();
        rotInfluence   = Magnum::Math::dot(torque, cmdRot.normalized());
    }

    float translInfluence = 0.0f;
    if (cmdTransl.length() > 0.0f)
    {
        translInfluence = Magnum::Math::dot(thrust, cmdTransl.normalized());
    }

    // Total component of thrust in direction of command
    float total = rotInfluence + translInfluence;

    if (total < .01f)
    {
        /* Ignore small contributions from imprecision
         * Real thrusters can't throttle this deep anyways, so if their
         * contribution is this small, it'd be a waste of fuel to fire them.
         */
        return 0.0f;
    }
    
    /* Compute thruster throttle output demanded by current command.
     * In the future, it would be neat to implement PWM so that unthrottlable
     * thrusters would pulse on and off in order to deliver reduced thrust
     */
    return std::clamp(rotInfluence + translInfluence, 0.0f, 1.0f);
}

void SysMachineRCSController::update_construct(ActiveScene &rScene)
{
    auto view = rScene.get_registry()
            .view<osp::active::ACompVehicle,
                  osp::active::ACompVehicleInConstruction>();

    machine_id_t const id = osp::mach_id<MachineRCSController>();

    for (auto [vehEnt, rVeh, rVehConstr] : view.each())
    {
        // Check if the vehicle blueprint might store MachineRCSControllers
        if (rVehConstr.m_blueprint->m_machines.size() <= id)
        {
            continue;
        }

        // Initialize all MachineRCSControllers in the vehicle
        for (BlueprintMachine &mach : rVehConstr.m_blueprint->m_machines[id])
        {
            // Get part
            ActiveEnt partEnt = rVeh.m_parts[mach.m_partIndex];

            // Get machine entity previously reserved by SysVehicle
            auto& machines = rScene.reg_get<ACompMachines>(partEnt);
            ActiveEnt machEnt = machines.m_machines[mach.m_protoMachineIndex];

            rScene.reg_emplace<MachineRCSController>(machEnt);
        }
    }
}


void SysMachineRCSController::update_calculate(ActiveScene& rScene)
{
    using adera::wire::Percent;
    using adera::wire::AttitudeControl;

    ACompWireNodes<Percent> &rNodesPercent = SysWire::nodes<Percent>(rScene);
    std::vector<ActiveEnt>& rToUpdate
            = SysWire::to_update<MachineRCSController>(rScene);

    UpdNodes_t<Percent> updPercent;

    for (ActiveEnt ent : rToUpdate)
    {
        float influence = 0.0f;

        auto const *pPanelAtCtrl = rScene.get_registry()
                .try_get< ACompWirePanel<AttitudeControl> >(ent);

        if (pPanelAtCtrl != nullptr)
        {
            WirePort<AttitudeControl> const *pPortCommand
                    = pPanelAtCtrl->port(MachineRCSController::smc_wiCommandOrient);

            // Read AttitudeControl Command Input
            if (pPortCommand != nullptr)
            {
                auto const &nodesAttCtrl
                        = rScene.reg_get< ACompWireNodes<AttitudeControl> >(
                            rScene.hier_get_root());
                WireNode<AttitudeControl> const &nodeCommand
                        = nodesAttCtrl.get_node(pPortCommand->m_nodeIndex);

                // Get rigidbody ancestor and its transformation component
                auto const *pRbAncestor
                        = SysPhysics::try_get_or_find_rigidbody_ancestor(
                            rScene, ent);
                auto const &rDyn = rScene.reg_get<ACompPhysDynamic>(
                            pRbAncestor->m_ancestor);

                Matrix4 transform = pRbAncestor->m_relTransform;

                // TODO: RCS translation is not currently implemented, only
                // rotation
                Vector3 commandTransl = Vector3{0.0f};
                Vector3 commandRot = nodeCommand.m_state.m_attitude;
                Vector3 thrusterPos = transform.translation()
                                    - rDyn.m_centerOfMassOffset;
                Vector3 thrusterDir = transform.rotation()
                                    * Vector3{0.0f, 0.0f, 1.0f};

                if ((commandRot.length() > 0.0f)
                        || (commandTransl.length() > 0.0f))
                {
                    influence = thruster_influence(thrusterPos, thrusterDir,
                                                   commandTransl, commandRot);
                }
            }
        }

        auto const *pPanelPercent = rScene.get_registry()
                .try_get< ACompWirePanel<Percent> >(ent);

        if (pPanelPercent != nullptr)
        {
            WirePort<Percent> const *pPortThrottle
                    = pPanelPercent->port(MachineRCSController::m_woThrottle);

            // Write to throttle output
            if (pPortThrottle != nullptr)
            {
                WireNode<Percent> const &nodeThrottle
                        = rNodesPercent.get_node(pPortThrottle->m_nodeIndex);

                // Write possibly new throttle value to node
                SysSignal<Percent>::signal_assign(
                        rScene, {influence}, nodeThrottle,
                        pPortThrottle->m_nodeIndex, updPercent);
            }
        }
    }

    // Request to update any wire nodes if they were modified
    if (!updPercent.empty())
    {
        std::sort(std::begin(updPercent), std::end(updPercent));
        rNodesPercent.update_write(updPercent);
        rScene.reg_get<ACompWire>(rScene.hier_get_root()).request_update();
    }
}

} // namespace adera::active::machines
