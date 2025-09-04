/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

 /**
 * @file
 * @brief Core Universe. see docs/universe.md
 */
#pragma once

#include "universetypes.h"

#include "../core/math_types.h"
#include "../core/buffer_format.h"
#include "../core/keyed_vector.h"
#include "../core/copymove_macros.h"

#include <longeron/id_management/bitview_id_set.hpp>
#include <longeron/id_management/id_set_stl.hpp>
#include <longeron/id_management/registry_stl.hpp>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StridedArrayView.h>

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <memory>
#include <set>
#include <unordered_map>

namespace osp::universe
{

template <typename T, std::size_t N>
using BufferAttribArray_t = std::array<BufAttribFormat<T>, N>;

template<typename ID_T, std::size_t SIZE>
using StaticIdSet_t = lgrn::BitViewIdSet<lgrn::BitView<std::array<std::uint64_t, SIZE/64>>, ID_T>;

//-----------------------------------------------------------------------------

// Coordinate spaces

struct CospaceTransform
{
    osp::Quaterniond    rotation;
    Vector3g            position;       ///< using parent cospace's precision
    Vector3             velocity;
    std::int64_t        timeBehindBy{};
    int                 precision{10};  ///< for child satellites and child cospaces. 1m = 2^precision

    SatelliteId parentSat;

    /// Rotate with parent satellite/coordspace.
    /// Set true for a planet surface coordspace so landed satellites rotate with the planet.
    bool inheritRotation{false};
};

struct UCtxCoordSpaces
{
    using TreePos_t = std::uint32_t;

    void resize()
    {
        auto cospaceCapacity = ids.capacity();
        treeposOf  .resize(cospaceCapacity);
        transformOf.resize(cospaceCapacity);
    }

    void insert(CoSpaceId parent, CoSpaceId addme)
    {
        if (parent.has_value())
        {
            TreePos_t const parentPos = treeposOf[parent];
            TreePos_t const addmePos  = parentPos + 1;

            treeToId       .insert(treeToId.begin()        + addmePos, addme);
            treeDescendants.insert(treeDescendants.begin() + addmePos, 0);

            // Calls to insert(..) above pushed all positions right of addmePos right by 1.
            // Tree positions of cospace IDs need to be updated.
            for (TreePos_t pos = addmePos; pos < treeDescendants.size(); ++pos)
            {
                treeposOf[treeToId[pos]] = pos;
            }

            // Starting from the root, walk down parent-child chain and increment descendant count
            // of all ascendants of addmePos
            TreePos_t ascendant = 0; // root
            while (ascendant != addmePos)
            {
                ++ treeDescendants[ascendant];

                // addmePos belongs to which child?
                auto        const childLast = ascendant + 1 + treeDescendants[ascendant];
                TreePos_t         child     = ascendant + 1;

                while (child != childLast)
                {
                    TreePos_t const nextChild = child + 1 + treeDescendants[child];

                    if (addmePos <= nextChild) // is 'addmePos' a descendent-of or is 'child'?
                    {
                        ascendant = child;
                        break;
                    }
                    else
                    {
                       child = nextChild;
                    }
                }
            }
        }
        else
        {
            LGRN_ASSERT( ! treeToId[0].has_value() );
            treeToId       .resize(1);
            treeDescendants.resize(1);
            treeToId[0]         = addme;
            treeDescendants[0]  = 0;
            treeposOf[addme]    = 0;
        }
    }

    lgrn::IdRegistryStl<CoSpaceId>              ids;
    lgrn::IdRefCount<CoSpaceId>                 refCounts;

    osp::KeyedVec<CoSpaceId, CospaceTransform>  transformOf;
    osp::KeyedVec<CoSpaceId, TreePos_t>         treeposOf;

