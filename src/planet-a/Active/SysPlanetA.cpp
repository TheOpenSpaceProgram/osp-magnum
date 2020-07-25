#include <iostream>

#include <osp/Active/ActiveScene.h>

#include "../Satellites/SatPlanet.h"
#include "SysPlanetA.h"


using namespace osp;

int SysPlanetA::area_activate_planet(SatActiveArea& area,
                                     SatelliteObject& loadMe)
{
    std::cout << "activatin a planet!!!!!!!!!!!!!!!!11\n";


    // Get the needed variables
    SatPlanet &planet = static_cast<SatPlanet&>(loadMe);
    ActiveScene &scene = *(area.get_scene());
    ActiveEnt root = scene.hier_get_root();

    ActiveEnt planetEnt = scene.hier_create_child(root, "Planet");

    // Convert position of the satellite to position in scene
    Vector3 positionInScene = planet.get_satellite()
            ->position_relative_to(*(area.get_satellite())).to_meters();

    CompTransform& planetTransform = scene.get_registry()
                                        .emplace<CompTransform>(planetEnt);
    planetTransform.m_transform = Matrix4::translation(positionInScene);
    planetTransform.m_enableFloatingOrigin = true;

    scene.reg_emplace<CompPlanet>(planetEnt);

    return 0;
}

SysPlanetA::SysPlanetA(ActiveScene &scene) :
    m_scene(scene),
    m_updateGeometry(scene.get_update_order(), "planet_geo", "physics", "",
                     std::bind(&SysPlanetA::update_geometry, this)),
    m_updatePhysics(scene.get_update_order(), "planet_phys", "planet_geo", "",
                    std::bind(&SysPlanetA::update_geometry, this))

{

}


void SysPlanetA::update_geometry()
{

    auto view = m_scene.get_registry().view<CompPlanet>();

    for (ActiveEnt ent : view)
    {
        CompPlanet &planet = view.get<CompPlanet>(ent);

        if (!planet.m_planet.is_initialized())
        {
            // initialize planet if not done so yet
            planet.m_planet.initialize(10);
            std::cout << "planet init\n";
        }
    }
}

void SysPlanetA::update_physics()
{

}
