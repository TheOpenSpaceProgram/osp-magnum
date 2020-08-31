#pragma once

#include "../Universe.h"

namespace osp::universe
{

/**
 * A static universe where everything stays still
 */
class TrajStationary : public CommonTrajectory<TrajStationary>
{
public:

    TrajStationary(Universe& universe, Satellite center);
    ~TrajStationary() = default;
    void update() {};
};

}
