#pragma once

#include "../Universe.h"

namespace osp::universe::traj
{

/**
 * A static universe where everything stays still
 */
class Stationary : public CommonTrajectory<Stationary>
{
public:

    Stationary(Universe& universe, Satellite center);
    ~Stationary() = default;
    void update() {};
};

}