    osp::KeyedVec<TreePos_t, CoSpaceId>         treeToId{{lgrn::id_null<CoSpaceId>()}};
    osp::KeyedVec<TreePos_t, std::uint32_t>     treeDescendants{std::initializer_list<std::uint32_t>{0}};
};

//-----------------------------------------------------------------------------

// Components

struct ComponentTypeInfo
{
    // TODO: maybe put specific data type info or hash here
    std::string name;
    std::size_t size;
};

struct DefaultComponents
{
    // 19 of these btw
    ComponentTypeId satId;
    ComponentTypeId posX;
    ComponentTypeId posY;
    ComponentTypeId posZ;
    ComponentTypeId velX;
    ComponentTypeId velY;
    ComponentTypeId velZ;
    ComponentTypeId velXd;
    ComponentTypeId velYd;
    ComponentTypeId velZd;
    ComponentTypeId accelX;
    ComponentTypeId accelY;
    ComponentTypeId accelZ;
    ComponentTypeId rotX;
    ComponentTypeId rotY;
    ComponentTypeId rotZ;
    ComponentTypeId rotW;
    ComponentTypeId radius;
    ComponentTypeId surface;
};

struct UCtxComponentTypes
{
    UCtxComponentTypes()
    {
        auto const id = [this] () { return ids.create(); };

        defaults = { id(), id(), id(), id(), id(), id(), id(), id(), id(), id(),
                     id(), id(), id(), id(), id(), id(), id(), id(), id() };

        info.resize(ids.capacity());
        info[defaults.satId]    = {"SatelliteID", sizeof(SatelliteId)};
        info[defaults.posX]     = {"PosX",      sizeof(spaceint_t)};
        info[defaults.posY]     = {"PosY",      sizeof(spaceint_t)};
        info[defaults.posZ]     = {"PosZ",      sizeof(spaceint_t)};
        info[defaults.velX]     = {"VelX",      sizeof(float)};
        info[defaults.velY]     = {"VelY",      sizeof(float)};
        info[defaults.velZ]     = {"VelZ",      sizeof(float)};
        info[defaults.velXd]    = {"VelXd",     sizeof(double)};
        info[defaults.velYd]    = {"VelYd",     sizeof(double)};
        info[defaults.velZd]    = {"VelZd",     sizeof(double)};
        info[defaults.accelX]   = {"AccelX",    sizeof(float)};
        info[defaults.accelY]   = {"AccelY",    sizeof(float)};
        info[defaults.accelZ]   = {"AccelZ",    sizeof(float)};
        info[defaults.rotX]     = {"RotX",      sizeof(float)};
        info[defaults.rotY]     = {"RotY",      sizeof(float)};
        info[defaults.rotZ]     = {"RotZ",      sizeof(float)};
        info[defaults.rotW]     = {"RotW",      sizeof(float)};
        info[defaults.radius]   = {"Radius",    sizeof(float)}; // not used anywhere
        info[defaults.surface]  = {"Surface",   sizeof(float)}; // not used anywhere
    }

    lgrn::IdRegistryStl<ComponentTypeId>            ids;
    KeyedVec<ComponentTypeId, ComponentTypeInfo>    info;
    DefaultComponents                               defaults;
};



//-----------------------------------------------------------------------------

// Data Accessors

struct SatIdIndexPair
{
    SatelliteId     sat;
    std::uint32_t   accessorIdx;
};

struct DataAccessor
{
    enum class IterationMethod : std::uint8_t { Dense, SkipNullSatellites, IndexOnly };

    struct Component
    {
        std::byte     const *pos    {nullptr};
        std::ptrdiff_t      stride  {0};
        //ComponentTypeId     type    {};
    };

    // TODO optimization: tiny map with tiny elements. maybe switch away from unordered_map
    using CompMap_t = std::unordered_map<ComponentTypeId, Component>;

    template<std::size_t SIZE>
    class MultiComponentIterator
    {
        friend struct DataAccessor;

    public:

        void next() noexcept
        {
            if (remaining != 0)
            {
                for (std::size_t i = 0; i < SIZE; ++i)
                {
                    pos[i] += stride[i];
                }
                --remaining;
            }
            else
            {
                std::fill(   pos.begin(),    pos.end(), nullptr);
                std::fill(stride.begin(), stride.end(), 0);
            }
        }

        [[nodiscard]] bool has(std::size_t index) const noexcept
        {
            return pos[index] != nullptr;
        }

        template<typename T>
        [[nodiscard]] T get(std::size_t index) const noexcept
        {
            T out;
            std::memcpy(&out, pos[index], sizeof(T));
            return out;
        }

    private:

        std::array<std::byte const*, SIZE>  pos{};
        std::array<std::ptrdiff_t, SIZE>    stride{};
        std::size_t remaining{};
    };

