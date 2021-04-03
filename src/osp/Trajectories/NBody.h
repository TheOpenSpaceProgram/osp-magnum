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
#pragma once

#include "../Universe.h"

#include <memory>

namespace osp::universe
{

struct UCompAsteroid {};

/*

Table structure (concept):

| Body 1 | Body 2 |  ...  | Body N |
| Vxyz,M | Vxyz,M |  ...  | Vxyz,M | Vel, mass, metadata
|--------|--------|-------|--------|
| X,Y,Z  | X,Y,Z  |  ...  | X,Y,Z  | Step 1
| X,Y,Z  | X,Y,Z  |  ...  | X,Y,Z  | Step 2
|        |        |       |        | ...
| X,Y,Z  | X,Y,Z  |  ...  | X,Y,Z  | Step N

Table structure (implementation):

| Body 1 | Body 2 |  ...  | Body N | Metadata
| M      | M      |  ...  | M      | Masses
| Vxyz   | Vxyz   |  ...  | Vxyz   | Velocities

| X[N] || Y[N] || Z[N] || Step 1   (|| == padding)
| X[N] || Y[N] || Z[N] || Step 2
| X[N] || Y[N] || Z[N] || Step 3

*/
class EvolutionTable
{
public:
    EvolutionTable(size_t nBodies, size_t nSteps);
    EvolutionTable();
    ~EvolutionTable() = default;
    EvolutionTable(EvolutionTable const&) = delete;
    EvolutionTable(EvolutionTable&& move) = default;

    void resize(size_t bodies, size_t timesteps);
private:
    size_t m_nBodies;
    size_t m_nTimesteps;
    size_t m_currentStep;

    // Current step only/static values

    std::unique_ptr<Vector3d[], decltype(&_aligned_free)> m_velocities;
    std::unique_ptr<Vector3d[], decltype(&_aligned_free)> m_accelerations;
    std::unique_ptr<double[], decltype(&_aligned_free)> m_masses;

    // Table of positions over time
    std::unique_ptr<Vector3d[], decltype(&_aligned_free)> m_posTable;
};

/**
 * A not very static universe where everything moves constantly
 */
class TrajNBody : public CommonTrajectory<TrajNBody>
{
public:
    static constexpr double sc_timestep = 1'000.0;

    TrajNBody(Universe& rUni, Satellite center);
    ~TrajNBody() = default;
    void update();

};

}