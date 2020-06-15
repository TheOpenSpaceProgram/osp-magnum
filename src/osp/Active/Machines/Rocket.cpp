#include <iostream>


#include "Rocket.h"

namespace osp
{

MachineRocket::MachineRocket() :
        m_wiIgnition(this, "Ignition"),
        m_wiThrottle(this, "Throttle")
{
    m_enable = true;
}

MachineRocket::MachineRocket(MachineRocket&& move) :
        m_wiIgnition(this, std::move(move.m_wiIgnition)),
        m_wiThrottle(this, std::move(move.m_wiThrottle)),
        Machine(std::move(move))
{
    m_enable = true;
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

void SysMachineRocket::update_sensor()
{
}

void SysMachineRocket::update_physics()
{
    for (MachineRocket& machine : m_machines)
    {
        if (!machine.m_enable)
        {
            continue;
        }

        //std::cout << "updating a rocket\n";

        if (WireData *ignition = machine.m_wiIgnition.connected_value())
        {

        }

        using wiretype::Percent;

        if (WireData *throttle = machine.m_wiThrottle.connected_value())
        {
            Percent *percent = std::get_if<Percent>(throttle);
            //std::cout << percent->m_value << "\n";
        }
    }
}

Machine& SysMachineRocket::instantiate()
{
    return emplace();
}

}
