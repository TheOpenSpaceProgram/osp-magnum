#pragma once

#include "../../UserInputHandler.h"
#include "../SysMachine.h"

namespace osp
{

class MachineUserControl;


class SysMachineUserControl :
        public SysMachine<SysMachineUserControl, MachineUserControl>
{
public:

    SysMachineUserControl(UserInputHandler& userControl);

    void doUpdate();

private:
    ButtonControlHandle m_throttleMax;
    ButtonControlHandle m_throttleMin;
    ButtonControlHandle m_selfDestruct;
};

/**
 * Processes user inputs into usable WireOutputs
 */
class MachineUserControl : public Machine
{
    friend SysMachineUserControl;

public:
    MachineUserControl();
    MachineUserControl(MachineUserControl&& move);

    ~MachineUserControl() = default;

    //void WireElement::propagate_test();
    void propagate_output(WireOutput* output) override;

    std::vector<WireInput*> available_inputs() override;
    std::vector<WireOutput*> available_outputs() override;

    //constexpr WireOutput& out_throttle() { return m_outputs[0]; }
    //constexpr WireOutput& out_test() { return m_outputs[1]; }

private:

    //std::array<WireOutput, 2> m_outputs;

    WireOutput m_woTestPropagate;
    WireOutput m_woThrottle;
    WireInput m_wiTest;

};


}
