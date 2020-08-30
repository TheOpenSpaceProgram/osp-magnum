#pragma once

#include <osp/Universe.h>


namespace planeta::universe
{

//using namespace osp;
//using namespace osp::universe;

struct UCompPlanet
{
    double m_radius;
};


class SatPlanet : public osp::universe::CommonTypeSat<
        SatPlanet, UCompPlanet,
        osp::universe::UCompActivatable>
{
public:
    SatPlanet(osp::universe::Universe& universe);
    ~SatPlanet() = default;

    virtual std::string get_name() { return "Planet"; };

private:


};

}
