#pragma once

#include <osp/Active/SysMachine.h>

namespace adera::active::machines
{

class MachineRocket;


class SysMachineRocket :
        public osp::active::SysMachine<SysMachineRocket, MachineRocket>
{
public:

    SysMachineRocket(osp::active::ActiveScene &scene);

    //void update_sensor();
    void update_physics();

    osp::active::Machine& instantiate(osp::active::ActiveEnt ent) override;

    osp::active::Machine& get(osp::active::ActiveEnt ent) override;

private:

    osp::active::SysPhysics &m_physics;
    osp::active::UpdateOrderHandle m_updatePhysics;
};

/**
 *
 */
class MachineRocket : public osp::active::Machine
{
    friend SysMachineRocket;

public:
    MachineRocket();
    MachineRocket(MachineRocket &&move);

    MachineRocket& operator=(MachineRocket&& move);


    ~MachineRocket() = default;

    void propagate_output(osp::active::WireOutput *output) override;

    osp::active::WireInput* request_input(osp::WireInPort port) override;
    osp::active::WireOutput* request_output(osp::WireOutPort port) override;

    std::vector<osp::active::WireInput*> existing_inputs() override;
    std::vector<osp::active::WireOutput*> existing_outputs() override;

private:
    osp::active::WireInput m_wiGimbal;
    osp::active::WireInput m_wiIgnition;
    osp::active::WireInput m_wiThrottle;

    osp::active::ActiveEnt m_rigidBody;
};


}
