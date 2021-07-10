/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "common.h"

#include <osp/CoordinateSpaces/CartesianSimple.h>
#include <osp/Satellites/SatActiveArea.h>


using namespace testapp;

using osp::universe::Universe;

using osp::universe::SatActiveArea;
using osp::universe::UCompActiveArea;

using osp::universe::CoordinateSpace;
using osp::universe::CoordspaceCartesianSimple;

std::function<void(Universe&)> testapp::generate_simple_universe_update(
        osp::universe::coordspace_index_t cartesian)
{
    return [cartesian] (Universe &rUni) {

        CoordinateSpace &rSpace = rUni.coordspace_get(cartesian);

        auto *pData = entt::any_cast<CoordspaceCartesianSimple>(&rSpace.m_data);

        rUni.coordspace_update_sats(cartesian);
        CoordspaceCartesianSimple::update_exchange(rUni, rSpace, *pData);
        rSpace.m_toAdd.clear();
        CoordspaceCartesianSimple::update_views(rSpace, *pData);

        // Update ActiveArea if exists

        auto viewAreas = rUni.get_reg().view<UCompActiveArea>();

        if (!viewAreas.empty())
        {
            SatActiveArea::scan(rUni, viewAreas.front());
        }
    };
}
