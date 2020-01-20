#pragma once

#include <vector>
#include <cstdint>
#include "Satellite.h"

namespace osp
{

typedef uint64_t Coordinate[3];
//typedef Math::Vector3<int64_t> Coordinate;

class Universe
{
public:
    Universe();

    //template <class T, class... Args>
    //T& create_sat(Args&& ... args);
    template <class T, class... Args>
    Satellite& create_sat(Args&& ... args);

protected:

    std::vector<Satellite> m_satellites;

};

}
