#include "SatVehicle.h"

using namespace osp;

SatVehicle::SatVehicle() : SatelliteObject()
{

}


SatVehicle::~SatVehicle()
{

}

bool SatVehicle::is_activatable() const
{
    // might put more stuff here
    return true;
}

BlueprintVehicle& SatVehicle::get_blueprint()
{
    // TODO: check if blueprint is loaded
    return *(m_blueprint.get_data());
}
