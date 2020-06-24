#pragma once

#include "../SysMachine.h"

namespace osp
{

class MachineRocket;


class SysMachineRocket :
        public SysMachine<SysMachineRocket, MachineRocket>
{
public:

    SysMachineRocket(ActiveScene &scene);

    void update_sensor() override;
    void update_physics(float delta) override;

    Machine& instantiate(ActiveEnt ent) override;

private:
    SysPhysics &m_physics;
};


class MachineRocket : public Machine
{
    friend SysMachineRocket;

public:
    MachineRocket(ActiveEnt &ent);
    MachineRocket(MachineRocket &&move);

    ~MachineRocket() = default;

    void propagate_output(WireOutput *output) override;

    WireInput* request_input(WireInPort port) override;
    WireOutput* request_output(WireOutPort port) override;

    std::vector<WireInput*> existing_inputs() override;
    std::vector<WireOutput*> existing_outputs() override;

private:
    WireInput m_wiIgnition;
    WireInput m_wiThrottle;

    ActiveEnt m_rigidBody;
};


}
