#pragma once

#include "activetypes.h"
#include "../Universe.h"

#include "../Satellites/SatActiveArea.h"

#include <cstdint>

namespace osp::active
{

class SysAreaAssociate;

//typedef int (*LoadStrategy)(ActiveScene &scene, SysAreaAssociate &area,
//                universe::Satellite areaSat, universe::Satellite loadMe);

/**
 * An interface that 'activates' and 'deactivates' a type of ITypeSatellite
 * into an ActiveScene.
 */
class IActivator
{
public:
    virtual int activate_sat(ActiveScene &scene, SysAreaAssociate &area,
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

    void update_scan();
    void connect(universe::Satellite sat);

    /**
     * Add a loading strategy to add support for loading a type of satellite
     * @param type
     * @param function
     */
    //void activate_func_add(universe::ITypeSatellite* type,
    //                       LoadStrategy function);
    void activator_add(universe::ITypeSatellite* type, IActivator &activator);

    constexpr universe::Universe& get_universe() { return m_universe; }

private:

    ActiveScene &m_scene;

    universe::Satellite m_areaSat;
    universe::Universe &m_universe;

    std::map<universe::ITypeSatellite*, IActivator*> m_activators;
    //std::vector<universe::Satellite> m_activatedSats;
    entt::sparse_set<universe::Satellite> m_activatedSats;

    UpdateOrderHandle m_updateScan;

    /**
     * Attempt to load a satellite
     * @return status, zero for no error
     */
    int sat_activate(universe::Satellite sat);

    int sat_deactivate(ActiveEnt entity);
};

}
