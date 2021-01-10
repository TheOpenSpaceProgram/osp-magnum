#pragma once

#include <osp/Active/SysMachine.h>
#include <Magnum/Math/Vector3.h>

namespace adera::active::machines
{

class MachineRCSController;

class SysMachineRCSController :
    public osp::active::SysMachine<SysMachineRCSController, MachineRCSController>
{
public:
    static inline std::string smc_name = "RCSController";

    SysMachineRCSController(osp::active::ActiveScene &rScene);

    void update_controls(osp::active::ActiveScene &rScene);

    osp::active::Machine& instantiate(osp::active::ActiveEnt ent) override;

    osp::active::Machine& get(osp::active::ActiveEnt ent) override;

private:
    float thruster_influence(Magnum::Vector3 posOset, Magnum::Vector3 direction,
        Magnum::Vector3 cmdTransl, Magnum::Vector3 cmdRot);

    osp::active::UpdateOrderHandle_t m_updateControls;
};

class MachineRCSController : public osp::active::Machine
{
    friend SysMachineRCSController;

public:
    MachineRCSController();
    MachineRCSController(MachineRCSController&& move) noexcept;
    MachineRCSController& operator=(MachineRCSController&& move) noexcept;
    ~MachineRCSController() = default;

    void propagate_output(osp::active::WireOutput *output) override;
    
    osp::active::WireInput* request_input(osp::WireInPort port) override;
    osp::active::WireOutput* request_output(osp::WireOutPort port) override;

    std::vector<osp::active::WireInput*> existing_inputs() override;
    std::vector<osp::active::WireOutput*> existing_outputs() override;

private:
    osp::active::WireInput m_wiCommandOrient;
    osp::active::WireOutput m_woThrottle;
};

inline MachineRCSController::MachineRCSController(MachineRCSController&& move) noexcept
    : Machine(std::move(move))
    , m_wiCommandOrient{this, std::move(move.m_wiCommandOrient)}
    , m_woThrottle{this, std::move(move.m_woThrottle)}
{}

inline MachineRCSController& MachineRCSController::operator=(MachineRCSController&& move) noexcept
{
    Machine::operator=(std::move(move));
    m_wiCommandOrient = {this, std::move(move.m_wiCommandOrient)};
    m_woThrottle = {this, std::move(move.m_woThrottle)};
    return *this;
}

} // adera::active::machines