    template<std::size_t SIZE>
    [[nodiscard]] MultiComponentIterator<SIZE> iterate(std::array<ComponentTypeId, SIZE> comps) const
    {
        MultiComponentIterator<SIZE> out;

        auto const& itNotFound = components.cend();
        for (std::size_t i = 0; i < SIZE; ++i)
        {
            auto it = components.find(comps[i]);
            if (it != itNotFound)
            {
                out.pos[i]    = it->second.pos;
                out.stride[i] = it->second.stride;
            }
            else
            {
                out.pos[i]    = nullptr;
                out.stride[i] = 0u;
            }
        }
        out.remaining = count;

        return out;
    }

    //std::vector<SatIdIndexPair> todo;
    std::string         debugName;
    CompMap_t           components;
    std::uint64_t       time{0};
    std::size_t         count{0};
    SimulationId        owner;
    CoSpaceId           cospace;
    IterationMethod     iterMethod  { IterationMethod::SkipNullSatellites };
};

template <typename T>
inline DataAccessor::Component make_comp(T const* ptr, std::ptrdiff_t stride)
{
    return {reinterpret_cast<std::byte const*>(ptr), stride};
}

struct UCtxDataAccessors
{
    using AccessorVec_t = std::vector<DataAccessorId>;

    lgrn::IdRegistryStl<DataAccessorId>         ids;
    osp::KeyedVec<DataAccessorId, DataAccessor> instances;
    std::vector<DataAccessorId>                 accessorDelete;
    osp::KeyedVec<CoSpaceId, AccessorVec_t>     accessorsOfCospace;
};

/**
 * @brief Allows marking satellites in a DataAccessor as stolen/removed from the accessor.
 *
 * This is useful for transferring satellite data across accessors.
 * separated out as UCtxDataAccessors are expected to be read as const most of the time, and this is mutable
 *
 * stolenSats.of[accessorId]
 */
struct UCtxStolenSatellites
{
    struct OfAccessor
    {
        std::set<SatelliteId>   sats;
        bool                    allStolen = false;
        bool                    dirty = false;

        bool has(SatelliteId satId) const noexcept
        {
            return allStolen || (dirty && sats.contains(satId));
        }
    };

    osp::KeyedVec<DataAccessorId, OfAccessor> of;
};

//-----------------------------------------------------------------------------

// Data Sources

using ComponentTypeIdSet_t = StaticIdSet_t<ComponentTypeId, 128>;

/**
 * @brief Determines what components a satellite has and which data accessors it uses.
 *
 *
 * ComponentTypeIds contained in every entries[n].components must be unique.
 *
 * if entries[i].components contains X, then another entry entries[j].components cannot contain X
 */
struct DataSource
{

    struct Entry
    {
        ComponentTypeIdSet_t    components{}; // zero init;
        DataAccessorId          accessor{}; ///< can't be null
        std::uint32_t           _padding{0};

        friend auto operator<=>(Entry const& lhs, Entry const& rhs)
        {
            if (lhs.accessor.value != rhs.accessor.value)
            {
                return lhs.accessor.value <=> rhs.accessor.value;
            }
            else
            {
                return std::memcmp(&lhs.components, &rhs.components, sizeof(ComponentTypeIdSet_t)) <=> 0;
            }
        }
    };
    static_assert(sizeof(ComponentTypeIdSet_t) == 16);
    static_assert(sizeof(DataAccessorId) == 4);
    static_assert(sizeof(Entry) == 24);

    void sort()
    {
        std::sort(entries.begin(), entries.end());
    }

    std::vector<Entry> entries;
};

using DataSourceOwner_t = lgrn::IdRefCount<DataSourceId>::Owner_t;

struct DataSourceChange
{
    std::vector<SatelliteId>            satsAffected;
    ComponentTypeIdSet_t                components;
    DataAccessorId                      accessor;
};

struct UCtxDataSources
{
    lgrn::IdRegistryStl<DataSourceId>               ids;
    lgrn::IdRefCount<DataSourceId>                  refCounts;
    osp::KeyedVec<DataSourceId, DataSource>         instances;
    osp::KeyedVec<SatelliteId, DataSourceOwner_t>   datasrcOf;
    std::vector<DataSourceChange>                   changes;

