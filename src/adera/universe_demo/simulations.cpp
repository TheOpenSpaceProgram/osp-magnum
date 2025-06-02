/**
 * Open Space Program
 * Copyright © 2019-2025 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "simulations.h"

#include <Magnum/Math/Math.h>
#include <Magnum/Math/Functions.h>

using osp::universe::spaceint_t;

namespace adera::sims
{

static constexpr double pi = Magnum::Math::Constants<double>::pi();

void CirclePathSim::update(std::uint64_t time) noexcept
{
    using Magnum::Math::cos;

    for (SatData &rSat : m_data)
    {
        double const cycleTime = double((time + rSat.initTime) % rSat.period) / double(rSat.period);

        auto const theta = Magnum::Radd(2.0 * pi * cycleTime);

        rSat.position.x() = spaceint_t(cos(theta) * rSat.radius);
        rSat.position.y() = spaceint_t(sin(theta) * rSat.radius);
        rSat.position.z() = 0;
    }
}


void ConstantSpinSim::update(std::uint64_t time) noexcept
{
    for (SatData &rSat : m_data)
    {
        double const cycleTime = double((time + rSat.initTime) % rSat.period) / double(rSat.period);
        auto const theta = Magnum::Rad(2.0f * pi * cycleTime);

        rSat.rot = Quaternion::rotation(theta, rSat.axis);
    }
}


void KinematicSim::update(std::uint64_t time) noexcept
{
    double const deltaTimeSec = (time - m_prevUpdateTime) * m_secPerTimeUnit;

    // Units: s/(m/PosUnit) = PosUnit/(m/s)
    // Used to get position delta from velocity: (m/s) * PosUnit/(m/s) = PosUnit
    double const velocityScale = deltaTimeSec / m_metersPerPosUnit;

    auto const count = m_data.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        SatData &rISat = m_data[i];

        Vector3d deltaVelocity{};
        for (std::size_t j = 0; j < count; ++j)
        {
            if (i == j) { continue; }

            SatData  const &rJSat         = m_data[j];
            Vector3d const relPos         = Vector3d(rJSat.position - rISat.position) * m_metersPerPosUnit;
            double   const r              = relPos.length();
            Vector3d const direction      = relPos.normalized();
            double   const forceMagnitude = (rISat.mass * rJSat.mass) / (r * r);
            Vector3d const force          = direction * forceMagnitude;
            Vector3d const acceleration   = (force / rISat.mass);

            deltaVelocity += acceleration * deltaTimeSec; // m/s/s * s = m/s
        }

        rISat.position += Vector3g(rISat.velocity * velocityScale);
        rISat.velocity += deltaVelocity;
    }
}




} // namespace adera::sims
