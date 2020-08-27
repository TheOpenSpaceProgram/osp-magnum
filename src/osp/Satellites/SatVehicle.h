#pragma once

#include "../Universe.h"

#include "../Resource/Resource.h"
#include "../Resource/blueprints.h"


namespace osp::universe
{

namespace ucomp
{

struct Vehicle
{
    DependRes<BlueprintVehicle> m_blueprint;
};

}

class SatVehicle : public CommonTypeSat<SatVehicle, ucomp::Vehicle, ucomp::Activatable>
{

public:
    SatVehicle(Universe& universe);
    ~SatVehicle() = default;
    virtual std::string get_name() { return "Vehicle"; };

};

}
