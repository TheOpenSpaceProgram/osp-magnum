#include "SatVehicle.h"

using namespace osp;

SatVehicle::SatVehicle() : SatelliteObject()
{

}


SatVehicle::~SatVehicle()
{

}

bool SatVehicle::is_loadable() const
{
    // might put more stuff here
    return true;
}

int SatVehicle::on_load()
{
    // do something here


    return 0;
}
