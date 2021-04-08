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
#include <immintrin.h>

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

    m_scalarArraySizeBytes = EvolutionTable::padded_size_aligned<double>(bodies);
    m_rowSizeBytes = 3 * m_scalarArraySizeBytes;

    // Allocate static/one-step rows
    m_velocities = table_ptr<double>(alloc_raw<double>(m_rowSizeBytes));
    m_accelerations = table_ptr<double>(alloc_raw<double>(m_rowSizeBytes));

    size_t dblPaddedArrayCount = m_scalarArraySizeBytes / sizeof(double);
    for (size_t i = 0; i < 3* dblPaddedArrayCount; i++)
    {
        m_velocities[i] = 0.0;
        m_accelerations[i] = 0.0;
    }

    m_masses = table_ptr<double>(bodies);
    m_ids = table_ptr<Satellite>(bodies);

    for (size_t i = 0; i < dblPaddedArrayCount; i++)
    {
        m_masses[i] = 0.0;
    }

    // Allocate main table

    size_t fullTableSize = m_rowSizeBytes * timesteps;
    m_posTable = table_ptr<double>(alloc_raw<double>(fullTableSize));

    for (size_t i = 0; i < 3 * dblPaddedArrayCount * timesteps; i++)
    {
        m_posTable[i] = 1.0;
    }
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
    size_t componentOffset = m_scalarArraySizeBytes / sizeof(double);
    double x = m_velocities[0* componentOffset + index];
    double y = m_velocities[1* componentOffset + index];
    double z = m_velocities[2* componentOffset + index];
    return Vector3d{x, y, z};
}

void EvolutionTable::set_velocity(size_t index, Vector3d vel)
{
    size_t componentOffset = m_scalarArraySizeBytes / sizeof(double);
    m_velocities[0 * componentOffset + index] = vel.x();
    m_velocities[1 * componentOffset + index] = vel.y();
    m_velocities[2 * componentOffset + index] = vel.z();
}

Vector3d EvolutionTable::get_acceleration(size_t index)
{
    size_t componentOffset = m_scalarArraySizeBytes / sizeof(double);
    double x = m_accelerations[0 * componentOffset + index];
    double y = m_accelerations[1 * componentOffset + index];
    double z = m_accelerations[2 * componentOffset + index];
    return Vector3d{x, y, z};
}

void EvolutionTable::set_acceleration(size_t index, Vector3d accel)
{
    size_t componentOffset = m_scalarArraySizeBytes / sizeof(double);
    m_accelerations[0 * componentOffset + index] = accel.x();
    m_accelerations[1 * componentOffset + index] = accel.y();
    m_accelerations[2 * componentOffset + index] = accel.z();
}

Vector3d EvolutionTable::get_position(size_t index, size_t timestep)
{
    size_t rowOffset = timestep * m_rowSizeBytes / sizeof(double);
    size_t componentOffset = m_scalarArraySizeBytes / sizeof(double);
    double x = m_posTable[rowOffset + 0 * componentOffset + index];
    double y = m_posTable[rowOffset + 1 * componentOffset + index];
    double z = m_posTable[rowOffset + 2 * componentOffset + index];
    return Vector3d{x, y, z};
}

void EvolutionTable::set_position(size_t index, size_t timestep, Vector3d pos)
{
    size_t rowOffset = timestep * m_rowSizeBytes / sizeof(double);
    size_t componentOffset = m_scalarArraySizeBytes / sizeof(double);
    m_posTable[rowOffset + 0 * componentOffset + index] = pos.x();
    m_posTable[rowOffset + 1 * componentOffset + index] = pos.y();
    m_posTable[rowOffset + 2 * componentOffset + index] = pos.z();
}

EvolutionTable::SystemState EvolutionTable::get_system_state(size_t timestep)
{
    size_t rowOffset = timestep * m_rowSizeBytes / sizeof(double);
    size_t componentOffset = m_scalarArraySizeBytes / sizeof(double);

    double* x = &m_posTable[rowOffset + 0 * componentOffset];
    double* y = &m_posTable[rowOffset + 1 * componentOffset];
    double* z = &m_posTable[rowOffset + 2 * componentOffset];
    double* vx = &m_velocities[0 * componentOffset];
    double* vy = &m_velocities[1 * componentOffset];
    double* vz = &m_velocities[2 * componentOffset];
    double* ax = &m_accelerations[0 * componentOffset];
    double* ay = &m_accelerations[1 * componentOffset];
    double* az = &m_accelerations[2 * componentOffset];
    double* masses = &m_masses[0];
    size_t nElems = m_nBodies;
    size_t arraySizes = m_scalarArraySizeBytes;
    return {{x, y, z}, {vx, vy, vz}, {ax, ay, az}, masses, nElems, arraySizes};
}

