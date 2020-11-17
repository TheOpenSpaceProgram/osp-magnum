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

class SysAreaAssociate;
struct ACompActivatedSat;

struct StatusActivated
{
    int m_status; // 0 for no errors
    ActiveEnt m_entity;
    bool m_mutable;
};

/**
 * An interface that 'activates' and 'deactivates' a type of ITypeSatellite
 * into an ActiveScene.
 */
class IActivator
{
public:
    virtual StatusActivated activate_sat(
            ActiveScene &scene, SysAreaAssociate &area,
            universe::Satellite areaSat, universe::Satellite tgtSat) = 0;
    virtual int deactivate_sat(
            ActiveScene &scene, SysAreaAssociate &area,
            universe::Satellite areaSat, universe::Satellite tgtSat,
            ActiveEnt tgtEnt) = 0;
};

class SysAreaAssociate : public IDynamicSystem
{
public:
    SysAreaAssociate(ActiveScene &rScene, universe::Universe &uni);
    ~SysAreaAssociate() = default;

    /**
     * Attempt to activate all the Satellites in the Universe for now.
     *
     * What this is suppose to do in the future:
     * Scan for nearby Satellites and attempt to activate them
     */
    void update_scan();

    /**
     * Connect this AreaAssociate to an ActiveArea Satellite. This sets the
     * region of space in the universe the ActiveScene will represent
     *
     * @param sat [in] Satellite containing a UCompActiveArea
     */
    void connect(universe::Satellite sat);

    /**
     * Deactivate all Activated Satellites and cut ties with ActiveArea
     * Satellite
     */
    void disconnect();

    /**
     * Move the ActiveArea satellite, and translate everything in the
     * ActiveScene, aka: floating origin translation
     */
    void area_move(Vector3s translate);

    /**
     * Update position of ent's associated Satellite in the Universe, based on
     * it's transform in the ActiveScene.
     */
    void sat_transform_update(ActiveEnt ent);

    /**
     * Add an Activator, to add support for a type of satellite
     * @param type [in] The type of Satellite that will be passed to the Activator
     * @param activator [in]
     */
    void activator_add(universe::ITypeSatellite const* type,
                       IActivator &activator);

    constexpr universe::Universe& get_universe() { return m_universe; }

    using MapActivators
            = std::map<universe::ITypeSatellite const*, IActivator*>;

private:

    ActiveScene &m_scene;

    universe::Satellite m_areaSat;
    universe::Universe &m_universe;

    MapActivators m_activators;
    //std::vector<universe::Satellite> m_activatedSats;
    entt::sparse_set<universe::Satellite> m_activatedSats;

    //UpdateOrderHandle m_updateFloatingOrigin;
    UpdateOrderHandle m_updateScan;

    /**
     * Translate everything in the ActiveScene
     * @param translation
     */
    void floating_origin_translate(Vector3 translation);

    /**
     * Attempt to load a satellite
     * @return status, zero for no error
     */
    int sat_activate(universe::Satellite sat,
                     universe::UCompActivatable &satAct);

    int sat_deactivate(ActiveEnt ent,
                       ACompActivatedSat& entAct);
};

struct ACompActivatedSat
{
    SysAreaAssociate::MapActivators::iterator m_activator;
    universe::Satellite m_sat;

    // Set true if the Satellite will be modified by the existance of this
    // entity, such as updating position. TODO: this does nothing yet
    bool m_mutable;
};

}
