#include "SatVehicle.h"

using namespace osp::universe;

SatVehicle::SatVehicle(Universe& universe) :
        CommonTypeSat<SatVehicle, ucomp::Vehicle, ucomp::Activatable>(universe)
{

}
