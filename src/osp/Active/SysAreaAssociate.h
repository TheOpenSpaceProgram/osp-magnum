#pragma once

#include "activetypes.h"
#include "../Universe.h"

#include "../Satellites/SatActiveArea.h"

#include <cstdint>

namespace osp::active
{

class SysAreaAssociate;
struct ACompActivatedSat;

//typedef int (*LoadStrategy)(ActiveScene &scene, SysAreaAssociate &area,
//                universe::Satellite areaSat, universe::Satellite loadMe);

// TODO: maybe consider getting rid of raw pointers here somehow.
//       none of these take ownership, only reference

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
    virtual StatusActivated activate_sat(ActiveScene &scene, SysAreaAssociate &area,
            universe::Satellite areaSat, universe::Satellite tgtSat) = 0;
    virtual int deactivate_sat(ActiveScene &scene, SysAreaAssociate &area,
            universe::Satellite areaSat, universe::Satellite tgtSat,
            ActiveEnt tgtEnt) = 0;
};

class SysAreaAssociate : public IDynamicSystem
{
public:
    SysAreaAssociate(ActiveScene &rScene, universe::Universe &uni);
    ~SysAreaAssociate() = default;

    /**
     *
     */
    void update_scan();

    void connect(universe::Satellite sat);
    void disconnect();

    /**
     * Update position of the associated Satellite of entity
     */
    void sat_transform_update(ActiveEnt ent);

    /**
     * Add a loading strategy to add support for loading a type of satellite
     * @param type
     * @param function
     */
    //void activate_func_add(universe::ITypeSatellite* type,
    //                       LoadStrategy function);
    void activator_add(universe::ITypeSatellite const* type, IActivator &activator);

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

    UpdateOrderHandle m_updateScan;

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
