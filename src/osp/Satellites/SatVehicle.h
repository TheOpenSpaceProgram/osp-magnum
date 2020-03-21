#pragma once

#include "../Resource/blueprints.h"
#include "../Satellite.h"

namespace osp
{

class SatVehicle : public SatelliteObject
{

public:
    SatVehicle();
    ~SatVehicle();

    int on_load();

    VehicleBlueprint& get_blueprint() { return m_blueprint; }

private:

    // blueprint to load when loaded by active area
    VehicleBlueprint m_blueprint;

};

}
