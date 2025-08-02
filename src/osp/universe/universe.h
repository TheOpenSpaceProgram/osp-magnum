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
#pragma once

#include "universetypes.h"

#include "../core/math_types.h"
#include "../core/buffer_format.h"
#include "../core/keyed_vector.h"
#include "../core/copymove_macros.h"
#include "../core/array_view.h"

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

struct CoSpaceTransform
{
    SatelliteId     parentSat;

    // Position and rotation is relative to m_parent
    // Ignore and use parentSat's position and rotation instead if non-null
    Quaterniond     m_rotation;
    Vector3g        m_position;

    int             m_precision{10}; // 1 meter = 2^m_precision
};

struct UCtxCoordSpaces
{
    using TreePos_t = std::uint32_t;

    lgrn::IdRegistryStl<CoSpaceId>              ids;
    lgrn::IdRefCount<CoSpaceId>                 refCounts;

    osp::KeyedVec<CoSpaceId, CoSpaceTransform>  transforms;

    osp::KeyedVec<TreePos_t, CoSpaceId>         treeToId{{lgrn::id_null<CoSpaceId>()}};
    osp::KeyedVec<TreePos_t, std::uint32_t>     treeDescendants{std::initializer_list<std::uint32_t>{0}};
    osp::KeyedVec<CoSpaceId, CoSpaceId>         idParent;
    osp::KeyedVec<CoSpaceId, TreePos_t>         idToTreePos;
};

//-----------------------------------------------------------------------------

// Components

struct ComponentTypeInfo
{
    std::string name;
    std::size_t size;
};

struct DefaultComponents
{
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
    lgrn::IdRegistryStl<ComponentTypeId>            ids;
    KeyedVec<ComponentTypeId, ComponentTypeInfo>    info;
    DefaultComponents                               defaults;
};


//-----------------------------------------------------------------------------

// Data Accessors

struct DataAccessor
{
    using SatToIndexMap_t = std::unordered_map<SatelliteId, std::uint32_t>;

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
        friend class DataAccessor;

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
            return *std::bit_cast<T const*>(pos[index]);
        }

    private:

        std::array<std::byte const*, SIZE>  pos;
        std::array<std::ptrdiff_t, SIZE>    stride;
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

    std::string         debugName;
    CompMap_t           components;
    SatToIndexMap_t     satToIndex;
    std::uint64_t       time{0};
    std::size_t         count{0};
    SimulationId        owner;
    CoSpaceId           cospace;
    IterationMethod     iterMethod  { IterationMethod::SkipNullSatellites };
};

struct UCtxDataAccessors
{
    lgrn::IdRegistryStl<DataAccessorId>         ids;
    //lgrn::IdRefCount<DataAccessorId>            refCounts;
    osp::KeyedVec<DataAccessorId, DataAccessor> instances;

    std::vector<DataAccessorId> accessorDelete;
};

/**
 * @brief allow marking satellites in a DataAccessor as stolen, and no longer part of that accessor
 *
 * separated out as UCtxDataAccessors are expected to be read as const most of the time, and this is mutable
 *
 *
 * stolenSats.of[accessorId]
 */
struct UCtxStolenSatellites
{
    struct Accessor
    {
        std::set<SatelliteId>   sats;
        bool                    allStolen = false;
        bool                    dirty = false;
    };

