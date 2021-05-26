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

#include "RCSController.h"

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/physics.h>

#include <osp/Resource/machines.h>

#include <Magnum/Math/Vector.h>
#include <functional>

using namespace adera::active::machines;
using namespace osp::active;
using namespace osp;
using Magnum::Vector3;
using Magnum::Matrix4;

using osp::active::wiretype::AttitudeControl;
using osp::active::wiretype::Percent;

void SysMachineRCSController::add_functions(ActiveScene& rScene)
{
    rScene.debug_update_add(rScene.get_update_order(), "mach_rcs_construct", "vehicle_activate", "vehicle_modification",
                            &SysMachineRCSController::update_construct);
}

float SysMachineRCSController::thruster_influence(Vector3 posOset, Vector3 direction,
    Vector3 cmdTransl, Vector3 cmdRot)
{
    using Magnum::Math::cross;
    using Magnum::Math::dot;

    // Thrust is applied in opposite direction of thruster nozzle direction
    Vector3 thrust = std::negate{}(direction.normalized());

    float rotInfluence = 0.0f;
    if (cmdRot.length() > 0.0f)
    {
        Vector3 torque = cross(posOset, thrust).normalized();
        rotInfluence = dot(torque, cmdRot.normalized());
    }

    float translInfluence = 0.0f;
    if (cmdTransl.length() > 0.0f)
    {
        translInfluence = dot(thrust, cmdTransl.normalized());
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

    machine_id_t const id = mach_id<MachineRCSController>();

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
    ACompWireNodes<Percent> &rNodesPercent = SysWire::nodes<Percent>(rScene);
    std::vector<ActiveEnt>& rToUpdate
            = SysWire::to_update<MachineRCSController>(rScene);

    std::vector< nodeindex_t<wiretype::Percent> > updPercent;

    auto view = rScene.get_registry().view<MachineRCSController>();

    for (ActiveEnt ent : rToUpdate)
    {

        float influence = 0.0f;

        auto const *panelAtCtrl = rScene.get_registry()
                .try_get< ACompWirePanel<AttitudeControl> >(ent);

        if (panelAtCtrl != nullptr)
        {
            WirePort<AttitudeControl> const *portCommand
                    = panelAtCtrl->port(MachineRCSController::smc_wiCommandOrient);

            // Read AttitudeControl Command Input
            if (portCommand != nullptr)
            {
                auto const &nodesAttCtrl
                        = rScene.reg_get< ACompWireNodes<AttitudeControl> >(
                            rScene.hier_get_root());
                WireNode<AttitudeControl> const &nodeCommand
                        = nodesAttCtrl.get_node(portCommand->m_nodeIndex);

                // Get rigidbody ancestor and its transformation component
                auto const *rbAncestor
                        = SysPhysics_t::try_get_or_find_rigidbody_ancestor(
                            rScene, ent);
                auto const &compRb = rScene.reg_get<ACompRigidBody_t>(
                            rbAncestor->m_ancestor);
                auto const &compTf = rScene.reg_get<ACompTransform>(
                            rbAncestor->m_ancestor);

                Matrix4 transform = rbAncestor->m_relTransform;

                // TODO: RCS translation is not currently implemented, only
                // rotation
                Vector3 commandTransl = Vector3{0.0f};
                Vector3 commandRot = nodeCommand.m_state.m_value.m_attitude;
                Vector3 thrusterPos = transform.translation()
                                    - compRb.m_centerOfMassOffset;
                Vector3 thrusterDir = transform.rotation()
                                    * Vector3{0.0f, 0.0f, 1.0f};

                if (commandRot.length() > 0.0f || commandTransl.length() > 0.0f)
                {
                    influence = thruster_influence(thrusterPos, thrusterDir,
                                                   commandTransl, commandRot);
                }
            }
        }

        auto const *panelPercent = rScene.get_registry()
                .try_get< ACompWirePanel<Percent> >(ent);

        if (panelPercent != nullptr)
        {
            WirePort<Percent> const *portThrottle
                    = panelPercent->port(MachineRCSController::m_woThrottle);

            // Write to throttle output
            if (portThrottle != nullptr)
            {
                WireNode<Percent> &nodeThrottle
                        = rNodesPercent.get_node(portThrottle->m_nodeIndex);

                // Write possibly new throttle value to node
                SysSignal<Percent>::signal_assign(
                        rScene, {influence}, nodeThrottle,
                        portThrottle->m_nodeIndex, updPercent);
            }
        }
    }

    // Request to update any wire nodes if they were modified
    if (!updPercent.empty())
    {
        rNodesPercent.update_request(std::move(updPercent));
        rScene.reg_get<ACompWire>(rScene.hier_get_root()).request_update();
    }
}
