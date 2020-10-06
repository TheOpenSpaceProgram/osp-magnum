#include "RCSController.h"
#include "osp/Active/ActiveScene.h"

#include <Magnum/Math/Vector.h>

using namespace adera::active::machines;
using namespace osp::active;
using Magnum::Vector3;


MachineRCSController::MachineRCSController()
    : Machine(true),
    m_wiCommandOrient(this, "Orient"),
    m_woThrottle(this, "Throttle")
{
    using wiretype::Percent;
    m_woThrottle.value() = Percent{0.0f};
}

MachineRCSController::MachineRCSController(MachineRCSController&& move)
    : osp::active::Machine(std::move(move)), m_wiCommandOrient(std::move(move.m_wiCommandOrient)), m_woThrottle(std::move(move.m_woThrottle))
{

}

MachineRCSController& MachineRCSController::operator=(MachineRCSController&& move)
{
    m_enable = std::move(move.m_enable);
    return *this;
}

void MachineRCSController::propagate_output(WireOutput* output)
{

}

WireInput* MachineRCSController::request_input(osp::WireInPort port)
{
    return existing_inputs()[port];
}

WireOutput* MachineRCSController::request_output(osp::WireOutPort port)
{
    return existing_outputs()[port];
}

std::vector<WireInput*> MachineRCSController::existing_inputs()
{
    return {&m_wiCommandOrient};
}

std::vector<WireOutput*> MachineRCSController::existing_outputs()
{
    return {&m_woThrottle};
}

SysMachineRCSController::SysMachineRCSController(ActiveScene& scene)
    :SysMachine<SysMachineRCSController, MachineRCSController>(scene),
    m_updateControls(scene.get_update_order(), "mach_rcs", "wire", "controls",
        std::bind(&SysMachineRCSController::update_controls, this))
{
}

float SysMachineRCSController::thruster_influence(Vector3 posOset, Vector3 direction,
    Vector3 cmdTransl, Vector3 cmdRot)
{
    using Magnum::Math::cross;
    using Magnum::Math::dot;
    using Magnum::Math::clamp;

    Vector3 thrust = -direction.normalized();
    
    float rotInfluence = 0.0f;
    float translInfluence = 0.0f;

    if (cmdRot.length() > 0.0f)
    {
        Vector3 torque = cross(posOset, thrust).normalized();
        rotInfluence = dot(torque, cmdRot.normalized());
    }

    if (cmdTransl.length() > 0.0f)
    {
        translInfluence = dot(thrust, cmdTransl.normalized());
    }

    float total = rotInfluence + translInfluence;

    if (total < .01f)
    {
        // Ignore small contributions from imprecision
        // Real thrusters can't throttle this deep anyways
        return 0.0f;
    }
    return clamp(rotInfluence + translInfluence, 0.0f, 1.0f);
}

void SysMachineRCSController::update_controls()
{
    auto view = m_scene.get_registry().view<MachineRCSController, ACompTransform>();

    for (ActiveEnt ent : view)
    {
        auto& machine = view.get<MachineRCSController>(ent);
        auto& transform = view.get<ACompTransform>(ent);

        using wiretype::AttitudeControl;
        using Magnum::Vector3;

        if (WireData *gimbal = machine.m_wiCommandOrient.connected_value())
        {
            AttitudeControl *attCtrl = std::get_if<AttitudeControl>(gimbal);

            Vector3 commandRot = attCtrl->m_attitude;
            Vector3 tmpOset{0.0f, 0.0f, -3.12f};
            Vector3 thrusterPos = transform.m_transform.translation() - tmpOset;
            Vector3 thrusterDir = transform.m_transform.rotation() * Vector3{0.0f, 0.0f, 1.0f};

            float influence = 0.0f;
            if (commandRot.length() > 0.0f)
            {
                influence = thruster_influence(thrusterPos, thrusterDir, Vector3{0.0f}, commandRot);
            }
            std::get<wiretype::Percent>(machine.m_woThrottle.value()).m_value = influence;
        }
    }
}

Machine& SysMachineRCSController::instantiate(ActiveEnt ent)
{
    return m_scene.reg_emplace<MachineRCSController>(ent);
}

Machine& SysMachineRCSController::get(ActiveEnt ent)
{
    return m_scene.reg_get<MachineRCSController>(ent);
}