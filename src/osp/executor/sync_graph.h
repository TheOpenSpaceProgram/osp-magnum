/**
 * Open Space Program
 * Copyright © 2019-2025 Open Space Program Project
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

#include <osp/core/array_view.h>
#include <osp/core/strong_id.h>
#include <osp/core/keyed_vector.h>

#include <longeron/id_management/registry_stl.hpp>

#include <cstdint>
#include <vector>


namespace osp::exec
{

using SubgraphId        = osp::StrongId<std::uint32_t, struct DummyForSubgraphId>;
using SubgraphTypeId    = osp::StrongId<std::uint32_t, struct DummyForSubgraphTypeId>;
using LocalPointId      = osp::StrongId<std::uint8_t,  struct DummyForLocalPointId>;
using LocalCycleId      = osp::StrongId<std::uint8_t,  struct DummyForLocalCycleId>;
using SynchronizerId    = osp::StrongId<std::uint32_t, struct DummyForSynchronizerId>;

struct SubgraphType
{
    struct Cycle
    {
        std::string debugName;
        std::vector<LocalPointId> path;
    };

    struct Point
    {
        std::string debugName;
    };

    std::string debugName;

    osp::KeyedVec<LocalCycleId, Cycle> cycles;

    osp::KeyedVec<LocalPointId, Point> points;
    //std::uint8_t pointCount{0};

    LocalCycleId initialCycle;
    std::uint8_t initialPos;
};

struct Subgraph
{
    struct Point
    {
        std::vector<SynchronizerId> connectedSyncs;
    };

    SubgraphTypeId instanceOf; ///< This graph is an instance of which type?
    osp::KeyedVec<LocalPointId, Point> points;
    std::string debugName;
};

struct SubgraphPointAddr
{
    SubgraphId      subgraph;
    LocalPointId    point;

    constexpr auto operator<=>(SubgraphPointAddr const&) const = default;
};

struct Synchronizer
{
    std::string debugName;
    bool debugGraphStraight = false;
    bool debugGraphLoose = false;
    bool debugGraphLongAndUgly = false;

    std::vector<SubgraphPointAddr> connectedPoints;
};

/**
 *
 * invariants:
 * * Two-way connection between a synchronizer and connected points:
 *   * syncs[SYNC].connectedPoints                       must contain Addr(SUBGRAPH, POINT)
 *   * subgraphs[SUBGRAPH].points[POINT].connectedSyncs  must contain SYNC
 *
 */
struct SyncGraph
{
    struct ConnectSubgraphPoint { SubgraphId subgraph; LocalPointId point; };
    struct ConnectArgs { SynchronizerId sync; ConnectSubgraphPoint subgraphPoint; };

    void debug_verify() const;
    void dot(std::ostream &rOS) const;

    void connect(ConnectArgs connect)
    {
        subgraphs[connect.subgraphPoint.subgraph].points[connect.subgraphPoint.point].connectedSyncs.push_back(connect.sync);
        syncs[connect.sync].connectedPoints.push_back({connect.subgraphPoint.subgraph, connect.subgraphPoint.point});
    }

    lgrn::IdRegistryStl<SubgraphId>             subgraphIds;
    osp::KeyedVec<SubgraphId, Subgraph>         subgraphs;

    lgrn::IdRegistryStl<SubgraphTypeId>         sgtypeIds;
    osp::KeyedVec<SubgraphTypeId, SubgraphType> sgtypes;

    lgrn::IdRegistryStl<SynchronizerId>         syncIds;
    osp::KeyedVec<SynchronizerId, Synchronizer> syncs;
};

class ISyncGraphDebugInfo
{
public:
    virtual bool            is_sync_enabled(SyncGraph const& graph, SynchronizerId syncId) = 0;
    virtual bool            is_sync_locked(SyncGraph const& graph, SynchronizerId syncId) = 0;
    virtual LocalPointId    current_point(SyncGraph const& graph, SubgraphId subgraphId) = 0;
};

struct SyncGraphDOTVisualizer
{
    SyncGraph const& graph;

    ISyncGraphDebugInfo *pDebugInfo{nullptr};

    friend std::ostream& operator<<(std::ostream& rStream, SyncGraphDOTVisualizer const& self);
};

};

