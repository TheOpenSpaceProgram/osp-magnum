#include <iostream>


#include "UserControl.h"

namespace osp
{

MachineUserControl::MachineUserControl() :
    //m_woTestPropagate(this, "TestOut", &MachineUserControl::propagate_test),
    m_woTestPropagate(this, "TestOut", m_wiTest),
    m_woThrottle(this, "Throttle"),
    m_wiTest(this, "Test")
{
    m_woTestPropagate.propagate();
}

MachineUserControl::MachineUserControl(MachineUserControl&& move) :
    m_woTestPropagate(this, std::move(move.m_woTestPropagate)),
    m_woThrottle(this, std::move(move.m_woThrottle)),
    m_wiTest(this, std::move(move.m_wiTest))
{

}

void MachineUserControl::propagate_output(WireOutput* output)
{
    std::cout << "propagate test: " << output->get_name() << "\n";
}

WireInput* MachineUserControl::request_input(WireInPort port)
{
    return existing_inputs()[port];
}

WireOutput* MachineUserControl::request_output(WireOutPort port)
{
    return existing_outputs()[port];
}

std::vector<WireInput*> MachineUserControl::existing_inputs()
{
    return {&m_wiTest};
}

std::vector<WireOutput*> MachineUserControl::existing_outputs()
{
    return {&m_woThrottle, &m_woTestPropagate};
}

SysMachineUserControl::SysMachineUserControl(UserInputHandler& userControl) :
    m_throttleMax(userControl.config_get("game_thr_max")),
    m_throttleMin(userControl.config_get("game_thr_min")),
    m_selfDestruct(userControl.config_get("game_self_destruct"))
{

}

void SysMachineUserControl::update_sensor()
{
    //std::cout << "updating all MachineUserControls\n";
    // InputDevice.IsActivated()
    // Combination
    
    if (m_throttleMin.triggered())
    {
        std::cout << "throttle min\n";
    }

    if (m_throttleMax.triggered())
    {
        std::cout << "throttle max\n";
    }

    if (m_selfDestruct.triggered())
    {
        std::cout << "EXPLOSION BOOM!!!!\n";
    }


    for (MachineUserControl& machine : m_machines)
    {
        std::cout << "updating a MachineUserControl" << "\n";
    }
}

void SysMachineUserControl::update_physics()
{

}

Machine& SysMachineUserControl::instantiate()
{
    return emplace();
}

}