void EvolutionTable::copy_step_to_top(size_t timestep)
{
    size_t rowEntries = m_rowSizeBytes / sizeof(double);
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
    /*if (m_data.m_currentStep == (m_data.m_nTimesteps - 1))
    {
        std::cout << "Resolving table\n";
        m_data.copy_step_to_top(m_data.m_nTimesteps - 1);
        m_data.m_currentStep = 0;
        solve_table();
    }*/

    solve_timestep_AVX(m_data.m_currentStep);

    m_data.m_currentStep++;

    if (m_data.m_currentStep == m_data.m_nTimesteps)
    {
        m_data.m_currentStep = 0;
    }

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

    solve_table();
}

void TrajNBody::solve_table()
{
    for (size_t i = 1; i < m_data.m_nTimesteps; i++)
    {
        solve_timestep_AVX(i);
    }
}

void osp::universe::TrajNBody::solve_timestep(size_t stepIndex)
{
    constexpr double dt = smc_timestep;
    assert(stepIndex < m_data.m_nTimesteps);
    // Set previous step, account for table wraparound
    size_t prevStep = ((stepIndex == 0) ? m_data.m_nTimesteps : stepIndex) - 1;

    for (size_t m = 0; m < m_data.m_nBodies; m++)
    {
        Satellite current = m_data.get_ID(m);
        Vector3d currentPos = m_data.get_position(m, prevStep);
        double currentMass = m_data.get_mass(m);

        Vector3d A{0.0};
        for (size_t n = 0; n < m_data.m_nBodies; n++)
        {
            if (n == m) { continue; }

            Vector3d r = m_data.get_position(n, prevStep) - currentPos;
            double otherMass = m_data.get_mass(n);
            Vector3d rHat = r.normalized();
            double denom = r.x() * r.x() + r.y() * r.y() + r.z() * r.z();
            A += (otherMass / denom) * rHat;
        }

        m_data.set_acceleration(m, A * G);
    }

    for (size_t n = 0; n < m_data.m_nBodies; n++)
    {
        Vector3d x = m_data.get_position(n, prevStep);
        Vector3d v = m_data.get_velocity(n);
        Vector3d a = m_data.get_acceleration(n);

        Vector3d newVel = v + a * dt;
        Vector3d newPos = x + newVel * dt;
        m_data.set_velocity(n, newVel);
        m_data.set_position(n, stepIndex, newPos);
    }
}

