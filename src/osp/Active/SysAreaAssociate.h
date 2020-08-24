#pragma once

#include "activetypes.h"
#include "../Universe.h"

#include "../Satellites/SatActiveArea.h"

#include <cstdint>

namespace osp::active
{

typedef int (*LoadStrategy)(ActiveScene& scene, osp::universe::SatActiveArea& area,
                universe::Satellite areaSat, universe::Satellite loadMe);


class SysAreaAssociate : public IDynamicSystem
{
public:
    SysAreaAssociate(ActiveScene &rScene);
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

private:

    ActiveScene &m_scene;

    universe::Satellite m_areaSat;

    std::map<universe::ITypeSatellite*, LoadStrategy> m_loadFunctions;
    std::vector<universe::Satellite> m_activatedSats;

    UpdateOrderHandle m_updateScan;
};

}
