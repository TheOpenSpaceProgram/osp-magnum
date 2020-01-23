#pragma once

#include <vector>
#include <cstdint>
#include "Satellite.h"

namespace osp
{

//typedef uint64_t Coordinate[3];
//typedef Math::Vector3<int64_t> Coordinate;

/**
 * A universe consisting of many satellites that can interact with each other.
 * [refer to document]
 */
class Universe
{
public:
    Universe();


    Satellite& add_satellite(Satellite& sat);
    template <class T, class... Args>
    Satellite& create_sat(Args&& ... args);


    const std::vector<Satellite>& get_sats() const { return m_satellites; }

protected:

    std::vector<Satellite> m_satellites;

};

/**
 *
 */
template <class T, class... Args>
Satellite& Universe::create_sat(Args&& ... args)
{
    //Satellite sat;
    //m_satellites.push_back(sat);
    //Satellite newSat;
    //m_satellites.push_back(newSat);

    m_satellites.emplace_back();
    Satellite& newSat = m_satellites.back();
    newSat.create_object<T>(args...);
    return newSat;
}

}
