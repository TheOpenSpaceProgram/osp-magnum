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

#include <map>

#include "../types.h"
#include "../Active/activetypes.h"

#include "../Resource/Package.h"
#include "../Resource/PrototypePart.h"

namespace osp::universe
{


class OSPMagnum;
class SatActiveArea;

/**
 * Tag for satellites that can be activated
 */
struct UCompActivatable { };

/**
 * Added to a satellite when it is being modified by an ActiveArea
 */
struct UCompActivatedMutable
{
    Satellite m_area{entt::null};
    //active::ActiveEnt m_ent{entt::null};
};

//-----------------------------------------------------------------------------

struct UCompActivationAlways { };

struct UCompActivationRadius
{
    float m_radius{ 8.0f };
};

//-----------------------------------------------------------------------------

struct UCompActiveArea
{
    float m_areaRadius{ 1024.0f };

    entt::basic_sparse_set<Satellite> m_inside;

    coordspace_index_t m_coordSpace;

    // capture satellite
    // new satellite
    //std::vector<Satellite> ;

    // Output queues
    std::vector<Satellite> m_enter;
    std::vector<Satellite> m_leave;
};


class SatActiveArea
{
public:

    static constexpr std::string_view smc_name = "ActiveArea";

    static void scan(osp::universe::Universe& rUni, Satellite areaSat);

    /**
     * @brief Set the type of a Satellite and add a UCompActiveArea to it
     *
     * @param rUni [out] Universe containing satellite
     * @param sat  [in] Satellite add a UCompActiveArea to
     * @return Reference to UCompActiveArea created
     */
    static UCompActiveArea& add_active_area(
        osp::universe::Universe& rUni, osp::universe::Satellite sat);
};

}
