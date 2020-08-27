#include <iostream>

#include "SatPlanet.h"

using namespace planeta::universe;

SatPlanet::SatPlanet(osp::universe::Universe &universe) :
        CommonTypeSat<SatPlanet, ucomp::Planet,
                      osp::universe::ucomp::Activatable>(universe)
{

}
