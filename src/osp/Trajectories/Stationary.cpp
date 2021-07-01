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
#include "Stationary.h"

#include <Corrade/Containers/ArrayViewStl.h>


using namespace osp::universe;


void CoordspaceSimple::update_views(CoordinateSpace& rSpace, CoordspaceSimple& rData)
{

    size_t const ccompCount = std::max({
                ccomp_id<CCompSat>(),
                ccomp_id<CCompX>(), ccomp_id<CCompY>(), ccomp_id<CCompZ>()});

    size_t const satCount = rData.m_satellites.size();

    rSpace.m_components.resize(ccompCount + 1);

    rSpace.m_components[ccomp_id<CCompSat>()] = {
                rData.m_satellites, satCount,
                sizeof(Satellite)};

    rSpace.m_components[ccomp_id<CCompX>()] = {
            rData.m_positions, &rData.m_positions[0].x(),
            satCount, sizeof (Vector3s)};

    rSpace.m_components[ccomp_id<CCompY>()] = {
            rData.m_positions, &rData.m_positions[0].y(),
            satCount, sizeof (Vector3s)};

    rSpace.m_components[ccomp_id<CCompZ>()] = {
            rData.m_positions, &rData.m_positions[0].z(),
            satCount, sizeof (Vector3s)};
}

void TrajStationary::update(CoordinateSpace& rSpace)
{
    auto *rData = entt::any_cast<CoordspaceSimple>(&rSpace.m_data);


}




