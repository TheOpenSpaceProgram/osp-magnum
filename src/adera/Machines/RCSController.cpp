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


void SysMachineRCSController::add_functions(ActiveScene& rScene)
{
    //rScene.debug_update_add(rScene.get_update_order(), "mach_rcs", "wire", "controls",
    //                        &SysMachineRCSController::update_controls);
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
            rScene.reg_emplace<ACompMachineType>(machEnt, id);
        }
    }
}

#if 0
void SysMachineRCSController::update_controls(ActiveScene& rScene)
{
    auto view = rScene.get_registry().view<MachineRCSController>();


    for (ActiveEnt ent : view)
    {
        auto& machine = view.get<MachineRCSController>(ent);
        if (!machine.m_enable)
        {
            continue;
        }

        using wiretype::AttitudeControl;
        using Magnum::Vector3;

        // Get rigidbody ancestor and its transformation component
        auto const* pRbAncestor =
            SysPhysics_t::try_get_or_find_rigidbody_ancestor(rScene, ent);
        auto const& rCompRb = rScene.reg_get<ACompRigidBody_t>(pRbAncestor->m_ancestor);
        auto const& rCompTf = rScene.reg_get<ACompTransform>(pRbAncestor->m_ancestor);

        if (WireData *gimbal = machine.m_wiCommandOrient.connected_value())
        {
            AttitudeControl *attCtrl = std::get_if<AttitudeControl>(gimbal);

            Matrix4 transform = pRbAncestor->m_relTransform;

            // TODO: RCS translation is not currently implemented, only rotation
            Vector3 commandTransl = Vector3{0.0f};
            Vector3 commandRot = attCtrl->m_attitude;
            Vector3 thrusterPos = transform.translation() - rCompRb.m_centerOfMassOffset;
            Vector3 thrusterDir = transform.rotation() * Vector3{0.0f, 0.0f, 1.0f};

            float influence = 0.0f;
            if (commandRot.length() > 0.0f || commandTransl.length() > 0.0f)
            {
                influence =
                    thruster_influence(thrusterPos, thrusterDir, commandTransl, commandRot);
            }
            std::get<wiretype::Percent>(machine.m_woThrottle.value()).m_value = influence;
        }
    }

}
#endif
