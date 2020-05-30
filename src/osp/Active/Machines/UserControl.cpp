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

std::vector<WireInput*> MachineUserControl::available_inputs()
{
    return {&m_wiTest};
}

std::vector<WireOutput*> MachineUserControl::available_outputs()
{
    return {&m_woThrottle, &m_woTestPropagate};
}

SysMachineUserControl::SysMachineUserControl(UserInputHandler& userControl) :
    m_throttleMax(userControl.config_get("game_thr_max")),
    m_throttleMin(userControl.config_get("game_thr_min"))
{

}

void SysMachineUserControl::doUpdate()
{
    std::cout << "updating all MachineUserControls\n";
    // InputDevice.IsActivated()
    // Combination
    
    
    for (MachineUserControl& machine : m_machines)
    {
        
    }
}

}
