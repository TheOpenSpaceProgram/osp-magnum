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

TrajNBody::TrajNBody(Universe& rUni, Satellite center)
    : CommonTrajectory<TrajNBody>(rUni, center)
{}

void TrajNBody::update()
{

}

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
