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

#include "activetypes.h"
#include "../Universe.h"

#include "../Satellites/SatActiveArea.h"

#include <cstdint>

namespace osp::active
{

struct ActivationTracker
{
    using MapSatToEnt_t = std::unordered_map<universe::Satellite, ActiveEnt>;

    // Satellites that are currently inside the active area
    // possibly replace with a single entt sparse_set in ACompAreaLink
    MapSatToEnt_t m_inside;

    std::vector<MapSatToEnt_t::iterator> m_enter;
    std::vector<std::pair<universe::Satellite, ActiveEnt>> m_leave;
};

struct ACompAreaLink
{
    ACompAreaLink(universe::Universe& rUniverse, universe::Satellite areaSat)
     : m_areaSat(areaSat)
     , m_rUniverse(rUniverse)
     , m_activated(rUniverse.sat_type_count())
    { }

    ActivationTracker& get_tracker(universe::TypeSatIndex type)
    { return m_activated[std::size_t(type)]; }

    universe::Universe& get_universe() noexcept
    { return m_rUniverse.get(); }

    universe::Satellite m_areaSat;

    std::reference_wrapper<universe::Universe> m_rUniverse;
    std::vector<ActivationTracker> m_activated;

};

struct ACompActivatedSat
{
    universe::Satellite m_sat;
};


/**
 * System used to associate an ActiveScene to a UCompActiveArea in the Universe
 */
class SysAreaAssociate : public IDynamicSystem
{
public:

    static const std::string smc_name;

    SysAreaAssociate(ActiveScene &rScene);
    ~SysAreaAssociate() = default;

    /**
     * Attempt to activate all the Satellites in the Universe for now.
     *
     * What this is suppose to do in the future:
     * Scan for nearby Satellites and attempt to activate them
     */
    static void update_scan(ActiveScene& rScene);

    static ACompAreaLink* try_get_area_link(ActiveScene &rScene);

    /**
     * Connect this AreaAssociate to an ActiveArea Satellite. This sets the
     * region of space in the universe the ActiveScene will represent
     *
     * @param sat [in] Satellite containing a UCompActiveArea
     */
    static void connect(ActiveScene& rScene, universe::Universe &rUni,
                        universe::Satellite areaSat);

    /**
     * Deactivate all Activated Satellites and cut ties with ActiveArea
     * Satellite
     */
    static void disconnect(ActiveScene& rScene);

    /**
     * Move the ActiveArea satellite, and translate everything in the
     * ActiveScene, aka: floating origin translation
     */
    static void area_move(ActiveScene& rScene, Vector3s translate);

    /**
     * Update position of ent's associated Satellite in the Universe, based on
     * a transform in ActiveScene.
     */
    static void sat_transform_update(
            universe::Universe& rUni, universe::Satellite areaSat,
            universe::Satellite tgtSat, Matrix4 transform);

private:

    UpdateOrderHandle_t m_updateScan;

    /**
     * Translate everything in the ActiveScene
     * @param translation
     */
    static void floating_origin_translate(ActiveScene& rScene, Vector3 translation);

};

}
