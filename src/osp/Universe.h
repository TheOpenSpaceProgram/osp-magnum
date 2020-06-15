#pragma once

#include <vector>
#include <cstdint>

#include "Resource/Package.h"
#include "Satellite.h"
#include "Resource/PrototypePart.h"


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

    /**
     * Creates then adds a new satellite to itself
     * @param args Arguments passed to the Satellite's SatelliteObject
     * @return The new Satellite just created
     */
    template <class T, class... Args>
    std::pair<Satellite&, T&> sat_create(Args&& ... args);

    /**
     * Create a blank satellite, and adds it to the universe
     * @return The new Satellite just created
     */
    Satellite& sat_create();

    /**
     * Remove a satellite by address
     */
    void sat_remove(Satellite* sat);

    /**
     * @return Vector of satellites.
     */
    std::vector<Satellite>& get_sats() { return m_satellites; }

    std::vector<Package>& debug_get_packges() { return m_packages; };
    //void add_package(Package&& p);
    //void get_package(unsigned index);

private:

    std::vector<Package> m_packages;

    std::vector<Satellite> m_satellites;
    //std::vector<PrototypePart> m_prototypes;

};


/**
 *
 */
template <class T, class... Args>
std::pair<Satellite&, T&> Universe::sat_create(Args&& ... args)
{
    //Satellite sat;
    //m_satellites.push_back(sat);
    //Satellite newSat;
    //m_satellites.push_back(newSat);
    Satellite& newSat = m_satellites.emplace_back(this);
    T& obj = newSat.create_object<T>(args...);
    return {newSat, obj};
}

}
