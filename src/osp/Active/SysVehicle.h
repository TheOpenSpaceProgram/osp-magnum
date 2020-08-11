#pragma once

#include "activetypes.h"

#include "../Satellite.h"
#include "../Resource/Package.h"
#include "../Resource/blueprints.h"

#include <Magnum/Shaders/Phong.h>

namespace osp
{

class SatActiveArea;
class PrototypePart;

struct WireMachineConnection
{
    ActiveEnt m_fromPartEnt;
    unsigned m_fromMachine;
    WireOutPort m_fromPort;

    ActiveEnt m_toPartEnt;
    unsigned m_toMachine;
    WireInPort m_toPort;
};

struct CompVehicle
{
    std::vector<ActiveEnt> m_parts;
};

class SysVehicle
{
public:

    SysVehicle(ActiveScene &scene);
    SysVehicle(SysNewton const& copy) = delete;
    SysVehicle(SysNewton&& move) = delete;

    static int area_activate_vehicle(SatActiveArea& area,
                                     SatelliteObject& loadMe);

    /**
     * Create a Physical Part from a PrototypePart and put it in the world
     * @param part the part to instantiate
     * @param rootParent Entity to put part into
     * @return Pointer to object created
     */
    ActiveEnt part_instantiate(PrototypePart& part, ActiveEnt rootParent);

private:
    ActiveScene& m_scene;
    //AppPackages& m_packages;

    // temporary
    std::unique_ptr<Magnum::Shaders::Phong> m_shader;
};


}
