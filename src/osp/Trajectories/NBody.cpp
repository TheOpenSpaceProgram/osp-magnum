/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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

#include "NBody.h"

using namespace osp::universe;

/* ============ EvolutionTable ============ */

EvolutionTable::EvolutionTable(size_t nBodies, size_t nSteps)
    : m_velocities{nullptr}
    , m_accelerations{nullptr}
    , m_masses{nullptr}
    , m_posTable{nullptr}
{
    resize(nBodies, nSteps);
}

EvolutionTable::EvolutionTable()
    : m_velocities{nullptr}
    , m_accelerations{nullptr}
    , m_masses{nullptr}
    , m_posTable{nullptr}
    , m_nBodies{0}
    , m_nTimesteps{0}
    , m_currentStep{0}
{}

void EvolutionTable::resize(size_t bodies, size_t timesteps)
{
    m_nBodies = bodies;
    m_nTimesteps = timesteps;

    size_t arrayPadding = 4 - (bodies % 4);
    size_t arraySize = (bodies + arrayPadding) * sizeof(double);
    size_t vecArraySize = 3 * arraySize;

    // Allocate static/one-step rows
    m_velocities = table_ptr<double>(3*bodies);
    m_accelerations = table_ptr<double>(3*bodies);
    m_masses = table_ptr<double>(bodies);

    // Allocate main table
    m_posTable = table_ptr<double>(3*bodies * timesteps);
}

/* ============ TrajNBody ============ */

TrajNBody::TrajNBody(Universe& rUni, Satellite center)
    : CommonTrajectory<TrajNBody>(rUni, center)
{}

void TrajNBody::update()
{
    auto& reg = m_universe.get_reg(); //rUni.get_reg();

    auto view =reg.view<UCompTransformTraj, UCompMass, UCompVel, UCompAccel>(
        entt::exclude<UCompAsteroid>);
    auto sourceView = reg.view<UCompTransformTraj, UCompMass>(
        entt::exclude<UCompAsteroid>);

    // Update accelerations (full dynamics)
    update_full_dynamics_acceleration(view, sourceView);
    update_full_dynamics_kinematics(view);
}

template <typename VIEW_T, typename SRC_VIEW_T>
void TrajNBody::update_full_dynamics_acceleration(VIEW_T& view, SRC_VIEW_T& inputView)
{
    // ### Precompute Gravity Sources ###
    struct Source
    {
        Vector3d pos;  // 24 bytes
        double mass;   // 8 bytes
        Satellite sat; // 4 bytes :'(
    };
    std::vector<Source> sources;
    sources.reserve(100);  // TMP magic constant
    for (Satellite src : inputView)
    {
        Source source
        {
            static_cast<Vector3d>(inputView.get<UCompTransformTraj>(src).m_position) / 1024.0,
            inputView.get<UCompMass>(src).m_mass,
            src
        };
        sources.push_back(std::move(source));
    }
    sources.shrink_to_fit();

    // ### Update Accelerations ###
    for (Satellite sat : view)
    {
        auto& pos = view.get<UCompTransformTraj>(sat).m_position;
        Vector3d posD = static_cast<Vector3d>(pos) / 1024.0;

        Vector3d A{0.0};
        for (Source const& src : sources)
        {
            if (src.sat == sat) { continue; }
            Vector3d r = src.pos - posD;
            Vector3d rHat = r.normalized();
            double denom = r.x() * r.x() + r.y() * r.y() + r.z() * r.z();
            A += (src.mass / denom) * rHat;
        }
        A *= 1024.0 * G;

        view.get<UCompAccel>(sat).m_acceleration = A;
    }
}

template<typename VIEW_T>
void TrajNBody::update_full_dynamics_kinematics(VIEW_T& view)
{
    constexpr double dt = smc_timestep;

    // Update velocities & positions (full dynamics)
    for (Satellite sat : view)
    {
        auto& acceleration = view.get<UCompAccel>(sat).m_acceleration;
        auto& vel = view.get<UCompVel>(sat).m_velocity;
        auto& pos = view.get<UCompTransformTraj>(sat).m_position;

        vel += acceleration * dt;
        pos += static_cast<Vector3s>(vel * dt);
    }
}

