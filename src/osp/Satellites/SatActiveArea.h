#pragma once

#include "../Satellite.h"

namespace osp
{

class SatActiveArea : public SatelliteObject
{
public:
    SatActiveArea();
    ~SatActiveArea();

    int on_load();

};

}