    osp::KeyedVec<DataAccessorId, Accessor> of;
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
        DataAccessorId          accessor{};
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

struct UCtxSatelliteInstances
{
    UCtxSatelliteInstances() = default;
    OSP_MOVE_ONLY_CTOR_ASSIGN(UCtxSatelliteInstances);

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

struct UCtxSimulationsDelete
{

};

//-----------------------------------------------------------------------------

// Intakes

//class ISatelliteInserter
//{
//public:
//    class Contract
//    {
//    public:
//        ~Contract()
//        {
//            LGRN_ASSERT(remaining == 0);
//        }
//    private:
//        std::size_t remaining;
//    };

//    virtual Contract reserve(std::size_t amount) = 0;

//    virtual void add(Contract &rContract, ArrayView<std::byte const> data) = 0;
//};

//struct SatelliteInserter
//{

//    using UserData_t = std::array<std::byte, 32>;


//    ReserveFunc_t reserve{nullptr};

//    using NextFunc_t = ArrayView<std::byte>(*)(, UserData_t&);
//    NextFunc_t next{nullptr};
//};

struct Intake
{
    ComponentTypeIdSet_t                components;
    //std::unique_ptr<ISatelliteInserter> inserter;
    SimulationId                        owner;
    CoSpaceId                           cospace;
};

struct UCtxIntakes
{
    lgrn::IdRegistryStl<IntakeId>           ids;
    osp::KeyedVec<IntakeId, Intake>         instances;


//    static bool components_match(lgrn::IdSetStl<ComponentTypeId> const& lhs, lgrn::IdSetStl<ComponentTypeId> const& rhs)
//    {
//        // pretty much just std::equal

//        auto       lhsIt     = lhs.begin();
//        auto const lhsItLast = lhs.end();
//        auto       rhsIt     = rhs.begin();
//        auto const rhsItLast = rhs.end();

//        while (true)
//        {
//            bool const lhsEnded = lhsIt == lhsItLast;
//            bool const rhsEnded = rhsIt == rhsItLast;

//            if ( ! lhsEnded && ! rhsEnded )
//            {
//                if (*lhsIt != *rhsIt)
//                {
//                    return false;
//                }
//                else
//                {
//                    ++lhsIt;
//                    ++rhsIt;
//                }
//            }
//            else if ( lhsEnded != rhsEnded )
//            {
//                return false;
//            }
//            else if ( lhsEnded && rhsEnded )
//            {
//                return true;
//            }
//            // else { unreachable }
//        }

//        return true;
//    }

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

// (cospace, components) -> intake ID




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
 * @brief intermediate buffers to help transfer satellites across different simulations
 *
 * simulations update at different rates and potentially not in sync. If a fast-updating simulation
 * pushes a satellite into a slow-updating simulation, there is a brief moment the satellite is
 * mid-transfer, waiting for the slow-updating simulation to update.
 *
 * Transfer Buffer stops mid-transfer satellites from popping out of existance for a short time,
 * by storing a mid-transfer satellite data in a buffer accessible through DataAccessors
 *
 *
 *
 */
struct UCtxTransferBuffers
{
    UCtxTransferBuffers() = default;
    OSP_MOVE_ONLY_CTOR_ASSIGN(UCtxTransferBuffers)
//    class Inserter : public ISatelliteInserter
//    {
//    public:
//        Contract reserve(std::size_t amount) override
//        {
//        }
//        void add(Contract &rContract, ArrayView<std::byte const> data) override
//        {
//        }
//    };

    SimulationId simId{};

    osp::KeyedVec<SimulationId, std::vector<MidTransfer>> midTransfersOf;
    std::vector<SimulationId>    midTransferDelete;

    std::vector<TransferRequest> requests;
    std::vector<DataAccessorId>  requestAccessorIds;
};



//class SimpleSatelliteBuffer
//{
//public:

//    //SatelliteInserter make_inserter();

//private:
//    DataAccessorId                  m_accessor;
//    std::vector<ComponentTypeId>    m_components;

//    /// Total of ComponentTypeInfo::size of all component types in m_components
//    std::size_t                     m_totalComponentSize;

//    ///
//    std::unique_ptr<std::byte[]>    m_data;
//};





//-----------------------------------------------------------------------------

//struct UCtxTime
//{
//    std::uint64_t time;
//};

//struct Universe
//{
//    Universe() = default;
//    OSP_MOVE_ONLY_CTOR_ASSIGN(Universe);

//    UniverseSimulations     simulations;
//    UniverseCoordSpaces     coords;
//    UniverseComponentTypes  components;
//    UniverseDataAccessors   accessors;
//    UniverseDataSources     sources;
//    UniverseSatelliteSets   satsets;
//};

//

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


//-----------------------------------------------------------------------------

template<typename ID_T, std::size_t COUNT>
using FixedIdReg_t = lgrn::BitViewIdRegistry<lgrn::BitView<std::array<std::uint64_t, lgrn::div_ceil(COUNT, 64u)>>, ID_T>;

/**
 * @brief all created IDs must be destroyed or else this will assert
 */
template <typename ID_T>
class NoLeakIdRegistry : private lgrn::IdRegistryStl<ID_T>
{
    using Base_t = lgrn::IdRegistryStl<ID_T>;
public:
    using Owner_t = lgrn::IdOwner<ID_T, NoLeakIdRegistry>;

