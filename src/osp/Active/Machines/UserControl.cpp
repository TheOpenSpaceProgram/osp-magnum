#include <iostream>


#include "UserControl.h"

namespace osp
{

MachineUserControl::MachineUserControl() :
    m_wiTest(this, "Test"),
    //m_woTestPropagate(this, "TestOut", &MachineUserControl::propagate_test),
    m_woTestPropagate(this, "TestOut", m_wiTest),
    m_woThrottle(this, "Throttle")
{
    m_woTestPropagate.propagate();
}

void MachineUserControl::propagate_output(WireOutput* output)
{
    std::cout << "propagate test: " << output->get_name() << "\n";
}

template<>
void SysMachineUserControl::update()
{
    std::cout << "updating all MachineUserControls\n";
}

}
