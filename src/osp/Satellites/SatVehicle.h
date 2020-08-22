#pragma once

#include "../Universe.h"

#include "../Resource/Resource.h"
#include "../Resource/blueprints.h"


namespace osp::universe
{

namespace ucomp
{

struct InstanceVehicle
{
    DependRes<BlueprintVehicle> m_blueprint;
};

}

class SatVehicle : public ITypeSatellite
{

public:
    SatVehicle() = default;
    ~SatVehicle() = default;
    virtual std::string get_name() { return "Vehicle"; };

};

}
