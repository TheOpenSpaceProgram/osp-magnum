#include <iostream>

#include "SatPlanet.h"

using namespace planeta::universe;

SatPlanet::SatPlanet(osp::universe::Universe &universe) :
        CommonTypeSat<SatPlanet, UCompPlanet,
                      osp::universe::UCompActivatable>(universe)
{

}
