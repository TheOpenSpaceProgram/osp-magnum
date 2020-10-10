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

    void floating_origin_translate(Vector3 translation);

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

    //void floating_origin_translate(Vector3 translation);

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
