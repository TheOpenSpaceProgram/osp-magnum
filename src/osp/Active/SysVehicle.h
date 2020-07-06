#pragma once


#include <Magnum/Shaders/Phong.h>

#include "../Satellite.h"

#include "activetypes.h"

#include "../Resource/Package.h"

namespace osp
{

class SatActiveArea;
class PrototypePart;

struct CompVehicle
{

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
