#include "links.h"

using namespace osp;

using osp::link::MachTypeReg_t;
using osp::link::MachTypeId;

namespace adera
{

MachTypeId const gc_mtUserCtrl      = MachTypeReg_t::create();
MachTypeId const gc_mtMagicRocket   = MachTypeReg_t::create();
MachTypeId const gc_mtRcsDriver     = MachTypeReg_t::create();

float thruster_influence(Vector3 const pos, Vector3 const dir, Vector3 const cmdLin, Vector3 const cmdAng) noexcept
{
    using Magnum::Math::cross;
    using Magnum::Math::dot;

    // Thrust is applied in opposite direction of thruster nozzle direction
    Vector3 const thrustDir = std::negate{}(dir.normalized());

    float angInfluence = 0.0f;
    if (cmdAng.dot() > 0.0f)
    {
        Vector3 const torque = cross(pos, thrustDir).normalized();
        angInfluence = dot(torque, cmdAng.normalized());
    }

    float linInfluence = 0.0f;
    if (cmdLin.dot() > 0.0f)
    {
        linInfluence = dot(thrustDir, cmdLin.normalized());
    }

    // Total component of thrust in direction of command
    float const total = angInfluence + linInfluence;

    if (total < .01f)
    {
        /* Ignore small contributions from imprecision
         * Real thrusters can't throttle this deep anyways, so if their
         * contribution is this small, it'd be a waste of fuel to fire them.
         */
        return 0.0f;
    }

    /* Compute thruster throttle output demanded by current command.
     * In the future, it would be neat to implement PWM so that unthrottlable
     * thrusters would pulse on and off in order to deliver reduced thrust
     */
    float const totalInfluence = std::clamp(angInfluence + linInfluence, 0.0f, 1.0f);

    if (totalInfluence != totalInfluence) // if NaN
    {
        return 0.0f;
    }

    return totalInfluence;
}


} // namespace adera
