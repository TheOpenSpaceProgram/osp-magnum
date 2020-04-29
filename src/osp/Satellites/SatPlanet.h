#pragma once

#include "../Satellite.h"

namespace osp
{


class SatPlanet : public SatelliteObject
{

public:
    SatPlanet();
    ~SatPlanet();

    static Id const& get_id_static()
    {
        static Id id{ typeid(SatPlanet).name() };
        return id;
    }

    Id const& get_id() override
    {
        return get_id_static();
    }

    bool is_loadable() const override;

private:


};

}
