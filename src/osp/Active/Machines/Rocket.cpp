#include <iostream>


#include "../ActiveScene.h"
#include "Rocket.h"

namespace osp
{

MachineRocket::MachineRocket(ActiveEnt &ent) :
        Machine(ent),
        m_wiIgnition(this, "Ignition"),
        m_wiThrottle(this, "Throttle"),
        m_rigidBody(entt::null)
{
    //m_enable = true;
}

MachineRocket::MachineRocket(MachineRocket&& move) :
        Machine(std::move(move)),
        m_wiIgnition(this, std::move(move.m_wiIgnition)),
        m_wiThrottle(this, std::move(move.m_wiThrottle))
{
    //m_enable = true;
}

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
    return {&m_wiIgnition, &m_wiThrottle};
}

std::vector<WireOutput*> MachineRocket::existing_outputs()
{
    return {};
}

SysMachineRocket::SysMachineRocket(ActiveScene &scene) :
    SysMachine<SysMachineRocket, MachineRocket>(scene),
    m_physics(scene.get_system<SysPhysics>())
{

}

void SysMachineRocket::update_sensor()
{
}

void SysMachineRocket::update_physics()
{
    for (MachineRocket& machine : m_machines)
    {
        //if (!machine.m_enable)
        //{
        //    continue;
        //}

        //std::cout << "updating a rocket\n";

        if (WireData *ignition = machine.m_wiIgnition.connected_value())
        {

        }

        using wiretype::Percent;

        if (WireData *throttle = machine.m_wiThrottle.connected_value())
        {
            Percent *percent = std::get_if<Percent>(throttle);
            //std::cout << percent->m_value << "\n";

            CompRigidBody *compRb;
            
            if (m_scene.get_registry().valid(machine.m_rigidBody))
            {
                // Try to get the CompRigidBody if valid
                compRb = m_scene.get_registry().try_get<CompRigidBody>(machine.m_rigidBody);
                if (!compRb)
                {
                    machine.m_rigidBody = entt::null;
                    continue;
                }
            }
            else
            {
                // rocket's rigid body not set yet
                auto body =
                        m_physics.find_rigidbody_ancestor(machine.get_ent());

                if (body.second == nullptr)
                {
                    std::cout << "no rigid body!\b";
                    continue;
                }

                machine.m_rigidBody = body.first;
                compRb = body.second;
            }

            m_physics.body_apply_force(*compRb, Vector3(0, percent->m_value * 10.0f, 0));
        }
    }
}

Machine& SysMachineRocket::instantiate(ActiveEnt ent)
{
    return emplace(ent);
}

}
