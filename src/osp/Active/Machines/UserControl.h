#pragma once

#include "../SysMachine.h"

namespace osp
{

/**
 * Processes user inputs into usable WireOutputs
 */
class MachineUserControl : public Machine
{
public:
    MachineUserControl();
    MachineUserControl(MachineUserControl&& move) = default;

    ~MachineUserControl() = default;

    //void WireElement::propagate_test();
    void propagate_output(WireOutput* output);

private:
    WireOutput m_woThrottle;
    WireOutput m_woTestPropagate;
    WireInput m_wiTest;

};

using SysMachineUserControl = SysMachine<MachineUserControl>;

}
