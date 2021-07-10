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


struct ACompAreaLink
{    
    ACompAreaLink(universe::Universe& rUniverse, universe::Satellite areaSat)
     : m_areaSat(areaSat)
     , m_rUniverse(rUniverse)
    { }

    universe::Universe& get_universe() noexcept
    { return m_rUniverse.get(); }

    universe::Universe const& get_universe() const noexcept
    { return m_rUniverse.get(); }

    universe::Satellite m_areaSat;

    std::reference_wrapper<universe::Universe> m_rUniverse;

};

struct ACompActivatedSat
{
    universe::Satellite m_sat;
};


/**
 * System used to associate an ActiveScene to a UCompActiveArea in the Universe
 */
class SysAreaAssociate
{
public:

    /**
     * Clears queues in associazted UCompActiveArea
     *
     * @param rScene [in/out] Scene to update
     */
    static void update_clear(ActiveScene& rScene);

    /**
     * Attempt to get an ACompAreaLink from an ActiveScene
     * @return ACompAreaLink in scene, nullptr if not found
     */
    static ACompAreaLink* try_get_area_link(ActiveScene &rScene);

    /**
     * Connect the ActiveScene to the Universe using the scene's ACompAreaLink,
     * and a Satellite containing a UCompActiveArea.
     *
     * @param rScene  [in/out] Scene containing ACompAreaLink
     * @param rUni    [ref] Universe the ActiveArea satellite is contained in.
     *                      This is stored in ACompAreaLink.
     * @param areaSat [in] ActiveArea Satellite
     */
    static void connect(ActiveScene& rScene, universe::Universe &rUni,
                        universe::Satellite areaSat);

    /**
     * Deactivate all Activated Satellites and cut ties with ActiveArea
     * Satellite (TODO)
     */
    static void disconnect(ActiveScene& rScene);

    /**
     * Move the ActiveArea satellite, and translate everything in the
     * ActiveScene, aka: floating origin translation
     */
    static void area_move(ActiveScene& rScene,
                          osp::universe::Vector3g translate);

    /**
     * Set the transform of a Satellite based on a transform (in meters)
     * Satellite.
     * @param rUni        [in/out] Universe containing satellites
     * @param relativeSat [in] Satellite that transform is relative to
     * @param tgtSat      [in] Satellite to set transform of
     * @param transform   [in] Transform to set
     */
    static void sat_transform_set_relative(
            universe::Universe& rUni, universe::Satellite relativeSat,
            universe::Satellite tgtSat, Matrix4 transform);

private:


    /**
     * Translate all entities in an ActiveScene that contain an
     * ACompFloatingOrigin
     *
     * @param rScene      [out] Scene containing entities that can be translated
     * @param translation [in] Meters to translate entities by
     */
    static void floating_origin_translate(ActiveScene& rScene, Vector3 translation);

};

}
