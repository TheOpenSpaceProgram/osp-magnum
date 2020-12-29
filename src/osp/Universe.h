/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
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

//using Satellite = entt::entity;
//ENTT_OPAQUE_TYPE(Satellite, uint32_t);
enum class Satellite: entt::id_type {};


using MapSatType = std::map<std::string, std::unique_ptr<ITypeSatellite>>;

//typedef uint64_t Coordinate[3];
//typedef Math::Vector3<int64_t> Coordinate;

/**
 * A model of deep space. This class stores the data of astronomical objects
 * represented in the universe, known as Satellites. Planets, stars, comets,
 * vehicles, etc... are all Satellites.
 *
 * This class uses EnTT ECS, where Satellites are ECS entities. Components are
 * structs prefixed with UComp.
 *
 * Satellites can have types that determine which components they have, see
 * ITypeSatellite. These types are registered at runtime.
 *
 * Positions are stored in 64-bit unsigned int vectors in UCompTransformTraj.
 * 1024 units = 1 meter
 *
 * Moving or any kind of iteration over Satellites, such as Orbits, are handled
 * by Trajectory classes, see ISystemTrajectory.
 *
 * Example of usage:
 * https://github.com/TheOpenSpaceProgram/osp-magnum/wiki/Cpp:-How-to-setup-a-Solar-System
 *
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
     * Create a Satellite with default components, and adds it to the universe.
     * This Satellite will not be assigned a type.
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
     * @param referenceFrame [in]
     * @param target [in]
     * @return relative position of target in SpaceInt vector
     */
    Vector3s sat_calc_pos(Satellite referenceFrame, Satellite target) const;

    /**
     * Calculate position between two satellites.
     * @param referenceFrame [in]
     * @param target [in]
     * @return relative position of target in meters
     */
    Vector3 sat_calc_pos_meters(Satellite referenceFrame, Satellite target) const;

    /**
     * Register an ITypeSatellite so the universe can recognize that this type
     * of Satellite can exist.
     * @tparam TYPESAT_T Unique ITypeSatellite to register
     * @tparam ARGS_T Arguments to forward to TYPESAT_T's constructor
     * @return reference to new TYPESAT_T just created
     */
    template<typename TYPESAT_T, typename ... ARGS_T>
    TYPESAT_T& type_register(ARGS_T&& ... args);

    /**
     * Tries to locate an element in the map of registered Satellites by name.
     * @param name [in] Name of ITypeSatellite
     * @return Map iterator directly from std::map::find()
     */
    MapSatType::iterator sat_type_find(const std::string &name)
    {
        return m_satTypes.find(name);
    }

    /**
     * Tries to locate an element in the map of registered Satellite by type,
     * and casts it to SATTYPE_T
     * @tparam SATTYPE_T Type of ITypeSatellite to find
     * @return Reference to satellite type found
     */
    template<typename SATTYPE_T>
    SATTYPE_T& sat_type_find();

    /**
     * Create a Trajectory, and add it to the universe.
     * @tparam TRAJECTORY_T Type of Trajectory to construct
     * @return Reference to new trajectory just created
     */
    template<typename TRAJECTORY_T, typename ... ARGS_T>
    TRAJECTORY_T& trajectory_create(ARGS_T&& ... args);

    constexpr entt::basic_registry<Satellite>& get_reg() { return m_registry; }
    constexpr const entt::basic_registry<Satellite>& get_reg() const
    { return m_registry; }

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

template<typename SATTYPE_T>
SATTYPE_T& Universe::sat_type_find()
{
    MapSatType::iterator it = sat_type_find(SATTYPE_T::smc_name);
    assert(it != m_satTypes.end());
    return static_cast<SATTYPE_T&>(*it->second);
}

template<typename TRAJECTORY_T, typename ... ARGS_T>
TRAJECTORY_T& Universe::trajectory_create(ARGS_T&& ... args)
{
    std::unique_ptr<TRAJECTORY_T> newTraj
                = std::make_unique<TRAJECTORY_T>(std::forward<ARGS_T>(args)...);
    return static_cast<TRAJECTORY_T&>(*m_trajectories.emplace_back(std::move(newTraj)));
}


// default ECS components needed for the universe


struct UCompTransformTraj
{
    // Relative to m_parent
    Vector3s m_position;

    // In 'global' space
    Quaternion m_rotation;

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
    auto &posTraj = m_universe.get_reg().get<UCompTransformTraj>(sat);

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
    auto &posTraj = m_universe.get_reg().get<UCompTransformTraj>(sat);

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
