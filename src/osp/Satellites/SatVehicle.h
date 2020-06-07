#pragma once

#include "../Satellite.h"

#include "../Resource/Resource.h"
#include "../Resource/blueprints.h"


namespace osp
{

class SatVehicle : public SatelliteObject
{

public:
    SatVehicle();
    ~SatVehicle();

    static Id const& get_id_static()
    {
        static Id id{ typeid(SatVehicle).name() };
        return id;
    }

    Id const& get_id() override
    {
        return get_id_static();
    }


    bool is_loadable() const override;


    BlueprintVehicle& get_blueprint();

    ResDependency<BlueprintVehicle>& get_blueprint_depend()
    {
        return m_blueprint;
    }



private:

    // blueprint to load when loaded by active area
    ResDependency<BlueprintVehicle> m_blueprint;

};

}
