#include "simple.h"
#include "vehicles.h"

#include <osp/Satellites/SatActiveArea.h>
#include <osp/Satellites/SatVehicle.h>

#include <osp/Trajectories/Stationary.h>

#include <planet-a/Satellites/SatPlanet.h>

using osp::Package;

using osp::universe::Satellite;
using osp::universe::Universe;

using osp::universe::SatActiveArea;
using osp::universe::SatVehicle;

using osp::universe::TrajStationary;

using osp::universe::UCompTransformTraj;

using planeta::universe::SatPlanet;
using planeta::universe::UCompPlanet;


void create_simple_solar_system(osp::OSPApplication& ospApp)
{
    Universe &uni = ospApp.get_universe();
    Package &pkg = ospApp.debug_get_packges()[0];

    // Get the planet system used to create a UCompPlanet
    SatPlanet &typePlanet = static_cast<planeta::universe::SatPlanet&>(
            *uni.sat_type_find("Planet")->second);

    // Create trajectory that will make things added to the universe stationary
    auto &stationary = uni.trajectory_create<TrajStationary>(
                                        uni, uni.sat_root());

    // Create 10 random vehicles
    for (int i = 0; i < 10; i ++)
    {
        // Creates a random mess of spamcans as a vehicle
        Satellite sat = debug_add_random_vehicle(uni, pkg, "TestyMcTestFace Mk"
                                                 + std::to_string(i));

        auto &posTraj = uni.get_reg().get<UCompTransformTraj>(sat);

        posTraj.m_position = osp::Vector3s(i * 1024l * 5l, 0l, 0l);
        posTraj.m_dirty = true;

        stationary.add(sat);
    }

    Satellite sat = debug_add_deterministic_vehicle(uni, pkg, "Stomper Mk. I");
    auto& posTraj = uni.get_reg().get<UCompTransformTraj>(sat);
    posTraj.m_position = osp::Vector3s(22 * 1024l * 5l, 0l, 0l);
    posTraj.m_dirty = true;
    stationary.add(sat);


    // Add Grid of planets too

    for (int x = -0; x < 1; x ++)
    {
        for (int z = -0; z < 1; z ++)
        {
            Satellite sat = uni.sat_create();

            // assign sat as a planet
            UCompPlanet &planet = typePlanet.add_get_ucomp(sat);

            // set radius
            planet.m_radius = 128;

            auto &posTraj = uni.get_reg().get<UCompTransformTraj>(sat);

            // space planets 400m apart from each other
            // 1024 units = 1 meter
            posTraj.m_position = {x * 1024l * 400l,
                                  1024l * -140l,
                                  z * 1024l * 400l};
        }
    }
}
