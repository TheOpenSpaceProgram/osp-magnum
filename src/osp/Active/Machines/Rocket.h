#pragma once

#include "../SysMachine.h"

namespace osp
{

class MachineRocket;


class SysMachineRocket :
        public SysMachine<SysMachineRocket, MachineRocket>
{
public:

    SysMachineRocket() = default;

    void update_sensor() override;
    void update_physics() override;

    Machine& instantiate() override;

private:

};


class MachineRocket : public Machine
{
    friend SysMachineRocket;

public:
    MachineRocket();
    MachineRocket(MachineRocket&& move);

    ~MachineRocket() = default;

    void propagate_output(WireOutput* output) override;

    WireInput* request_input(WireInPort port) override;
    WireOutput* request_output(WireOutPort port) override;

    std::vector<WireInput*> existing_inputs() override;
    std::vector<WireOutput*> existing_outputs() override;

private:
};


}
