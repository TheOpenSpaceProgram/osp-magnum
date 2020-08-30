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

using MapSatType = std::map<std::string, std::unique_ptr<ITypeSatellite>>;

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

    /**
     * @return an null satellite
     */
    static constexpr Satellite sat_null() { return entt::null; };

    /**
     * Create a satellite with default components, and adds it to the universe
     * @return The new Satellite just created
     */
    Satellite sat_create();

    /**
     * @return Root Satellite of the universe. This will never change.
     */
    constexpr Satellite sat_root() { return m_root; };

    /**
     * Remove a satellite
     */
    void sat_remove(Satellite sat);

    /**
     * Calculate position between two satellites.
     * @param referenceFrame
     * @param target
     * @return relative position of target in SpaceInt vector
     */
    Vector3s sat_calc_pos(Satellite referenceFrame, Satellite target);

    /**
     * Calculate position between two satellites.
     * @param referenceFrame
     * @param target
     * @return relative position of target in meters
     */
    Vector3 sat_calc_pos_meters(Satellite referenceFrame, Satellite target);

    /**
     * Register an ITypeSatellite so the universe can recognize that this type
     * of Satellite can exist.
     * @tparam TYPESAT_T Unique ITypeSatellite to register
     * @tparam ARGS_T Arguments to forward to TYPESAT_T's constructor
     * @return reference to new TYPESAT_T just created
     */
    template<typename TYPESAT_T, typename ... ARGS_T>
    TYPESAT_T& type_register(ARGS_T&& ... args);

    MapSatType::iterator sat_type_find(const std::string &name)
    {
        return m_satTypes.find(name);
    }

    /**
     *
     */
    template<typename TRAJECTORY_T, typename ... ARGS_T>
    TRAJECTORY_T& trajectory_create(ARGS_T&& ... args);

    constexpr entt::basic_registry<Satellite>& get_reg() { return m_registry; }

private:

    //std::vector<Package> m_packages;

    //std::vector<Satellite> m_satellites;
    //std::vector<PrototypePart> m_prototypes;

    Satellite m_root;

    std::vector<std::unique_ptr<ISystemTrajectory>> m_trajectories;

    MapSatType m_satTypes;

    entt::basic_registry<Satellite> m_registry;

};


template<typename TYPESAT_T, typename ... ARGS_T>
TYPESAT_T& Universe::type_register(ARGS_T&& ... args)
{
    std::unique_ptr<TYPESAT_T> newType
            = std::make_unique<TYPESAT_T>(std::forward<ARGS_T>(args)...);
    TYPESAT_T& toReturn = *newType;
    m_satTypes[newType->get_name()] = std::move(newType);
    return toReturn;
}

template<typename TRAJECTORY_T, typename ... ARGS_T>
TRAJECTORY_T& Universe::trajectory_create(ARGS_T&& ... args)
{
    std::unique_ptr<TRAJECTORY_T> newTraj = std::make_unique<TRAJECTORY_T>(std::forward<ARGS_T>(args)...);
    TRAJECTORY_T& toReturn = *newTraj;
    m_trajectories.push_back(std::move(newTraj)); // this invalidates newTraj
    return toReturn;
}


// default ECS components needed for the universe


struct UCompPositionTrajectory
{
    // Position is relative to m_parent
    Vector3s m_position;

    Satellite m_parent; // Set only by trajectory

    ISystemTrajectory *m_trajectory{nullptr}; // get rid of this some day
    unsigned m_index; // index in trajectory's vector

    // set this to true when you modify something the trajectory might use
    bool m_dirty{true};

    // move this somewhere else eventually
    std::string m_name;
};

//struct ACompActivated
//{
//    // Index to [probably an active area] when activated
//    Satellite m_area;
//}

struct UCompVelocity
{
    Vector3 m_velocity;
};

struct UCompType
{
    // maybe use an iterator
    ITypeSatellite* m_type{nullptr};
};

struct UCompActivatable
{
    // put something here some day
    int m_dummy;
};


/**
 * A specific type of Satellite. A planet, star, vehicle, etc...
 */
class ITypeSatellite
{
public:
    virtual ~ITypeSatellite() = default;
    virtual std::string get_name() = 0;

    virtual void add(Satellite sat) = 0;
    virtual void remove(Satellite sat) = 0;
};


/**
 * CRTP to implement common functions of TypeSatellites. add and remove
 * functions will add or remove the component UCOMP_T
 */
