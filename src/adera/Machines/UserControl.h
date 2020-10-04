#pragma once

#include <osp/UserInputHandler.h>
#include <osp/Active/SysMachine.h>

namespace adera::active::machines
{

class MachineUserControl;

/**
 * Gets ButtonControlHandle from a UserInputHandler, and updates
 * MachineUserControls
 */
class SysMachineUserControl :
        public osp::active::SysMachine<SysMachineUserControl, MachineUserControl>
{
public:

    SysMachineUserControl(osp::active::ActiveScene &scene,
                          osp::UserInputHandler& userControl);

    void update_sensor();

    osp::active::Machine& instantiate(osp::active::ActiveEnt ent) override;

    osp::active::Machine& get(osp::active::ActiveEnt ent) override;

private:
    osp::ButtonControlHandle m_throttleMax;
    osp::ButtonControlHandle m_throttleMin;
    osp::ButtonControlHandle m_selfDestruct;

    osp::ButtonControlHandle m_pitchUp;
    osp::ButtonControlHandle m_pitchDn;
    osp::ButtonControlHandle m_yawLf;
    osp::ButtonControlHandle m_yawRt;
    osp::ButtonControlHandle m_rollLf;
    osp::ButtonControlHandle m_rollRt;

    osp::active::UpdateOrderHandle m_updateSensor;
};

/**
 * Interfaces user input into WireOutputs designed for controlling spacecraft.
 */
class MachineUserControl : public osp::active::Machine
{
    friend SysMachineUserControl;

public:
    MachineUserControl();
    MachineUserControl(MachineUserControl&& move);

    MachineUserControl& operator=(MachineUserControl&& move);


    ~MachineUserControl() = default;

    void propagate_output(osp::active::WireOutput* output) override;

    osp::active::WireInput* request_input(osp::WireInPort port) override;
    osp::active::WireOutput* request_output(osp::WireOutPort port) override;

    std::vector<osp::active::WireInput*> existing_inputs() override;
    std::vector<osp::active::WireOutput*> existing_outputs() override;

private:

    //std::array<WireOutput, 2> m_outputs;

    osp::active::WireOutput m_woAttitude;
    osp::active::WireOutput m_woTestPropagate;
    osp::active::WireOutput m_woThrottle;
    osp::active::WireInput m_wiTest;

};


}
