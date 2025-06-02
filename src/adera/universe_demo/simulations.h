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
#pragma once

#include <osp/universe/universe.h>
#include <osp/core/math_types.h>

#include <memory>

namespace adera::sims
{

using osp::Quaternion;
using osp::Vector3;
using osp::Vector3d;

using osp::universe::DataAccessor;
using osp::universe::SatelliteId;
using osp::universe::SimulationId;
using osp::universe::Vector3g;

struct CirclePathSim
{
    struct SatData
    {
        Vector3g        position;
        Vector3         velocity;
        Vector3         accel;
        double          radius;
        std::uint64_t   period;
        std::uint64_t   initTime;
        SatelliteId     id;
    };

    void update(std::uint64_t time) noexcept;

    std::vector<SatData>            m_data;
    std::uint64_t                   m_prevUpdateTime{0u};

    std::shared_ptr<DataAccessor>   m_accessor;
    SimulationId                    m_id;
};


struct ConstantSpinSim
{
    struct SatData
    {
        Quaternion      rot;
        Vector3         axis;
        std::uint64_t   period;
        std::uint64_t   initTime;
        SatelliteId     id;
    };

    void update(std::uint64_t time) noexcept;

    std::vector<SatData>            m_data;
    std::uint64_t                   m_prevUpdateTime;

    std::shared_ptr<DataAccessor>   m_accessor;
    SimulationId                    m_id;
};


struct KinematicSim
{
    struct SatData
    {
        Vector3g        position;
        Vector3d        velocity;
        Vector3d        accel;
        float           mass;
        SatelliteId     id;
    };

    void update(std::uint64_t time) noexcept;

    std::vector<SatData>            m_data;
    std::uint64_t                   m_prevUpdateTime;
    double                          m_metersPerPosUnit;
    double                          m_secPerTimeUnit;

    std::shared_ptr<DataAccessor>   m_accessor;
    SimulationId                    m_id;
};



} // namespace adera::sims
