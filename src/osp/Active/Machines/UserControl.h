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

    SysMachineUserControl(ActiveScene &scene, UserInputHandler& userControl);

    void update_sensor() override;
    void update_physics(float delta) override;

    Machine& instantiate(ActiveEnt ent) override;

private:
    ButtonControlHandle m_throttleMax;
    ButtonControlHandle m_throttleMin;
    ButtonControlHandle m_selfDestruct;
};

/**
 * Interfaces user control into WireOutputs for other Machines to use
 */
class MachineUserControl : public Machine
{
    friend SysMachineUserControl;

public:
    MachineUserControl(ActiveEnt &ent);
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
