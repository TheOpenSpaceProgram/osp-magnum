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

    /**
     * Create a new satellite
     * Arguments and template passed to Satellite::create_object to initialize
     * a SatelliteObject
     * @return The new Satellite just created
     */
    template <class T, class... Args>
    Satellite& create_sat(Args&& ... args);

    /**
     * Create a blank satellite
     * @return The new Satellite just created
     */
    Satellite& create_sat();


    const std::vector<Satellite>& get_sats() const { return m_satellites; }

private:

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
