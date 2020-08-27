#pragma once

#include "activetypes.h"
#include "../Universe.h"

#include "../Satellites/SatActiveArea.h"

#include <cstdint>

namespace osp::active
{

class SysAreaAssociate;

typedef int (*LoadStrategy)(ActiveScene &scene, SysAreaAssociate &area,
                universe::Satellite areaSat, universe::Satellite loadMe);


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
    void activate_func_add(universe::ITypeSatellite* type,
                           LoadStrategy function);

    constexpr universe::Universe& get_universe() { return m_universe; }

private:

    ActiveScene &m_scene;

    universe::Satellite m_areaSat;
    universe::Universe &m_universe;

    std::map<universe::ITypeSatellite*, LoadStrategy> m_loadFunctions;
    //std::vector<universe::Satellite> m_activatedSats;
    entt::sparse_set<universe::Satellite> m_activatedSats;

    UpdateOrderHandle m_updateScan;

    /**
     * Attempt to load a satellite
     * @return status, zero for no error
     */
    int activate_satellite(universe::Satellite sat);
};

}
