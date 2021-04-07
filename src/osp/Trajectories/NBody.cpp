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
#include <iostream>

using namespace osp::universe;
using osp::Vector3d;

/* ============ EvolutionTable ============ */

EvolutionTable::EvolutionTable(size_t nBodies, size_t nSteps)
    : m_velocities{nullptr}
    , m_accelerations{nullptr}
    , m_masses{nullptr}
    , m_ids{nullptr}
    , m_posTable{nullptr}
{
    resize(nBodies, nSteps);
}

EvolutionTable::EvolutionTable()
    : m_velocities{nullptr}
    , m_accelerations{nullptr}
    , m_masses{nullptr}
    , m_ids{nullptr}
    , m_posTable{nullptr}
    , m_nBodies{0}
    , m_nTimesteps{0}
    , m_currentStep{0}
{}

void EvolutionTable::resize(size_t bodies, size_t timesteps)
{
    m_nBodies = bodies;
    m_nTimesteps = timesteps;

    m_componentBlockSize = EvolutionTable::padded_size_aligned<double>(bodies);
    m_rowSize = 3 * m_componentBlockSize;

    // Allocate static/one-step rows
    m_velocities = table_ptr<double>(alloc_raw<double>(m_rowSize));
    m_accelerations = table_ptr<double>(alloc_raw<double>(m_rowSize));
    m_masses = table_ptr<double>(bodies);
    m_ids = table_ptr<Satellite>(bodies);

    // Allocate main table

    size_t fullTableSize = m_rowSize * timesteps;
    m_posTable = table_ptr<double>(alloc_raw<double>(fullTableSize));
}

Satellite EvolutionTable::get_ID(size_t index)
{
    return m_ids[index];
}

void EvolutionTable::set_ID(size_t index, Satellite sat)
{
    m_ids[index] = sat;
}

double EvolutionTable::get_mass(size_t index)
{
    return m_masses[index];
}

void EvolutionTable::set_mass(size_t index, double mass)
{
    m_masses[index] = mass;
}

Vector3d EvolutionTable::get_velocity(size_t index)
{
    size_t offset = m_componentBlockSize / sizeof(double);
    double x = m_velocities[0* offset + index];
    double y = m_velocities[1* offset + index];
    double z = m_velocities[2* offset + index];
    return Vector3d{x, y, z};
}

void EvolutionTable::set_velocity(size_t index, Vector3d vel)
{
    size_t offset = m_componentBlockSize / sizeof(double);
    m_velocities[0 * offset + index] = vel.x();
    m_velocities[1 * offset + index] = vel.y();
    m_velocities[2 * offset + index] = vel.z();
}

Vector3d EvolutionTable::get_acceleration(size_t index)
{
    size_t offset = m_componentBlockSize / sizeof(double);
    double x = m_accelerations[0 * offset + index];
    double y = m_accelerations[1 * offset + index];
    double z = m_accelerations[2 * offset + index];
    return Vector3d{x, y, z};
}

void EvolutionTable::set_acceleration(size_t index, Vector3d accel)
{
    size_t offset = m_componentBlockSize / sizeof(double);
    m_accelerations[0 * offset + index] = accel.x();
    m_accelerations[1 * offset + index] = accel.y();
    m_accelerations[2 * offset + index] = accel.z();
}

Vector3d EvolutionTable::get_position(size_t index, size_t timestep)
{
    size_t rowOffset = timestep * m_rowSize / sizeof(double);
    size_t colOffset = m_componentBlockSize / sizeof(double);
    double x = m_posTable[rowOffset + 0 * colOffset + index];
    double y = m_posTable[rowOffset + 1 * colOffset + index];
    double z = m_posTable[rowOffset + 2 * colOffset + index];
    return Vector3d{x, y, z};
}

void EvolutionTable::set_position(size_t index, size_t timestep, Vector3d pos)
{
    size_t rowOffset = timestep * m_rowSize / sizeof(double);
    size_t colOffset = m_componentBlockSize / sizeof(double);
    m_posTable[rowOffset + 0 * colOffset + index] = pos.x();
    m_posTable[rowOffset + 1 * colOffset + index] = pos.y();
    m_posTable[rowOffset + 2 * colOffset + index] = pos.z();
}

