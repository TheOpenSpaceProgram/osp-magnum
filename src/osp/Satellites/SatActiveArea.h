#pragma once


#include "../Types.h"
#include "../Satellite.h"

class OSPMagnum;

namespace osp
{

class SatActiveArea : public SatelliteObject
{

    friend OSPMagnum;

public:
    SatActiveArea();
    ~SatActiveArea();

    int on_load();

private:

    Scene3D m_scene;

};

}
