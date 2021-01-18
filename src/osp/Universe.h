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

#include <cstdint>
#include <tuple>
#include <vector>


namespace osp::universe
{

class ISystemTrajectory;

enum class Satellite : entt::id_type {};

enum class TypeSatIndex : uint16_t
{
    Invalid = std::numeric_limits<uint16_t>::max()
};

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
    using MapTypeSats_t = std::map<std::string_view, TypeSatIndex, std::less<>>;

public:

    using Registry_t = entt::basic_registry<Satellite>;

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

    bool sat_try_set_type(Satellite sat, TypeSatIndex type);

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
    template<typename TYPESAT_T>
    TypeSatIndex sat_type_register();

    /**
     * @return Names of registered satellites. Access using a TypeSatIndex
     */
    constexpr std::vector<std::string_view> const& sat_type_get_names() const
    noexcept { return m_typeSatNames; }

    /**
     * Find index of a registered satellite by name
     * @param name [in] Name of satellite to find
     * @return Index of type, TypeSatIndex::Invalid if not found.
     */
    TypeSatIndex sat_type_find_index(std::string_view name);

    /**
     * Find index of a registered satellite by type. The type should contain a
     * static member string named smc_name
     * @tparam SATTYPE_T Satellite type containing smc_name
     * @return Index of type, TypeSatIndex::Invalid if not found.
     */
    template<typename SATTYPE_T>
    TypeSatIndex sat_type_find_index()
    { return sat_type_find_index(SATTYPE_T::smc_name); }

    /**
     * Attempt to set the type of a Satellite. If a type is already set, then
     * the type will not be set successfully.
     * @param sat  [in] Satellite to set type of
     * @param type [in] Type to set to
     * @return True if type is set succesfully, or else false
     */
    bool sat_type_try_set(Satellite sat, TypeSatIndex type);

    /**
     * Create a Trajectory, and add it to the universe.
     * @tparam TRAJECTORY_T Type of Trajectory to construct
     * @return Reference to new trajectory just created
     */
    template<typename TRAJECTORY_T, typename ... ARGS_T>
    TRAJECTORY_T& trajectory_create(ARGS_T&& ... args);

    constexpr Registry_t& get_reg() noexcept { return m_registry; }
    constexpr const Registry_t& get_reg() const noexcept
    { return m_registry; }

private:

    Satellite m_root;

    std::vector<std::unique_ptr<ISystemTrajectory>> m_trajectories;

    MapTypeSats_t m_typeSatIndices;
    std::vector<std::string_view> m_typeSatNames;

    Registry_t m_registry;

};

template<typename TYPESAT_T>
TypeSatIndex Universe::sat_type_register()
{
    TypeSatIndex newIndex = static_cast<TypeSatIndex>(m_typeSatIndices.size());
    auto const &[it, success] = m_typeSatIndices.emplace(TYPESAT_T::smc_name,
                                                         newIndex);
    if (success)
    {
        m_typeSatNames.emplace_back(TYPESAT_T::smc_name);
        return newIndex;
    }
    return TypeSatIndex::Invalid;
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

struct UCompVelocity
{
    Vector3 m_velocity;
};

struct UCompType
{
    TypeSatIndex m_type{TypeSatIndex::Invalid};
};

// TODO: move to different files and de-OOPify trajectories too

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