template<typename TYPESAT_T, typename ... UCOMP_T>
class CommonTypeSat : public ITypeSatellite
{

public:


    CommonTypeSat(Universe& universe) : m_universe(universe) {}
    ~CommonTypeSat() = default;

    virtual void add(Satellite sat) { add_get_ucomp(sat); };
    virtual void remove(Satellite sat) override;

    std::tuple<UCOMP_T& ...> add_get_ucomp_all(Satellite sat);
    auto& add_get_ucomp(Satellite sat);

    constexpr Universe& get_universe() { return m_universe; }

private:

    Universe& m_universe;
};

template<typename TYPESAT_T, typename ... UCOMP_T>
std::tuple<UCOMP_T& ...> CommonTypeSat<TYPESAT_T, UCOMP_T ...>::add_get_ucomp_all(Satellite sat)
{
    auto& type = m_universe.get_reg().get<UCompType>(sat);

    if (type.m_type != nullptr)
    {
        //return ;
    }

    type.m_type = this;
    return std::forward_as_tuple((m_universe.get_reg().emplace<UCOMP_T>(sat)) ...);
}

template<typename TYPESAT_T, typename ... UCOMP_T>
auto& CommonTypeSat<TYPESAT_T, UCOMP_T ...>::add_get_ucomp(Satellite sat)
{
    auto& type = m_universe.get_reg().get<UCompType>(sat);

    if (type.m_type != nullptr)
    {
        //return ;
    }

    type.m_type = this;
    return std::get<0>(std::forward_as_tuple((m_universe.get_reg().emplace<UCOMP_T>(sat)) ...));
}

template<typename TYPESAT_T, typename ... UCOMP_T>
void CommonTypeSat<TYPESAT_T, UCOMP_T ...>::remove(Satellite sat)
{
    auto& type = m_universe.get_reg().get<UCompType>(sat);

    if (type.m_type != this)
    {
        return;
    }

    type.m_type = nullptr;
    m_universe.get_reg().remove<UCOMP_T ...>(sat);
}

using TrajectoryType = entt::id_type;

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
    virtual Satellite get_center() const = 0;

    virtual TrajectoryType get_type() = 0;
    virtual std::string const& get_type_name() = 0;

private:
    //std::vector<Satellite> k;
};

/**
 * Implement some commmon functions for Trajectories
 */
template<typename TRAJECTORY_T>
class CommonTrajectory : public ISystemTrajectory
{
public:

    CommonTrajectory(Universe& universe, Satellite center) :
            m_universe(universe),
            m_center(center) { };
    ~CommonTrajectory() = default;

    virtual void add(Satellite sat) override;
    virtual void remove(Satellite sat) override;

    TrajectoryType get_type() override
    {
        static auto id = entt::type_info<TRAJECTORY_T>::id();
        return id;
    }
    std::string const& get_type_name() override
    {
        //static auto name = entt::type_info<TRAJECTORY_T>::name();
        static std::string name = typeid(TRAJECTORY_T).name();
        return name;
    }

    Satellite get_center() const override { return m_center; };

    constexpr Universe& get_universe() const { return m_universe; }
    constexpr std::vector<Satellite>& get_satellites() { return m_satellites; }

private:

    Universe& m_universe;
    Satellite m_center;
    std::vector<Satellite> m_satellites;
};

template<typename TRAJECTORY_T>
void CommonTrajectory<TRAJECTORY_T>::add(Satellite sat)
{
    auto &posTraj = m_universe.get_reg().get<UCompPositionTrajectory>(sat);

    if (posTraj.m_trajectory)
    {
        return; // already part of trajectory
    }

    posTraj.m_trajectory = this;
    posTraj.m_index = m_satellites.size();
    posTraj.m_parent = m_center;
    m_satellites.push_back(sat);
}

template<typename TRAJECTORY_T>
void CommonTrajectory<TRAJECTORY_T>::remove(Satellite sat)
{
    auto &posTraj = m_universe.get_reg().get<UCompPositionTrajectory>(sat);

    if (posTraj.m_trajectory != this)
    {
        return; // not associated with this satellite
    }

    m_satellites.erase(m_satellites.begin() + posTraj.m_index);

    // disassociate with this satellite
    posTraj.m_parent = entt::null;
    posTraj.m_trajectory = nullptr;
}

}