EvolutionTable::SystemState EvolutionTable::get_system_state(size_t timestep)
{
    size_t rowOffset = timestep * m_rowSize / sizeof(double);
    size_t colOffset = m_componentBlockSize / sizeof(double);

    double* xs = &m_posTable[rowOffset + 0 * colOffset];
    double* ys = &m_posTable[rowOffset + 1 * colOffset];
    double* zs = &m_posTable[rowOffset + 2 * colOffset];
    double* masses = &m_masses[0];
    size_t nElems = m_nBodies;
    size_t arraySizes = m_componentBlockSize;
    return {xs, ys, zs, masses, nElems, arraySizes};
}

void EvolutionTable::solve(double dt)
{
    for (size_t i = 1; i < m_nTimesteps; i++)
    {
        for (size_t m = 0; m < m_nBodies; m++)
        {
            Satellite current = get_ID(m);
            Vector3d currentPos = get_position(m, i - 1);
            double currentMass = get_mass(m);

            Vector3d A{0.0};
            for (size_t n = 0; n < m_nBodies; n++)
            {
                if (n == m) { continue; }

                Vector3d r = get_position(n, i - 1) - currentPos;
                double otherMass = get_mass(n);
                Vector3d rHat = r.normalized();
                double denom = r.x()*r.x() + r.y()*r.y() + r.z()*r.z();
                A += (otherMass / denom) * rHat;
            }

            set_acceleration(m, A*G);
        }

        for (size_t n = 0; n < m_nBodies; n++)
        {
            Vector3d x = get_position(n, i - 1);
            Vector3d v = get_velocity(n);
            Vector3d a = get_acceleration(n);

            Vector3d newVel = v + a*dt;
            Vector3d newPos = x + newVel*dt;
            set_velocity(n, newVel);
            set_position(n, i, newPos);
        }
    }
}

void EvolutionTable::copy_step_to_top(size_t timestep)
{
    size_t rowEntries = m_rowSize / sizeof(double);
    double* dest = m_posTable.get();
    double* src = &m_posTable[timestep * rowEntries];
    for (size_t i = 0; i < rowEntries; i++)
    {
        dest[i] = src[i];
    }
}

/* ============ TrajNBody ============ */

TrajNBody::TrajNBody(Universe& rUni, Satellite center)
    : CommonTrajectory<TrajNBody>(rUni, center)
{}

void TrajNBody::update()
{
    auto& reg = m_universe.get_reg(); //rUni.get_reg();

    /*auto view = reg.view<UCompTransformTraj, UCompMass, UCompVel, UCompAccel>();
    auto sourceView = reg.view<UCompTransformTraj, UCompMass, UCompEmitsGravity>();

    // Update accelerations (full dynamics)
    update_full_dynamics_acceleration(view, sourceView);
    update_full_dynamics_kinematics(view);*/

    // Spaghetti
    if (m_data.m_currentStep == (m_data.m_nTimesteps - 1))
    {
        std::cout << "Resolving table\n";
        m_data.copy_step_to_top(m_data.m_nTimesteps - 1);
        m_data.m_currentStep = 0;
        m_data.solve(smc_timestep);
    }

    m_data.m_currentStep++;

    for (size_t i = 0; i < m_data.m_nBodies; i++)
    {
        Satellite sat = m_data.get_ID(i);
        Vector3d vel = m_data.get_velocity(i);
        Vector3d accel = m_data.get_acceleration(i);
        Vector3d pos = m_data.get_position(i, m_data.m_currentStep);

        reg.get<UCompTransformTraj>(sat).m_position = Vector3s{pos} * 1024ll;
        reg.get<UCompVel>(sat).m_velocity = vel;
        reg.get<UCompAccel>(sat).m_acceleration = accel;
    }

}

void TrajNBody::build_table()
{
    std::vector<Satellite> fullBodies;
    auto& rUni = m_universe.get_reg();
    for (auto [sat] : rUni.view<UCompEmitsGravity>().each())
    {
        fullBodies.push_back(sat);
    }
    m_data.resize(fullBodies.size(), 512);

    for (size_t i = 0; i < fullBodies.size(); i++)
    {
        Satellite sat = fullBodies[i];
        m_data.set_ID(i, sat);
        m_data.set_mass(i, rUni.get<UCompMass>(sat).m_mass);
        m_data.set_velocity(i, rUni.get<UCompVel>(sat).m_velocity / 1024.0);
        m_data.set_position(i, 0,
            static_cast<Vector3d>(rUni.get<UCompTransformTraj>(sat).m_position) / 1024.0);
    }
    
    m_data.solve(smc_timestep);
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