void TrajNBody::solve_timestep_AVX(size_t stepIndex)
{
    constexpr double dt = smc_timestep;
    assert(stepIndex < m_data.m_nTimesteps);
    // Set previous step, account for table wraparound
    size_t prevStep = ((stepIndex == 0) ? m_data.m_nTimesteps : stepIndex) - 1;

    __m256d vec4_0 = _mm256_set_pd(0.0, 0.0, 0.0, 0.0);
    __m256d vec4_1 = _mm256_set_pd(1.0, 1.0, 1.0, 1.0);
    constexpr size_t simdWidthBytes = 256/8;
    size_t nLoops = m_data.m_scalarArraySizeBytes / simdWidthBytes;

    EvolutionTable::SystemState prevState = m_data.get_system_state(prevStep);

    for (size_t n = 0; n < m_data.m_nBodies; n++)
    {
        double posX = prevState.m_position.x[n];
        double posY = prevState.m_position.y[n];
        double posZ = prevState.m_position.z[n];
        __m256d ownPos = _mm256_set_pd(posX, posY, posZ, 0.0);
        __m256d ownPosxxx = _mm256_set_pd(posX, posX, posX, posX);
        __m256d ownPosyyy = _mm256_set_pd(posY, posY, posY, posY);
        __m256d ownPoszzz = _mm256_set_pd(posZ, posZ, posZ, posZ);

        __m256d a = _mm256_set_pd(0.0, 0.0, 0.0, 0.0);
        __m256i id = _mm256_set_epi64x(n, n, n, n);
        for (size_t i = 0; i < 4*nLoops; i += 4)
        {
            // Fetch next 4 sources
            __m256d dx = _mm256_load_pd(prevState.m_position.x + i);
            __m256d dy = _mm256_load_pd(prevState.m_position.y + i);
            __m256d dz = _mm256_load_pd(prevState.m_position.z + i);
            __m256d masses = _mm256_load_pd(prevState.m_masses + i);
            // Compute positions rel. to asteroid
            dx = _mm256_sub_pd(dx, ownPosxxx);
            dy = _mm256_sub_pd(dy, ownPosyyy);
            dz = _mm256_sub_pd(dz, ownPoszzz);

            // Square components
            __m256d x2 = _mm256_mul_pd(dx, dx);
            __m256d y2 = _mm256_mul_pd(dy, dy);
            __m256d z2 = _mm256_mul_pd(dz, dz);

            // Sum to get norm squared
            __m256d normSqd = _mm256_add_pd(x2, y2);
            normSqd = _mm256_add_pd(normSqd, z2);

            __m256d norm = _mm256_sqrt_pd(normSqd);
            __m256d invNorm = _mm256_div_pd(vec4_1, norm);

            // Compute gravity coefficients (mass / denom) * (1/norm)
            __m256d gravCoeff = _mm256_div_pd(masses, normSqd);
            gravCoeff = _mm256_mul_pd(gravCoeff, invNorm);

            // Check for calculation against self
            __m256i indices = _mm256_set_epi64x(3, 2, 1, 0);
            __m256i baseIdx = _mm256_set_epi64x(i, i, i, i);
            indices = _mm256_add_epi64(indices, baseIdx);
            __m256i isEqual = _mm256_cmpeq_epi64(id, indices);
            gravCoeff = _mm256_blendv_pd(gravCoeff, vec4_0, _mm256_castsi256_pd(isEqual));

            // Compute force components
            dx = _mm256_mul_pd(dx, gravCoeff);
            dy = _mm256_mul_pd(dy, gravCoeff);
            dz = _mm256_mul_pd(dz, gravCoeff);

            /* Horizontal sum into net force

            dx = [F4.x, F3.x, F2.x, F1.x]
            dy = [F4.y, F3.y, F2.y, F1.y]
            dz = [F4.z, F3.z, F2.z, F1.z]
            need [F.x, F.y, F.z, 0]
            */

            // hsum into [y3+y4, x3+x4, y1+y2, x1+x2]
            __m256d xy = _mm256_hadd_pd(dx, dy);
            // permute 3,2,1,0 -> 1,2,0,2
            // xy becomes [y1+y2, y3+y4, x1+x2, x3+x4]
            xy = _mm256_permute4x64_pd(xy, 0b01110010);

            // hsum into [z3+z4, z3+z4, z1+z2, z1+z2]
            __m256d zz = _mm256_hadd_pd(dz, dz);
            // permute 3,2,1,0 -> 0,2,1,3
            zz = _mm256_permute4x64_pd(zz, 0b00100111);
            // produce [x1+x2, x3+x4, z1+z2, z3+z4] from xy, zz
            __m256d xz = _mm256_permute2f128_pd(xy, zz, 0b00010);
            // hsum xy [y1+y2, y3+y4, x1+x2, x3+x4]
            //      xz [x1+x2, x3+x4, z1+z2, z3+z4]
            //   xyz = [x1234, y1234, z1234, x1234]
            __m256d xyz = _mm256_hadd_pd(xy, xz);

            // Accumulate acceleration
            a = _mm256_add_pd(a, xyz);
        }

        constexpr double convert = G;
        __m256d c = _mm256_set_pd(convert, convert, convert, convert);
        a = _mm256_mul_pd(a, c);

        double data[4];
        _mm256_storeu_pd(data, a);
        m_data.set_acceleration(n, {data[3], data[2], data[1]});
    }

    /*for (size_t n = 0; n < m_data.m_nBodies; n++)
    {
        Vector3d x = m_data.get_position(n, prevStep);
        Vector3d v = m_data.get_velocity(n);
        Vector3d a = m_data.get_acceleration(n);

        Vector3d newVel = v + a * dt;
        Vector3d newPos = x + newVel * dt;
        m_data.set_velocity(n, newVel);
        m_data.set_position(n, stepIndex, newPos);
    }*/
    EvolutionTable::SystemState newState = m_data.get_system_state(stepIndex);
    __m256d dt_4 = _mm256_set_pd(dt, dt, dt, dt);
    for (size_t i = 0; i < 4 * nLoops; i += 4)
    {
        __m256d ax = _mm256_load_pd(prevState.m_acceleration.x + i);
        __m256d ay = _mm256_load_pd(prevState.m_acceleration.y + i);
        __m256d az = _mm256_load_pd(prevState.m_acceleration.z + i);

        __m256d vx = _mm256_load_pd(prevState.m_velocity.x + i);
        __m256d vy = _mm256_load_pd(prevState.m_velocity.y + i);
        __m256d vz = _mm256_load_pd(prevState.m_velocity.z + i);

        vx = _mm256_fmadd_pd(ax, dt_4, vx);
        vy = _mm256_fmadd_pd(ay, dt_4, vy);
        vz = _mm256_fmadd_pd(az, dt_4, vz);

        __m256d x = _mm256_load_pd(prevState.m_position.x + i);
        __m256d y = _mm256_load_pd(prevState.m_position.y + i);
        __m256d z = _mm256_load_pd(prevState.m_position.z + i);

        x = _mm256_fmadd_pd(vx, dt_4, x);
        y = _mm256_fmadd_pd(vy, dt_4, y);
        z = _mm256_fmadd_pd(vz, dt_4, z);

        _mm256_store_pd(newState.m_velocity.x + i, vx);
        _mm256_store_pd(newState.m_velocity.y + i, vy);
        _mm256_store_pd(newState.m_velocity.z + i, vz);
        _mm256_store_pd(newState.m_position.x + i, x);
        _mm256_store_pd(newState.m_position.y + i, y);
        _mm256_store_pd(newState.m_position.z + i, z);
    }
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

