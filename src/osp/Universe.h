#pragma once

#include <vector>
#include <cstdint>

#include "Resource/Package.h"
#include "Satellite.h"
#include "Resource/PartPrototype.h"


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


    void add_part(PartPrototype& prototype);
    void add_parts(const std::vector<PartPrototype>& prototypes);

    /**
     * Add a satellite to the universe
     * @param sat
     * @return
     */
    Satellite& add_satellite(Satellite& sat);

    /**
     * Creates then adds a new satellite to itself
     * @param args Arguments passed to the Satellite's SatelliteObject
     * @return The new Satellite just created
     */
    template <class T, class... Args>
    Satellite& create_sat(Args&& ... args);

    /**
     * Create a blank satellite, and adds it to the universe
     * @return The new Satellite just created
     */
    Satellite& create_sat();

    /**
     * @return Read-only list of satellites.
     */
    const std::vector<Satellite>& get_sats() const { return m_satellites; }

    std::vector<Package>& debug_get_packges() { return m_packages; };
    //void add_package(Package&& p);
    //void get_package(unsigned index);

private:

    std::vector<Package> m_packages;

    std::vector<Satellite> m_satellites;
    std::vector<PartPrototype> m_prototypes;

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

    m_satellites.emplace_back(this);
    Satellite& newSat = m_satellites.back();
    newSat.create_object<T>(args...);
    return newSat;
}

}
