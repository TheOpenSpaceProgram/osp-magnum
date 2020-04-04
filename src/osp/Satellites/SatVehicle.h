#pragma once

#include "../Resource/blueprints.h"
#include "../Satellite.h"

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

    int on_load() override;

    bool is_loadable() const override;


    VehicleBlueprint& get_blueprint() { return m_blueprint; }



private:

    // blueprint to load when loaded by active area
    VehicleBlueprint m_blueprint;

};

}
