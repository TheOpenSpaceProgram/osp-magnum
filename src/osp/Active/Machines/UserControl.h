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

    void propagate_output(WireOutput* output) override;

    WireInput* request_input(WireInPort port) override;
    WireOutput* request_output(WireOutPort port) override;

    std::vector<WireInput*> existing_inputs() override;
    std::vector<WireOutput*> existing_outputs() override;

private:

    //std::array<WireOutput, 2> m_outputs;

    WireOutput m_woTestPropagate;
    WireOutput m_woThrottle;
    WireInput m_wiTest;

};


}
