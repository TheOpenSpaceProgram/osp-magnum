#pragma once

#include "../Universe.h"

#include "../Resource/Resource.h"
#include "../Resource/blueprints.h"


namespace osp::universe
{

struct UCompVehicle
{
    DependRes<BlueprintVehicle> m_blueprint;
};

class SatVehicle : public CommonTypeSat<SatVehicle, UCompVehicle, UCompActivatable>
{

public:
    SatVehicle(Universe& universe);
    ~SatVehicle() = default;
    virtual std::string get_name() { return "Vehicle"; };

};

}
