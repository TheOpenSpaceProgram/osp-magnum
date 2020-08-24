#pragma once

#include <osp/Universe.h>

namespace planeta::universe
{

//using namespace osp;
//using namespace osp::universe;

namespace ucomp
{

struct Planet
{
    double m_radius;
};

}


class SatPlanet : public osp::universe::CommonTypeSat<SatPlanet, ucomp::Planet>
{
public:
    SatPlanet(osp::universe::Universe& universe);
    ~SatPlanet() = default;

    virtual std::string get_name() { return "Planet"; };

private:


};

}