    DataSourceId find_datasource(DataSource const& query)
    {
        std::size_t const size = query.entries.size();
        for (DataSourceId const dataSrcId : ids)
        {
            DataSource const& candidate = instances[dataSrcId];
            if (   candidate.entries.size() == size
                && std::memcmp(candidate.entries.data(), query.entries.data(), size * sizeof(DataSource::Entry)) == 0)
            {
                return dataSrcId;
            }
        }
        return {};
    }

};

//-----------------------------------------------------------------------------

// Satellite instances

struct UCtxSatellites
{
    UCtxSatellites() = default;
    OSP_MOVE_ONLY_CTOR_ASSIGN(UCtxSatellites);

    lgrn::IdRegistryStl<SatelliteId>                ids;

};

//-----------------------------------------------------------------------------

// Simulations

struct Simulation
{
    /// in milliseconds. whoever controls time adds to timeBehindBy. simulation update logic reads
    /// this, checks if its behind enough to justify updating (i.e. threshold for time interval),
    /// then updates data buffers and subtracts passed time from timeBehindBy
    std::int64_t timeBehindBy{};
};

struct UCtxSimulations
{
    lgrn::IdRegistryStl<SimulationId>       ids;
    osp::KeyedVec<SimulationId, Simulation> simulationOf;
};


//-----------------------------------------------------------------------------

// Intakes

struct Intake
{
    ComponentTypeIdSet_t                components;
    SimulationId                        owner;
    CoSpaceId                           cospace;
};

struct UCtxIntakes
{
    lgrn::IdRegistryStl<IntakeId>           ids;
    osp::KeyedVec<IntakeId, Intake>         instances;

    IntakeId find_intake_at(CoSpaceId cospaceId, ComponentTypeIdSet_t const& comps) const
    {
        for (IntakeId const intakeId : ids)
        {
            Intake const &intake = instances[intakeId];

            if (std::memcmp(&comps, &intake.components, sizeof(ComponentTypeIdSet_t)) == 0)
            {
                return intakeId;
            }
        }
        return {};
    }

    IntakeId make_intake(SimulationId owner, CoSpaceId cospaceId, ComponentTypeIdSet_t components)
    {
        IntakeId id = ids.create();
        instances.resize(ids.size());

        instances[id] = Intake{
            .components = std::move(components),
            .owner      = owner,
            .cospace    = cospaceId
        };

        return id;
    }
};

//-----------------------------------------------------------------------------

struct MidTransfer
{
    std::unique_ptr<std::byte const[]> data{nullptr};
    DataAccessorId accessor;
    IntakeId target{};
};

struct TransferRequest
{
    std::unique_ptr<std::byte const[]> data{nullptr};
    std::size_t count;
    std::int64_t time;
    IntakeId target{};
};

/**
 * @brief Intermediate buffers to help transfer satellites across two simulations
 *
 * Simulations update at different rates and may not be in sync. If a fast-updating simulation
 * pushes a satellite into a slow-updating simulation, there is a brief moment the satellite is
 * 'mid-transfer', waiting for the slow-updating simulation to update.
 *
 * Transfer Buffer stops mid-transfer satellites from popping out of existance for a short time,
 * by storing a mid-transfer satellite data in a buffer accessible as its own Simulation.
 */
struct UCtxTransferBuffers
{
    UCtxTransferBuffers(SimulationId const id) : simId{id} {}
    OSP_MOVE_ONLY_CTOR_ASSIGN(UCtxTransferBuffers)

    SimulationId simId{};

    osp::KeyedVec<SimulationId, std::vector<MidTransfer>> midTransfersOf;
    std::vector<SimulationId>    midTransferDelete;

    std::vector<TransferRequest> requests;
    std::vector<DataAccessorId>  requestAccessorIds;
};

//-----------------------------------------------------------------------------

static ComponentTypeIdSet_t component_type_set(std::initializer_list<ComponentTypeId const> typeIds)
{
    ComponentTypeIdSet_t out{};
    out.clear(); // paranoid this might not zero-init, so just clear to zero
    for (ComponentTypeId const typeId : typeIds)
    {
        out.emplace(typeId);
    }
    return out;
}


} // namespace osp::universe
