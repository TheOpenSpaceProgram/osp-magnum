#include <iostream>

#include "SatPlanet.h"

namespace osp
{
SatPlanet::SatPlanet() : SatelliteObject()
{
    // put stuff here eventually
    m_radius = 128;
}


SatPlanet::~SatPlanet()
{

}

bool SatPlanet::is_activatable() const
{
    // might put more stuff here
    return true;
}

}


