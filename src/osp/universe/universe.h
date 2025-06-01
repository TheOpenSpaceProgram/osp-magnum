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
#include "../core/array_view.h"

#include <longeron/id_management/bitview_id_set.hpp>
#include <longeron/id_management/id_set_stl.hpp>
#include <longeron/id_management/registry_stl.hpp>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StridedArrayView.h>

#include <cstdint>
#include <cstring>

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
    SatelliteId parentSat;

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

struct UCtxComponentTypes
{
    lgrn::IdRegistryStl<ComponentTypeId>         ids;
    KeyedVec<ComponentTypeId, ComponentTypeInfo> info;
};

struct UCtxDefaultComponents
{
    ComponentTypeId satId;
    ComponentTypeId posX;
    ComponentTypeId posY;
    ComponentTypeId posZ;
    ComponentTypeId velX;
    ComponentTypeId velY;
    ComponentTypeId velZ;
    ComponentTypeId rotX;
    ComponentTypeId rotY;
    ComponentTypeId rotZ;
    ComponentTypeId rotW;
    ComponentTypeId radius;
    ComponentTypeId surface;
};

//-----------------------------------------------------------------------------

// Data Accessors

// simulations have std::shared_ptr<DataAccessor>
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

    // optimization: tiny map with tiny elements. maybe switch away from unordered_map
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
    osp::KeyedVec<DataAccessorId, std::weak_ptr<DataAccessor>> instances;
};

/**
 * @brief allow marking satellites in a DataAccessorId for deletion.
 *
 * separated out as UCtxDataAccessors are expected to be read as const most of the time, and this is mutable
 */
struct UCtxDeletedSatellites
{
    struct Accessor
    {
        std::set<SatelliteId> set;
        bool dirty = false;
    };

    osp::KeyedVec<DataAccessorId, Accessor> sets;
};

//-----------------------------------------------------------------------------

// Data Sources

/**
 * @brief Determines what components a satellite has and which data accessors it uses.
 */
struct DataSource
{
    using ComponentTypeSet_t = StaticIdSet_t<ComponentTypeId, 128>;

    struct Entry
    {
        ComponentTypeSet_t      components{}; // zero init;
        DataAccessorId          accessor{};
        std::uint32_t           _padding{0};
    };
    static_assert(sizeof(ComponentTypeSet_t) == 16);
    static_assert(sizeof(DataAccessorId) == 4);
    static_assert(sizeof(Entry) == 24);

    void sort()
    {
        std::sort(entries.begin(), entries.end(), [] (Entry const& lhs, Entry const& rhs) noexcept
        {
            if (lhs.accessor.value < rhs.accessor.value)
            {
                return true;
            }
            else if (lhs.accessor.value > rhs.accessor.value)
            {
                return false;
            }
            return std::memcmp(&lhs.components, &rhs.components, sizeof(ComponentTypeSet_t)) < 0;
        });
    }

    std::vector<Entry> entries;
};

struct UCtxDataSources
{
    lgrn::IdRegistryStl<DataSourceId>       ids;
    lgrn::IdRefCount<DataSourceId>          refCounts;
    osp::KeyedVec<DataSourceId, DataSource> instances;
};

//-----------------------------------------------------------------------------

// Satellite instances

struct UCtxSatelliteInstances
{
    lgrn::IdRegistryStl<SatelliteId>            ids;
    osp::KeyedVec<SatelliteId, DataSourceId>    dataSrc;
};

//-----------------------------------------------------------------------------

// Simulations

struct UCtxSimulations
{
    lgrn::IdRegistryStl<SimulationId> ids;
};

struct UCtxSimulationsDelete
{

};

//-----------------------------------------------------------------------------

// Intakes

struct SatelliteInserter
{
    class Contract
    {
    public:
        ~Contract()
        {
            LGRN_ASSERT(remaining == 0);
        }
    private:
        std::size_t remaining;
    };

    using UserData_t = std::array<std::byte, 32>;

    using ReserveFunc_t = Contract(*)(std::size_t amount, UserData_t&);
    ReserveFunc_t reserve{nullptr};

    using NextFunc_t = ArrayView<std::byte>(*)(Contract &rContract, UserData_t&);
    NextFunc_t next{nullptr};
};

struct Intake
{
    std::vector<ComponentTypeId>    components;
    SatelliteInserter               inserter;
    SimulationId                    owner;
    CoSpaceId                       cospace;
};

struct UCtxIntakes
{
    lgrn::IdRegistryStl<IntakeId>           ids;
    osp::KeyedVec<IntakeId, Intake>         instances;
};

class SimpleSatelliteBuffer
{
public:

    SatelliteInserter make_inserter();

private:
    DataAccessorId                  m_accessor;
    std::vector<ComponentTypeId>    m_components;

    /// Total of ComponentTypeInfo::size of all component types in m_components
    std::size_t                     m_totalComponentSize;

    ///
    std::unique_ptr<std::byte[]>    m_data;
};

//-----------------------------------------------------------------------------

struct UCtxTime
{
    std::uint64_t time;
};

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
