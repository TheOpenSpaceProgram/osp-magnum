#pragma once

//#include "Satellite.h"
#include "types.h"

#include <entt/entity/registry.hpp>

#include <vector>
#include <cstdint>

namespace osp::universe
{

class ISystemTrajectory;
class ITypeSatellite;

using Satellite = entt::entity;

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
    Universe(Universe const &copy) = delete;
    Universe(Universe &&move) = delete;
    ~Universe() = default;

    template<typename TYPESAT_T, typename ... ARGS_T>
    TYPESAT_T& register_satellite_type(ARGS_T&& ... args);

    template<typename TRAJECTORY_T, typename ... ARGS_T>
    TRAJECTORY_T& create_trajectory(ARGS_T&& ... args);


    /**
     * Create a satellite with default components, and adds it to the universe
     * @return The new Satellite just created
     */
    Satellite sat_create();

    constexpr Satellite sat_root() { return m_root; };

    /**
     * Remove a satellite by address
     */
    void sat_remove(Satellite sat);

    //std::vector<Package>& debug_get_packges() { return m_packages; };
    //void add_package(Package&& p);
    //void get_package(unsigned index);

    constexpr entt::basic_registry<Satellite>& get_reg() { return m_registry; }

private:

    //std::vector<Package> m_packages;

    //std::vector<Satellite> m_satellites;
    //std::vector<PrototypePart> m_prototypes;

    Satellite m_root;

    std::vector<std::unique_ptr<ISystemTrajectory>> m_trajectories;

    std::map<std::string, std::unique_ptr<ITypeSatellite>> m_satTypes;

    entt::basic_registry<Satellite> m_registry;

};


template<typename TYPESAT_T, typename ... ARGS_T>
TYPESAT_T& Universe::register_satellite_type(ARGS_T&& ... args)
{
    std::unique_ptr<TYPESAT_T> newType
            = std::make_unique<TYPESAT_T>(std::forward<ARGS_T>(args)...);
    m_satTypes[newType->get_name()] = std::move(newType);
    return *newType;
}

template<typename TRAJECTORY_T, typename ... ARGS_T>
TRAJECTORY_T& Universe::create_trajectory(ARGS_T&& ... args)
{
    std::unique_ptr<TRAJECTORY_T> newTraj = std::make_unique<TRAJECTORY_T>(std::forward<ARGS_T>(args)...);
    TRAJECTORY_T& toReturn = *newTraj;
    m_trajectories.push_back(std::move(newTraj)); // this invalidates newTraj
    return toReturn;
}


// default ECS components needed for the universe
namespace ucomp
{


struct PositionTrajectory
{
    // Position is relative to trajectory's center satellite
    // no longer has a 'parent'
    Vector3s m_position;
    ISystemTrajectory* m_trajectory{nullptr}; // get rid of this some day
    unsigned m_index; // index in trajectory's vector
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

/**
 * A specific type of Satellite. A planet, star, vehicle, etc...
 */
class ITypeSatellite
{
public:
    virtual ~ITypeSatellite() = default;
    virtual std::string get_name() = 0;
};

/**
 * A system that manages the movement of multiple satellites, centered around a
 * single satellite
 */
class ISystemTrajectory
{
public:
    virtual ~ISystemTrajectory() = default;
    virtual void update() = 0;
    virtual void add(Satellite sat) = 0;
    virtual void remove(Satellite sat) = 0;
    virtual Satellite get_center() = 0;


private:
    //std::vector<Satellite> k;
};

/**
 * CRTP to implement common states and functions needed by most Trajectories
 */
template <typename TRAJECTORY_T>
class CommonTrajectory : public ISystemTrajectory
{
    friend TRAJECTORY_T;
public:

    CommonTrajectory(Universe& universe, Satellite center) :
            m_center(center),
            m_universe(universe) { };
    ~CommonTrajectory() = default;

    virtual void add(Satellite sat);
    virtual void remove(Satellite sat);
    Satellite get_center() { return m_center; };

private:

    Universe& m_universe;
    Satellite m_center;
    std::vector<Satellite> m_satellites;
};

template <typename TRAJECTORY_T>
void CommonTrajectory<TRAJECTORY_T>::add(Satellite sat)
{
    auto posTraj = m_universe.get_reg().get<ucomp::PositionTrajectory>(sat);

    if (posTraj.m_trajectory)
    {
        return; // already part of trajectory
    }

    posTraj.m_index = m_satellites.size();
    m_satellites.push_back(sat);
}

template <typename TRAJECTORY_T>
void CommonTrajectory<TRAJECTORY_T>::remove(Satellite sat)
{
    auto posTraj = m_universe.get_reg().get<ucomp::PositionTrajectory>(sat);

    if (posTraj.m_trajectory != this)
    {
        return; // not associated with this satellite
    }

    m_satellites.erase(m_satellites.begin() + posTraj.m_index);

    // disassociate with this satellite
    posTraj.m_trajectory = nullptr;
}

}
