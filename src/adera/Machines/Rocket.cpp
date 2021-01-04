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


#include "Rocket.h"

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

Machine& SysMachineRocket::instantiate(ActiveEnt ent)
{
    return m_scene.reg_emplace<MachineRocket>(ent);//emplace(ent);
}

Machine& SysMachineRocket::get(ActiveEnt ent)
{
    return m_scene.reg_get<MachineRocket>(ent);//emplace(ent);
}
