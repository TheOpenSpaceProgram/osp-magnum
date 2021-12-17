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

/**
 * @brief Tag for satellites that can be activated
 *
 * Activatable satellites are 'real' physical things that could be 'activated'
 * into the scene to be interacted with.
 *
 * ie: planets, atmospheres, vehicles, stars.
 *
 * Non-activatable satellites may be waypoints, barycenters, lagrange points
 *
 */
struct UCompActivatable { };

//-----------------------------------------------------------------------------

/**
 * @brief Rule to have this satellite always activated
 */
struct UCompActivationAlways { };

/**
 * @brief Rule to activate a satellite when nearby an ActiveArea
 */
struct UCompActivationRadius
{
    float m_radius{ 8.0f };
};

//-----------------------------------------------------------------------------

struct UCompActiveArea
{
    float m_areaRadius{ 1024.0f };

    entt::basic_sparse_set<Satellite> m_inside{};

    coordspace_index_t m_captureSpace;

    coordspace_index_t m_releaseSpace;

    // maybe have multiple release coordspaces with priorities?
    //std::vector<coordspace_index_t> m_releaseSpaces;

    // Input queues
    std::vector<Vector3g> m_requestMove;

    // Output queues
    std::vector<Satellite> m_enter;
    std::vector<Satellite> m_leave;
    std::vector<Vector3g> m_moved;
};


class SatActiveArea
{
public:

    static constexpr std::string_view smc_name = "ActiveArea";

    /**
     * @brief Move an ActiveArea according to m_requestMove
     *
     * @param rUni    [ref] Universe containing ActiveArea satellite
     * @param areaSat [in] Satellite with UCompActiveArea
     */
    static void move(Universe& rUni, Satellite areaSat, UCompActiveArea& rArea);

    /**
     * @brief Activate all satellites with UCompActivationAlways
     *
     * TODO: not yet implemented
     *
     * @param rUni    [ref] Universe containing ActiveArea satellite
     * @param areaSat [in] Satellite with UCompActiveArea
     */
    static void scan_always(Universe& rUni, Satellite areaSat);

    /**
     * @brief Scan for nearby activatable satellites with UCompActivationRadius.
     *
     * @param rUni    [ref] Universe containing ActiveArea satellite
     * @param areaSat [in] Satellite with UCompActiveArea
     */
    static void scan_radius(
            Universe& rUni,  Satellite areaSat, UCompActiveArea& rArea,
            ucomp_view_t<UCompActivationRadius const> viewActRadius);

    /**
     * @brief Request to move Satellites into the ActiveArea's 'capture'
     *        coordinate space.
     *
     * This will give the ActiveArea full control over the Satellite's
     * movements.
     *
     * @param rUni      [ref] Universe containing ActiveArea satellite
     * @param areaSat   [in] Satellite with UCompActiveArea
     * @param toCapture [in] Satellite to capture
     */
    static void capture(
            Universe& rUni, Satellite areaSat, UCompActiveArea& rArea,
            Corrade::Containers::ArrayView<Satellite> toCapture);

    /**
     * @brief Update positions of captured objects
     *
     * @param rUni    [ref] Universe containing ActiveArea satellite
     * @param capture [in] Index to capture coordinate space
     */
    static void update_capture(Universe& rUni, coordspace_index_t capture);
};

}
