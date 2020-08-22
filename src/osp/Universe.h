#pragma once

//#include "Satellite.h"
#include "types.h"

#include <entt/entity/registry.hpp>

#include <vector>
#include <cstdint>

namespace osp::universe
{

using Satellite = entt::entity;

/**
 * A specific type of Satellite. A planet, star, vehicle, etc...
 */
class ITypeSatellite
{
public:
    virtual ~ITypeSatellite() = 0;
    virtual std::string get_name() = 0;
};

/**
 * A system that manages the movement of multiple satellites, centered around a
 * single satellite
 */
class ISystemTrajectory
{
public:
    virtual ~ISystemTrajectory() = 0;
    virtual void update() = 0;
    virtual void add(Satellite sat) = 0;
    virtual void remove(Satellite sat) = 0;
    virtual Satellite get_center() = 0;

private:
    //std::vector<Satellite> k;
};


namespace ucomp
{


struct PositionTrajectory
{
    // Position is relative to trajectory's center satellite
    // no longer has a 'parent'
    Vector3s m_position;
    ISystemTrajectory* m_trajectory{nullptr};
    unsigned m_index;
};


//struct Activated
//{
//    // Index to [probably an active area] when activated
//    Satellite m_area;
//}

struct Velocity
{
    Vector3 m_velocity;
};

struct Type
{
    // maybe use an iterator
    ITypeSatellite* m_type;
};

}

//typedef uint64_t Coordinate[3];
//typedef Math::Vector3<int64_t> Coordinate;

/**
 * A universe consisting of many satellites that can interact with each other.
 * [refer to document]
 */
class Universe
{
public:
    Universe() = default;
    Universe(Universe const &copy) = delete;
    Universe(Universe &&move) = delete;
    ~Universe() = default;

    /**
     * Creates then adds a new satellite to itself
     * @param args Arguments passed to the Satellite's SatelliteObject
     * @return The new Satellite just created
     */
    //template <class T, class... Args>
    //std::pair<Satellite&, T&> sat_create(Args&& ... args);

    /**
     * Create a blank satellite, and adds it to the universe
     * @return The new Satellite just created
     */
    Satellite sat_create();

    /**
     * Remove a satellite by address
     */
    void sat_remove(Satellite sat);

    //std::vector<Package>& debug_get_packges() { return m_packages; };
    //void add_package(Package&& p);
    //void get_package(unsigned index);

private:

    //std::vector<Package> m_packages;

    //std::vector<Satellite> m_satellites;
    //std::vector<PrototypePart> m_prototypes;

    std::vector<std::unique_ptr<ISystemTrajectory>> m_trajectories;

    entt::basic_registry<Satellite> m_registry;

};


///**
// *
// */
//template <class T, class... Args>
//std::pair<Satellite&, T&> Universe::sat_create(Args&& ... args)
//{
//    //Satellite sat;
//    //m_satellites.push_back(sat);
//    //Satellite newSat;
//    //m_satellites.push_back(newSat);
//    Satellite& newSat = m_satellites.emplace_back(this);
//    T& obj = newSat.create_object<T>(args...);
//    return {newSat, obj};
//}

}
