#pragma once

#include <osp/UserInputHandler.h>
#include <osp/Active/SysMachine.h>

namespace osp
{

class MachineUserControl;


class SysMachineUserControl :
        public SysMachine<SysMachineUserControl, MachineUserControl>
{
public:

    SysMachineUserControl(ActiveScene &scene, UserInputHandler& userControl);

    void update_sensor();

    Machine& instantiate(ActiveEnt ent) override;

    Machine& get(ActiveEnt ent) override;

private:
    ButtonControlHandle m_throttleMax;
    ButtonControlHandle m_throttleMin;
    ButtonControlHandle m_selfDestruct;

    ButtonControlHandle m_pitchUp;
    ButtonControlHandle m_pitchDn;
    ButtonControlHandle m_yawLf;
    ButtonControlHandle m_yawRt;
    ButtonControlHandle m_rollLf;
    ButtonControlHandle m_rollRt;

    UpdateOrderHandle m_updateSensor;
};

/**
 * Interfaces user control into WireOutputs for other Machines to use
 */
class MachineUserControl : public Machine
{
    friend SysMachineUserControl;

public:
    MachineUserControl();
    MachineUserControl(MachineUserControl&& move);

    MachineUserControl& operator=(MachineUserControl&& move);


    ~MachineUserControl() = default;

    void propagate_output(WireOutput* output) override;

    WireInput* request_input(WireInPort port) override;
    WireOutput* request_output(WireOutPort port) override;

    std::vector<WireInput*> existing_inputs() override;
    std::vector<WireOutput*> existing_outputs() override;

private:

    //std::array<WireOutput, 2> m_outputs;

    WireOutput m_woAttitude;
    WireOutput m_woTestPropagate;
    WireOutput m_woThrottle;
    WireInput m_wiTest;

};


}