    using Base_t::Base_t;
    using Base_t::begin;
    using Base_t::bitview;
    using Base_t::capacity;
    using Base_t::end;
    using Base_t::exists;
    using Base_t::reserve;
    using Base_t::size;

    ~NoLeakIdRegistry()
    {
        std::size_t const remainingSubscribers = Base_t::size();
        LGRN_ASSERTMV(remainingSubscribers == 0,
                      "All IDs from NoLeakIdRegistry must be removed before NoLeakIdRegistry is destructed",
                      remainingSubscribers,
                      Base_t::bitview().ints()[0]);
    }

    Owner_t create()
    {
        Owner_t out;
        out.m_id = Base_t::create();
        return out;
    }

    void remove(Owner_t&& owner)
    {
        if (owner.m_id.has_value())
        {
            LGRN_ASSERT(Base_t::exists(owner.m_id));
            Base_t::remove(owner.m_id);

            [[maybe_unused]] Owner_t moved = std::move(owner);
            moved.m_id = {};
            // destruct 'moved'
        }
    }
};

template <typename T, typename MSG_T, typename SUBSCRIPTION_INFO_T>
class SPMC
{
    struct ConsumerIdDummy {};

    using Mailbox_t = std::vector<MSG_T>;

    struct ConsumerData
    {
        SUBSCRIPTION_INFO_T subInfo;
        Mailbox_t mailbox;
    };

public:
    using ConsumerId      = osp::StrongId<std::uint32_t, SPMC::ConsumerIdDummy>;
    using ConsumerOwner_t = typename NoLeakIdRegistry<ConsumerId>::Owner_t;

    // TODO: some kind of system to enforce that there's only one producer
    //struct ProducerToken
    //{
    //    OSP_MOVE_ONLY_CTOR_ASSIGN_CONSTEXPR_NOEXCEPT(ProducerToken);
    //private:
    //    ProducerToken() {};
    //};

    template<typename FUNC_T> requires
            ( requires(FUNC_T &&func, SUBSCRIPTION_INFO_T const& info, Mailbox_t &rMailbox)
              { func(info, rMailbox); } )
    void send(FUNC_T &&func)
    {
        for (ConsumerId const id : m_consumers)
        {
            ConsumerData &rData = m_data[id];
            SUBSCRIPTION_INFO_T const& info = rData.subInfo;
            func(info, rData.mailbox);
        }
    }

    ConsumerOwner_t subscribe()
    {
        ConsumerOwner_t out = m_consumers.create();
        return out;
    }

    void unsubscribe(ConsumerOwner_t&& owner)
    {
        m_consumers.destroy(owner);
    }

private:
    NoLeakIdRegistry<ConsumerId> m_consumers;
    osp::KeyedVec<ConsumerId, ConsumerData> m_data;
};


struct SceneFrame : CoSpaceTransform
{
    Vector3g m_scenePosition;
};


/**
 * @brief Get StridedArrayView1Ds from all components of a SatVectorDesc
 *
 * @param strideDescArray   [in] Array of stride description for data in rData
 * @param rData             [in] unsigned char* Satellite data, optionally const
 * @param satCount          [in] Range of valid satellites
 *
 * @return std::array of views, intended to work with structured bindings
 */
template <typename T, std::size_t DIMENSIONS_T, typename DATA_T, std::size_t ... COUNT_T>
constexpr auto sat_views(
        BufferAttribArray_t<T, DIMENSIONS_T> const& strideDescArray,
        DATA_T &rData,
        std::size_t satCount) noexcept
{
    // Recursive call to make COUNT_T a range of numbers from 0 to DIMENSIONS_T
    // This is unpacked into strideDescArray[COUNT_T].view(...) to access components
    constexpr int countUp = sizeof...(COUNT_T);
    if constexpr (countUp != DIMENSIONS_T)
    {
        return sat_views<T, DIMENSIONS_T, DATA_T, COUNT_T ..., countUp>(strideDescArray, rData, satCount);
    }
    else
    {
        return std::array{ strideDescArray[COUNT_T].view(Corrade::Containers::arrayView(rData), satCount) ... };
    }
}


} // namespace osp::universe
