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

#include <gtest/gtest.h>


#include <osp/core/array_view.h>
#include <osp/core/strong_id.h>
#include <osp/core/keyed_vector.h>

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/id_management/id_set_stl.hpp>

#include <vector>
#include <iostream>

using SubgraphId        = osp::StrongId<std::uint32_t, struct DummyForSubgraphId>;
using SubgraphTypeId    = osp::StrongId<std::uint32_t, struct DummyForSubgraphTypeId>;
using LocalPointId      = osp::StrongId<std::uint8_t, struct DummyForLocalPointId>;
using LocalCycleId      = osp::StrongId<std::uint8_t, struct DummyForLocalCycleId>;
using SynchronizerId    = osp::StrongId<std::uint8_t, struct DummyForSynchronizerId>;

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
    std::uint8_t pointCount{0};

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
struct Graph
{
    lgrn::IdRegistryStl<SubgraphId>             subgraphIds;
    osp::KeyedVec<SubgraphId, Subgraph>         subgraphs;

    lgrn::IdRegistryStl<SubgraphTypeId>         sgtypeIds;
    osp::KeyedVec<SubgraphTypeId, SubgraphType> sgtypes;

    lgrn::IdRegistryStl<SynchronizerId>         syncIds;
    osp::KeyedVec<SynchronizerId, Synchronizer> syncs;
};

// probably initialized with uninitialized memory
template<typename ID_T, std::size_t SIZE>
using StaticIdSet_t = lgrn::BitViewIdSet<lgrn::BitView<std::array<std::uint64_t, SIZE/64>>, ID_T>;

/**
 * data required to execute a Graph. Graph stays constant during execution.
 * better executors can be made in the future.
 *
 */
struct Executor
{
    enum class ESyncState : std::int8_t { Inactive, WaitForAlign, WaitForUnlock, WaitForAdvance };

    struct PerSubgraph
    {
        LocalCycleId activeCycle;
        std::uint8_t position;
    };
    struct PerSync
    {
        lgrn::IdSetStl<SubgraphId>          needToAdvance;
        ESyncState                          state               {ESyncState::Inactive};
    };

    void load(Graph const& graph) noexcept
    {
        auto const subgraphCapacity = graph.subgraphIds.capacity();
        perSubgraph.resize(subgraphCapacity);
        toCycle.resize(subgraphCapacity);

        perSync.resize(graph.syncIds.capacity());

        for (SynchronizerId const syncId : graph.syncIds)
        {
            perSync[syncId].needToAdvance.resize(subgraphCapacity);
        }

        for(SubgraphId const subgraphId : graph.subgraphIds)
        {
            SubgraphType const &sgtype = graph.sgtypes[graph.subgraphs[subgraphId].instanceOf];
            PerSubgraph &rPerSubgraph = perSubgraph[subgraphId];
            rPerSubgraph.activeCycle = sgtype.initialCycle;
            rPerSubgraph.position = sgtype.initialPos;
        }
    }

    bool update(std::vector<SynchronizerId> &rJustAlignedOut, Graph const& graph) noexcept
    {
        // 'pull/push' algorithm

        // 1. Search for syncs that are state=WaitForAlign
        //   * try to 'pull' connected points towards self. add subgraph to toCycle
        //   * check for canceled too
        // 2. Search for syncs that are state=WaitForAdvance
        //   * try to 'push' not-yet-advanced stages. add subgraph to toCycle
        // 3. Disqualify candidate subgraphs
        //   * subgraphs with (current position = a point with a sync on WaitForUnlock)
        //   * subgraphs with (current position = a point with a sync on WaitForAlign)
        //   * subgraphs with (current position = a point with a sync on WaitForAdvance and subgraph is not in needToAdvance)

        bool somethingHappened = false;
        toCycle.clear();

        for (SynchronizerId const syncId : graph.syncIds)
        {
            Synchronizer const &rSync = graph.syncs[syncId];
            Executor::PerSync &rExecSync = perSync[syncId];

            if (rExecSync.state == Executor::ESyncState::WaitForAlign)
            {
                bool aligned = true;
                for (SubgraphPointAddr addr : rSync.connectedPoints)
                {
                    Subgraph                const &rSubgraph = graph.subgraphs[addr.subgraph];
                    Executor::PerSubgraph   const &rExecSubgraph = perSubgraph[addr.subgraph];
                    SubgraphType            const &sgtype = graph.sgtypes[rSubgraph.instanceOf];
                    LocalPointId            const point = sgtype.cycles[rExecSubgraph.activeCycle].path[rExecSubgraph.position];

                    // if not yet aligned
                    if (addr.point != point)
                    {
                        toCycle.insert(addr.subgraph); // pull subgraph's position towards self
                        aligned = false;
                    }
                }
                if (aligned)
                {
                    rExecSync.state = Executor::ESyncState::WaitForUnlock;
                    rJustAlignedOut.push_back(syncId);
                    somethingHappened = true;
                }
            }
            else if (rExecSync.state == Executor::ESyncState::WaitForAdvance)
            {
                for (SubgraphId const subgraphId : perSync[syncId].needToAdvance)
                {
                    toCycle.insert(subgraphId); // push subgraph's position out of self
                }
            }
        }

        for (SubgraphId const subgraphId : toCycle)
        {
            Subgraph              const &rSubgraph     = graph.subgraphs[subgraphId];
            Executor::PerSubgraph const &rExecSubgraph = perSubgraph[subgraphId];
            SubgraphType          const &sgtype        = graph.sgtypes[rSubgraph.instanceOf];
            LocalPointId          const point          = sgtype.cycles[rExecSubgraph.activeCycle].path[rExecSubgraph.position];

            for (SynchronizerId const syncId : rSubgraph.points[point].connectedSyncs)
            {
                Synchronizer const &rSync = graph.syncs[syncId];
                Executor::PerSync const& rExecSync = perSync[syncId];

                if (rExecSync.state == Executor::ESyncState::WaitForAlign)
                {
                    // Sync is aligned with the current point, and wants this subgraph to stay at
                    // its current position and wait for other subgraphs to align
                    toCyclcErase.push_back(subgraphId);
                }
                else if (rExecSync.state == Executor::ESyncState::WaitForUnlock)
                {
                    // Sync is locked (task in progress). don't move!
                    toCyclcErase.push_back(subgraphId);
                }
                else if (     rExecSync.state == Executor::ESyncState::WaitForAdvance
                         && ! rExecSync.needToAdvance.contains(subgraphId))
                {
                    // only happens when a cycle has only 1 state to loop through
                    toCyclcErase.push_back(subgraphId);
                }
            }
        }

        for (SubgraphId const subgraphId : toCyclcErase)
        {
            toCycle.erase(subgraphId);
        }
        toCyclcErase.clear();

        for (SubgraphId const subgraphId : toCycle)
        {
            Subgraph                const &rSubgraph = graph.subgraphs[subgraphId];
            SubgraphType            const &sgtype      = graph.sgtypes[rSubgraph.instanceOf];
            Executor::PerSubgraph &rExecSubgraph = perSubgraph[subgraphId];
            LocalPointId          const point          = sgtype.cycles[rExecSubgraph.activeCycle].path[rExecSubgraph.position];

            for (SynchronizerId const syncId : graph.subgraphs[subgraphId].points[point].connectedSyncs)
            {
                PerSync &rExecSync = perSync[syncId];

                if (rExecSync.state == ESyncState::Inactive)
                {
                    continue;
                }

                rExecSync.needToAdvance.erase(subgraphId);

                if (rExecSync.needToAdvance.empty())
                {
                    // done advancing all
                    rExecSync.state = ESyncState::WaitForAlign;
                }
            }

            rExecSubgraph.position ++;
            if (rExecSubgraph.position == sgtype.cycles[rExecSubgraph.activeCycle].path.size())
            {
                rExecSubgraph.position = 0;
            }
        }

        somethingHappened |= !toCycle.empty();

        return somethingHappened;
    }

    void unlock(SynchronizerId syncId, Graph const& graph)
    {
        PerSync &rExecSync = perSync[syncId];
        LGRN_ASSERT(rExecSync.state == ESyncState::WaitForUnlock);

        rExecSync.state = ESyncState::WaitForAdvance;

        for (SubgraphPointAddr const addr : graph.syncs[syncId].connectedPoints)
        {
            rExecSync.needToAdvance.insert(addr.subgraph);
        }
    }

    bool is_locked(SynchronizerId syncId, Graph const& graph) const noexcept
    {
        return perSync[syncId].state == ESyncState::WaitForUnlock;
    }

    bool select_cycle(SubgraphId const subgraphId, LocalCycleId const cycleId, Graph const& graph) noexcept
    {
        Subgraph                const &rSubgraph    = graph.subgraphs[subgraphId];
        PerSubgraph                   &rPerSubgraph = perSubgraph[subgraphId];
        SubgraphType            const &sgtype       = graph.sgtypes[rSubgraph.instanceOf];
        LocalPointId            const currentPoint  = sgtype.cycles[rPerSubgraph.activeCycle].path[rPerSubgraph.position];

        SubgraphType::Cycle const& cycle = sgtype.cycles[cycleId];

        auto const &cycleFirstIt = cycle.path.begin();
        auto const &cycleLastIt  = cycle.path.end();
        auto const foundIt = std::find(cycleFirstIt, cycleLastIt, currentPoint);

        if (foundIt == cycleLastIt)
        {
            return false;
        }

        rPerSubgraph.activeCycle = cycleId;
        rPerSubgraph.position    = std::uint8_t(std::distance(cycleFirstIt, foundIt));

        return true;
    }

    enum class ESyncAction : std::int8_t { SetEnable, SetDisable, Unlock };

    void batch(ESyncAction const action, osp::ArrayView<SynchronizerId const> const syncs, Graph const& graph)
    {
        for (SynchronizerId const syncId : syncs)
        {
            PerSync &rExecSync = perSync[syncId];
            switch(action)
            {
            case ESyncAction::SetEnable:
                if (rExecSync.state == ESyncState::Inactive)
                {
                    rExecSync.state = ESyncState::WaitForAlign;
                }
                break;
            case ESyncAction::SetDisable:
                if (rExecSync.state == ESyncState::WaitForAdvance)
                {
                    rExecSync.needToAdvance.clear();
                }
                rExecSync.state = ESyncState::Inactive;
                break;
            case ESyncAction::Unlock:
                LGRN_ASSERT(rExecSync.state == ESyncState::WaitForUnlock);
                rExecSync.state = ESyncState::WaitForAdvance;
                for (SubgraphPointAddr const addr : graph.syncs[syncId].connectedPoints)
                {
                    rExecSync.needToAdvance.insert(addr.subgraph);
                }
                break;
            }
        }
    }

    void batch(ESyncAction const action, std::initializer_list<SynchronizerId const> const syncs, Graph const& graph)
    {
        batch(action, osp::arrayView(syncs), graph);
    }

    lgrn::IdSetStl<SubgraphId>     toCycle;
    std::vector<SubgraphId>  toCyclcErase;

    osp::KeyedVec<SubgraphId, PerSubgraph>  perSubgraph;
    osp::KeyedVec<SynchronizerId, PerSync>  perSync;
};


// Most of the main codebase uses strong ID types and variable names, which are fast and harder
// to mess up, but string names are more 'stupid simple'. Great for a small unit test.

struct ArgCycle
{
    std::string_view name;
    std::initializer_list<std::string_view> path;
};

struct ArgInitialCycle
{
    std::string_view cycle;
    std::uint8_t position;
};

struct ArgSubgraphType
{
    std::string_view name;
    std::initializer_list<std::string_view> points;
    std::initializer_list<ArgCycle> cycles;
    ArgInitialCycle initialCycle;
};

struct ArgSubgraph
{
    std::string_view name;
    std::string_view type;
};

struct ArgConnectToPoint
{
    std::string_view subgraph;
    std::string_view point;

};

struct ArgSync
{
    std::string_view name;
    std::initializer_list<ArgConnectToPoint> connections;
};

struct Args
{
    std::initializer_list<ArgSubgraphType> types;
    std::initializer_list<ArgSubgraph> subgraphs;
    std::initializer_list<ArgSync> syncs;
};

SubgraphId find_subgraph(std::string_view debugName, Graph const& graph)
{
    auto const &subgraphFirstIt = graph.subgraphs.begin();
    auto const &subgraphLastIt  = graph.subgraphs.end();
    auto const foundIt = std::find_if(
            subgraphFirstIt, subgraphLastIt,
            [debugName] (Subgraph const& subgraph)
            { return subgraph.debugName == debugName; });
    return foundIt == subgraphLastIt
            ? SubgraphId{}
            : SubgraphId::from_index(std::distance(subgraphFirstIt, foundIt));
}

LocalCycleId find_cycle(std::string_view debugName, SubgraphTypeId sgtypeId, Graph const& graph)
{
    SubgraphType const type       = graph.sgtypes[sgtypeId];
    auto const &cyclesFirstIt = type.cycles.begin();
    auto const &cyclesLastIt  = type.cycles.end();
    auto const foundIt = std::find_if(
            cyclesFirstIt, cyclesLastIt,
            [debugName] (SubgraphType::Cycle const& cycle)
            { return cycle.debugName == debugName; });
    return foundIt == cyclesLastIt
            ? LocalCycleId{}
            : LocalCycleId::from_index(std::distance(cyclesFirstIt, foundIt));
}

SubgraphTypeId find_sgtype(std::string_view debugName, Graph const& graph)
{
    auto const &sgtypeFirstIt = graph.sgtypes.begin();
    auto const &sgtypeLastIt = graph.sgtypes.end();
    auto const foundIt = std::find_if(sgtypeFirstIt, sgtypeLastIt,
            [debugName] (SubgraphType const& sgtype)
            { return sgtype.debugName == debugName; });
    return SubgraphTypeId::from_index(std::distance(sgtypeFirstIt, foundIt));
}

SynchronizerId find_sync(std::string_view debugName, Graph const& graph)
{
    auto const &syncFirstIt = graph.syncs.begin();
    auto const &syncLastIt = graph.syncs.end();
    auto const foundIt = std::find_if(syncFirstIt, syncLastIt,
            [debugName] (Synchronizer const& sync)
            { return sync.debugName == debugName; });
    return SynchronizerId::from_index(std::distance(syncFirstIt, foundIt));
}

Graph make_test_graph(Args args)
{
    Graph out;

    out.sgtypeIds.reserve(args.types.size());
    out.sgtypes.resize(out.sgtypeIds.capacity());

    out.subgraphIds.reserve(args.subgraphs.size());
    out.subgraphs.resize(out.subgraphIds.capacity());

    out.syncIds.reserve(args.syncs.size());
    out.syncs.resize(out.syncIds.capacity());

    // Make subgraph types
    for (ArgSubgraphType const& argSgtype : args.types)
    {
        SubgraphTypeId const subgraphTypeId = out.sgtypeIds.create();
        SubgraphType &rSgtype =  out.sgtypes[subgraphTypeId];

        rSgtype.debugName = argSgtype.name;

        // set point count and names
        int const pointCount = argSgtype.points.size();
        rSgtype.pointCount = std::uint8_t(pointCount);
        rSgtype.points.resize(pointCount);
        for (int i = 0; i < pointCount; ++i)
        {
            rSgtype.points[LocalPointId::from_index(i)].debugName = argSgtype.points.begin()[i];
        }

        auto const& pointsFirstIt = rSgtype.points.begin();
        auto const& pointsLastIt = rSgtype.points.end();

        // make cycles
        rSgtype.cycles.resize(argSgtype.cycles.size());
        for (int i = 0; i < argSgtype.cycles.size(); ++i)
        {
            auto const cycleId = LocalCycleId::from_index(i);
            auto const& argCycle = argSgtype.cycles.begin()[i];
            SubgraphType::Cycle &rCycle = rSgtype.cycles[cycleId];

            rCycle.debugName = argCycle.name;
            if (rCycle.debugName == argSgtype.initialCycle.cycle)
            {
                rSgtype.initialCycle = cycleId;
                rSgtype.initialPos   = argSgtype.initialCycle.position;
            }

            for (std::string_view const pointName : argCycle.path)
            {
                auto foundIt = std::find_if(pointsFirstIt, pointsLastIt,
                                            [pointName] (SubgraphType::Point const& point)
                                            { return point.debugName == pointName; });

                LGRN_ASSERTMV(foundIt != pointsLastIt, "No point with name found", pointName, rSgtype.debugName);
                rCycle.path.push_back(LocalPointId::from_index(std::distance(pointsFirstIt, foundIt)));
            }
        }
        LGRN_ASSERTM(rSgtype.initialCycle.has_value(), "Initial cycle is missing");
    }

    // Make Subgraphs
    for (ArgSubgraph const& argSubgraph : args.subgraphs)
    {
        SubgraphId const subgraphId = out.subgraphIds.create();
        Subgraph &rSubgraph = out.subgraphs[subgraphId];

        rSubgraph.debugName = argSubgraph.name;

        // Find SubgraphTypeId by name to set rSubgraph.instanceOf
        rSubgraph.instanceOf = find_sgtype(argSubgraph.type, out);
        LGRN_ASSERTMV(subgraphId.has_value(), "No SubgraphType with name found", argSubgraph.type);

        rSubgraph.points.resize(out.sgtypes[rSubgraph.instanceOf].pointCount);
    }

    // Make synchronizers
    for (ArgSync const& argSync : args.syncs)
    {
        SynchronizerId const syncId = out.syncIds.create();
        Synchronizer &rSync = out.syncs[syncId];

        rSync.debugName = argSync.name;

        for (ArgConnectToPoint const& argConnect : argSync.connections)
        {
            SubgraphId const subgraphId = find_subgraph(argConnect.subgraph, out);
            LGRN_ASSERTMV(subgraphId.has_value(), "No Subgraph with name found", argConnect.subgraph);
            Subgraph &rSubgraph = out.subgraphs[subgraphId];

            SubgraphType const& sgtype = out.sgtypes[rSubgraph.instanceOf];
            auto const &pointsFirst = sgtype.points.begin();
            auto const &pointsLast  = sgtype.points.end();

            auto const foundPointIt = std::find_if(
                    pointsFirst, pointsLast,
                    [argConnect] (SubgraphType::Point const& point)
                    { return point.debugName == argConnect.point; });
            LGRN_ASSERTMV(foundPointIt != pointsLast, "No Point with name found", argConnect.subgraph, sgtype.debugName, argConnect.point);
            auto const pointId = LocalPointId::from_index(std::distance(pointsFirst, foundPointIt));

            rSubgraph.points[pointId].connectedSyncs.push_back(syncId);
            rSync.connectedPoints.push_back({subgraphId, pointId});
        }
    }

    return out;
} // make_test_graph

testing::AssertionResult is_locked(std::initializer_list<SynchronizerId> locked, Executor& rExec, std::vector<SynchronizerId> &rJustLocked, Graph const& graph)
{
    for (SynchronizerId const syncId : locked)
    {
        if( ! rExec.is_locked(syncId, graph) )
        {
            return testing::AssertionFailure() << "SynchronizerId=" << int(syncId.value) << " debugName=\"" << graph.syncs[syncId].debugName << "\" is not locked";
        }

        if (std::find(rJustLocked.begin(), rJustLocked.end(), syncId) == rJustLocked.end())
        {
            return testing::AssertionFailure() << "justLocked vector does not contain SynchronizerId=" << syncId.value << " \"" << graph.syncs[syncId].debugName << "\"";
        }
    }
    if(rJustLocked.size() != locked.size())
    {
        return testing::AssertionFailure() << "Excess items in justLocked vector";
    }
    return testing::AssertionSuccess();
}

template <typename ... ID_T>
bool is_all_not_null(ID_T const& ... ids)
{
    return (ids.has_value() && ... );
}

using ESyncAction = Executor::ESyncAction;

TEST(SyncExec, Basic)
{
    Graph const graph = make_test_graph(
    {
        .types =
        {
            {
                .name = "4PointLoop",
                .points = {"A", "B", "C", "D"},
                .cycles =
                {
                    {
                        .name = "MainCycle",
                        .path = {"A", "B", "C", "D"}
                    }
                },
                .initialCycle = { .cycle = "MainCycle", .position = 0 }
            }
        },
        .subgraphs =
        {
            { .name = "Bulb", .type = "4PointLoop" },
            { .name = "Fish", .type = "4PointLoop" },
            { .name = "Rock", .type = "4PointLoop" }
        },
        .syncs =
        {
            {
                .name = "Sync_0",
                .connections =
                {
                    { .subgraph = "Bulb", .point = "A" },
                    { .subgraph = "Fish", .point = "A" },
                    { .subgraph = "Rock", .point = "B" }
                }
            },
            {
                .name = "Sync_1",
                .connections =
                {
                    { .subgraph = "Bulb", .point = "A" },
                    { .subgraph = "Fish", .point = "B" },
                    { .subgraph = "Rock", .point = "B" }
                }
            },
            {
                .name = "Sync_2",
                .connections =
                {
                    { .subgraph = "Bulb", .point = "B" },
                    { .subgraph = "Fish", .point = "B" },
                    { .subgraph = "Rock", .point = "B" }
                }
            },
            {
                .name = "Sync_3",
                .connections =
                {
                    { .subgraph = "Bulb", .point = "D" },
                    { .subgraph = "Fish", .point = "D" },
                    { .subgraph = "Rock", .point = "D" }
                }
            },
            {
                .name = "Sync_4",
                .connections =
                {
                    { .subgraph = "Bulb", .point = "D" },
                    { .subgraph = "Fish", .point = "D" },
                    { .subgraph = "Rock", .point = "D" }
                }
            },
        }
    });

    SynchronizerId const sync0Id = find_sync("Sync_0", graph);
    SynchronizerId const sync1Id = find_sync("Sync_1", graph);
    SynchronizerId const sync2Id = find_sync("Sync_2", graph);
    SynchronizerId const sync3Id = find_sync("Sync_3", graph);
    SynchronizerId const sync4Id = find_sync("Sync_4", graph);

    std::vector<SynchronizerId> justLocked;
    Executor exec;
    exec.load(graph);
    exec.batch(ESyncAction::SetEnable, {sync0Id, sync1Id, sync2Id, sync3Id, sync4Id}, graph);

    while (exec.update(justLocked, graph)) { }

    // Sync 0 locks first
    ASSERT_TRUE(is_locked({sync0Id}, exec, justLocked, graph));
    exec.unlock(sync0Id, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // Sync 1 locks
    ASSERT_TRUE(is_locked({sync1Id}, exec, justLocked, graph));
    exec.unlock(sync1Id, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // Sync 2 locks
    ASSERT_TRUE(is_locked({sync2Id}, exec, justLocked, graph));
    exec.unlock(sync2Id, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // Sync 3 and 4 locks simultaneously
    ASSERT_TRUE(is_locked({sync3Id, sync4Id}, exec, justLocked, graph));
    exec.unlock(sync3Id, graph);
    exec.unlock(sync4Id, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // Loop back to Sync 0
    ASSERT_TRUE(is_locked({sync0Id}, exec, justLocked, graph));
    exec.unlock(sync0Id, graph);
    justLocked.clear();
}

TEST(SyncExec, ParallelSize1Loop)
{
    Graph const graph = make_test_graph(
    {
        .types =
        {
            {
                .name = "SinglePoint",
                .points = {"TheOnlyPoint"},
                .cycles =
                {
                    {
                        .name = "MainCycle",
                        .path = {"TheOnlyPoint"}
                    }
                },
                .initialCycle = { .cycle = "MainCycle", .position = 0 }
            }
        },
        .subgraphs =
        {
            { .name = "Foo", .type = "SinglePoint" },
            { .name = "Bar", .type = "SinglePoint" },
        },
        .syncs =
        {
            {
                .name = "Sync_0",
                .connections =
                {
                    { .subgraph = "Foo", .point = "TheOnlyPoint" },
                    { .subgraph = "Bar", .point = "TheOnlyPoint" }
                }
            }
        }
    });

    SynchronizerId const syncId = find_sync("Sync_0", graph);
    ASSERT_TRUE(syncId.has_value());

    std::vector<SynchronizerId> justLocked;
    Executor exec;
    exec.load(graph);
    exec.batch(ESyncAction::SetEnable, {syncId}, graph);

    for(int i = 0; i < 10; ++i)
    {
        // something 'should happen' after first run or after unlock()
        ASSERT_TRUE(exec.update(justLocked, graph));

        // update 'a couple more times' until there's nothing to do
        exec.update(justLocked, graph);
        exec.update(justLocked, graph);
        exec.update(justLocked, graph);

        // Sync_0 should be aligned and locked
        ASSERT_TRUE(is_locked({syncId}, exec, justLocked, graph));
        justLocked.clear();

        exec.unlock(syncId, graph);
        ASSERT_FALSE(exec.is_locked(syncId, graph));
    }
}

TEST(SyncExec, BranchingPath)
{
    Graph const graph = make_test_graph(
    {
        .types =
        {
            {
                .name = "BranchingPaths",
                .points = {"Common", "A", "B"},
                .cycles =
                {
                    {
                        .name = "Idle",
                        .path = {"Common"}
                    },
                    {
                        .name = "ViaA",
                        .path = {"Common", "A"}
                    },
                    {
                        .name = "ViaB",
                        .path = {"Common", "B"}
                    }
                },
                .initialCycle = { .cycle = "Idle", .position = 0 }
            },
            {
                .name = "3PointLoop",
                .points = {"X", "Y", "Z"},
                .cycles =
                {
                    {
                        .name = "MainCycle",
                        .path = {"X", "Y", "Z"}
                    }
                },
                .initialCycle = { .cycle = "MainCycle", .position = 0 }
            }
        },
        .subgraphs =
        {
            { .name = "BP", .type = "BranchingPaths" },
            { .name = "3PL", .type = "3PointLoop" },
        },
        .syncs =
        {
            {
                .name = "Schedule",
                .connections =
                {
                    { .subgraph = "BP", .point = "Common" },
                    { .subgraph = "3PL", .point = "X" }
                }
            },
            {
                .name = "End of 3PL",
                .connections =
                {
                    { .subgraph = "3PL", .point = "Z" }
                }
            },
            {
                .name = "With A",
                .connections =
                {
                    { .subgraph = "BP", .point = "A" },
                    { .subgraph = "3PL", .point = "Y" }
                }
            },
            {
                .name = "With B",
                .connections =
                {
                    { .subgraph = "BP", .point = "B" },
                    { .subgraph = "3PL", .point = "Y" }
                }
            }
        }
    });

    SubgraphTypeId const branching      = find_sgtype("BranchingPaths", graph);

    LocalCycleId   const branchingViaA  = find_cycle("ViaA", branching, graph);
    LocalCycleId   const branchingViaB  = find_cycle("ViaB", branching, graph);

    SubgraphId     const bp             = find_subgraph("BP", graph);

    SynchronizerId const schedule       = find_sync("Schedule", graph);
    SynchronizerId const eo3pl          = find_sync("End of 3PL", graph);
    SynchronizerId const withA          = find_sync("With A", graph);
    SynchronizerId const withB          = find_sync("With B", graph);

    ASSERT_TRUE(is_all_not_null(schedule, eo3pl, withA, withB, branching, branchingViaA, branchingViaB, bp));

    std::vector<SynchronizerId> justLocked;
    Executor exec;
    exec.load(graph);

    exec.batch(ESyncAction::SetEnable, {schedule, eo3pl}, graph);

    // initial Idle cycle just repeatedly locks "Schedule" and "End of 3PL" sync
    for (int i = 0; i < 20; ++i)
    {
        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
        exec.unlock(schedule, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({eo3pl}, exec, justLocked, graph));
        exec.unlock(eo3pl, graph);
        justLocked.clear();
    }

    while (exec.update(justLocked, graph)) { }

    // Switch subgraph BranchingPaths's current cycle to ViaA
    ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
    exec.select_cycle(bp, branchingViaA, graph);
    exec.batch(ESyncAction::SetEnable, {withA}, graph);

    for (int i = 0; i < 20; ++i)
    {
        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
        exec.unlock(schedule, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({withA}, exec, justLocked, graph));
        exec.unlock(withA, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({eo3pl}, exec, justLocked, graph));
        exec.unlock(eo3pl, graph);
        justLocked.clear();
    }

    while (exec.update(justLocked, graph)) { }

    // Switch subgraph BranchingPaths's current cycle back to MainCycle
    ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
    exec.select_cycle(bp, branchingViaA, graph);
    exec.batch(ESyncAction::SetDisable, {withA}, graph);

    for (int i = 0; i < 20; ++i)
    {
        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
        exec.unlock(schedule, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({eo3pl}, exec, justLocked, graph));
        exec.unlock(eo3pl, graph);
        justLocked.clear();
    }

    while (exec.update(justLocked, graph)) { }

    // Switch subgraph BranchingPaths's current cycle to ViaB
    ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
    exec.select_cycle(bp, branchingViaB, graph);
    exec.batch(ESyncAction::SetEnable, {withB}, graph);

    for (int i = 0; i < 20; ++i)
    {
        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({schedule}, exec, justLocked, graph));
        exec.unlock(schedule, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({withB}, exec, justLocked, graph));
        exec.unlock(withB, graph);
        justLocked.clear();

        while (exec.update(justLocked, graph)) { }

        ASSERT_TRUE(is_locked({eo3pl}, exec, justLocked, graph));
        exec.unlock(eo3pl, graph);
        justLocked.clear();
    }
}

//
// Task O0 - Write <Requests>
//
// scheduler Task L0 - check if we need to loop, like `while(hasRequests)`
// {
//     Task L1 - Read <Request>, Write to <Process 0>
//     Task L2 - Read <Process 0>, Write to <Process 1>
//     Task L3 - Read <Process 1>, Write to <Results>
// }
//
// Task O1 - Clear <Requests>
// Task O2 - Read <Results>
// Task O3 - Clear <Results>
//
TEST(SyncExec, NestedLoop)
{
    Graph const graph = make_test_graph(
    {

        .types =
        {
            {
                .name = "LoopBlockController",
                .points = {"Start", "Running", "Finish"},
                .cycles =
                {
                    {
                        .name = "OnlyCycle",
                        .path = {"Start", "Running", "Finish"}
                    }
                },
                .initialCycle = { .cycle = "OnlyCycle", .position = 0 }
            },
            {
                .name = "OSP-Style Intermediate-Value Pipeline",
                .points = {"Finish", "Start", "Schedule", "Read", "Clear", "Modify"},
                .cycles =
                {
                    {
                        .name = "Control",
                        .path = {"Start", "Schedule", "Finish"}
                    },
                    {
                        .name = "Stages",
                        .path = {"Schedule", "Read", "Clear", "Modify"}
                    },
                    {
                        .name = "Canceled",
                        .path = {"Schedule"}
                    }
                },
                .initialCycle = { .cycle = "Control", .position = 0 }
            }
        },
        .subgraphs =
        {
            { .name = "OuterLoopCtrl",      .type = "LoopBlockController" },
            { .name = "Outer-Request",      .type = "OSP-Style Intermediate-Value Pipeline" },
            { .name = "Outer-Results",      .type = "OSP-Style Intermediate-Value Pipeline" },
            { .name = "InnerLoopCtrl",      .type = "LoopBlockController" },
            { .name = "Inner-Process0",     .type = "OSP-Style Intermediate-Value Pipeline" },
            { .name = "Inner-Process1",     .type = "OSP-Style Intermediate-Value Pipeline" },
        },
        .syncs =
        {
            // Stops the outer loop from running until it's commanded to start externally
            {
                .name = "syOtrExtStart",
                .connections =
                {
                    { .subgraph = "OuterLoopCtrl", .point = "Start" },
                }
            },

            // Sync Start and Finish of OuterLoopCtrl's childen to its Running point.
            // This assures children can only run while OuterLoopCtrl is in its Running state.
            // SchInit 'schedule init' assures that all children start (cycles set) at the same
            // time.
            {
                .name = "syOtrLCLeft",
                .connections =
                {
                    { .subgraph = "OuterLoopCtrl",      .point = "Running" },
                    { .subgraph = "Outer-Results",      .point = "Start" },
                    { .subgraph = "Outer-Request",      .point = "Start" },
                    { .subgraph = "InnerLoopCtrl",      .point = "Start" },
                }
            },
            {
                .name = "syOtrLCRight",
                .connections =
                {
                    { .subgraph = "OuterLoopCtrl",      .point = "Running" },
                    { .subgraph = "Outer-Request",      .point = "Finish" },
                    { .subgraph = "Outer-Results",      .point = "Finish" },
                    { .subgraph = "InnerLoopCtrl",      .point = "Finish" },
                }
            },
            {
                .name = "syOtrLCSchInit",
                .connections =
                {
                    { .subgraph = "Outer-Request",      .point = "Schedule" },
                    { .subgraph = "Outer-Results",      .point = "Schedule" },
                    { .subgraph = "InnerLoopCtrl",      .point = "Start" },
                }
            },

            // Same as above, but for InnerLoopCtrl
            {
                .name = "syInrLCLeft",
                .connections =
                {
                    { .subgraph = "InnerLoopCtrl",      .point = "Running" },
                    { .subgraph = "Inner-Process0",     .point = "Start" },
                    { .subgraph = "Inner-Process1",     .point = "Start" },
                }
            },
            {
                .name = "syInrLCRight",
                .connections =
                {
                    { .subgraph = "InnerLoopCtrl",      .point = "Running" },
                    { .subgraph = "Inner-Process0",     .point = "Finish" },
                    { .subgraph = "Inner-Process1",     .point = "Finish" },
                }
            },
            {
                .name = "syInrLCSchInit",
                .connections =
                {
                    { .subgraph = "Inner-Process0",     .point = "Schedule" },
                    { .subgraph = "Inner-Process1",     .point = "Schedule" },
                }
            },

            {
                .name = "syTaskO0",
                .connections =
                {
                    { .subgraph = "Outer-Request",      .point = "Modify" },
                }
            },

            // An extra 'sustainer' (sus) sync is needed to sync across inner and outer loop blocks.
            //
            // The sustainer keeps outer points in a steady position for inner tasks to loop
            // multiple times. It works by syncing with every outer point the 1st sync connects to,
            // but also syncs with InnerLoopCtrl.Finish.
            //
            // Process:
            // * 1st sync locks normally first, as if there was no loop.
            // * 1st sync is then immediately disabled.
            // * Task can be run multiple times as the loop iterates.
            // * sus locks when the loop is over.
            // * 1st sync can then be re-enabled.
            {
                .name = "syTaskL0",
                .connections =
                {
                    { .subgraph = "Outer-Request",      .point = "Read" },
                }
            },
            {
                .name = "syTaskL0sus",
                .connections =
                {
                    { .subgraph = "Outer-Request",      .point = "Read" },
                    { .subgraph = "InnerLoopCtrl",      .point = "Finish" },
                }
            },

            {
                .name = "syTaskL1",
                .connections =
                {
                    { .subgraph = "Outer-Request",      .point = "Read" },
                    { .subgraph = "Inner-Process0",     .point = "Modify" },
                }
            },
            {
                .name = "syTaskL1sus",
                .connections =
                {
                    { .subgraph = "Outer-Request",      .point = "Read" },
                    { .subgraph = "InnerLoopCtrl",      .point = "Finish" },
                }
            },

            {
                .name = "syTaskL2",
                .connections =
                {
                    { .subgraph = "Inner-Process0",     .point = "Read" },
                    { .subgraph = "Inner-Process1",     .point = "Modify" },
                }
            },

            {
                .name = "syTaskL3",
                .connections =
                {
                    { .subgraph = "Inner-Process1",     .point = "Read" },
                    { .subgraph = "Outer-Results",      .point = "Modify" },
                }
            },
            {
                .name = "syTaskL3sus",
                .connections =
                {
                    { .subgraph = "Inner-Process1",     .point = "Read" },
                    { .subgraph = "Outer-Results",      .point = "Modify" },
                    { .subgraph = "InnerLoopCtrl",      .point = "Finish" },
                }
            },

            {
                .name = "syTaskO1",
                .connections =
                {
                    { .subgraph = "Outer-Request",      .point = "Clear" },
                }
            },

            {
                .name = "syTaskO2",
                .connections =
                {
                    { .subgraph = "Outer-Results",      .point = "Read" },
                }
            },

            {
                .name = "syTaskO3",
                .connections =
                {
                    { .subgraph = "Outer-Results",      .point = "Clear" },
                }
            },
        }
    });

    SubgraphTypeId const loopBlkCtrl        = find_sgtype("LoopBlockController",                   graph);
    SubgraphTypeId const ospPipeline        = find_sgtype("OSP-Style Intermediate-Value Pipeline", graph);

    LocalCycleId   const ospPipelineControl = find_cycle("Control",  ospPipeline, graph);
    LocalCycleId   const ospPipelineStages  = find_cycle("Stages",   ospPipeline, graph);
    LocalCycleId   const ospPipelineCancel  = find_cycle("Canceled", ospPipeline, graph);

    SubgraphId     const outerLoopCtrl      = find_subgraph("OuterLoopCtrl",  graph);
    SubgraphId     const outerRequests      = find_subgraph("Outer-Request",  graph);
    SubgraphId     const outerResults       = find_subgraph("Outer-Results",  graph);
    SubgraphId     const innerLoopCtrl      = find_subgraph("InnerLoopCtrl",  graph);
    SubgraphId     const innerProcess0      = find_subgraph("Inner-Process0", graph);
    SubgraphId     const innerProcess1      = find_subgraph("Inner-Process1", graph);

    SynchronizerId const syOtrExtStart      = find_sync("syOtrExtStart",    graph);
    SynchronizerId const syOtrLCLeft        = find_sync("syOtrLCLeft",      graph);
    SynchronizerId const syOtrLCRight       = find_sync("syOtrLCRight",     graph);
    SynchronizerId const syOtrLCSchInit     = find_sync("syOtrLCSchInit",   graph);
    SynchronizerId const syInrLCLeft        = find_sync("syInrLCLeft",      graph);
    SynchronizerId const syInrLCRight       = find_sync("syInrLCRight",     graph);
    SynchronizerId const syInrLCSchInit     = find_sync("syInrLCSchInit",   graph);
    SynchronizerId const syTaskO0           = find_sync("syTaskO0",         graph);
    SynchronizerId const syTaskL0           = find_sync("syTaskL0",         graph);
    SynchronizerId const syTaskL0sus        = find_sync("syTaskL0sus",      graph);
    SynchronizerId const syTaskL1           = find_sync("syTaskL1",         graph);
    SynchronizerId const syTaskL1sus        = find_sync("syTaskL1sus",      graph);
    SynchronizerId const syTaskL2           = find_sync("syTaskL2",         graph);
    SynchronizerId const syTaskL3           = find_sync("syTaskL3",         graph);
    SynchronizerId const syTaskL3sus        = find_sync("syTaskL3sus",      graph);
    SynchronizerId const syTaskO1           = find_sync("syTaskO1",         graph);
    SynchronizerId const syTaskO2           = find_sync("syTaskO2",         graph);
    SynchronizerId const syTaskO3           = find_sync("syTaskO3",         graph);

    ASSERT_TRUE(is_all_not_null(
            loopBlkCtrl, ospPipeline, ospPipelineControl, ospPipelineStages, ospPipelineCancel,
            outerLoopCtrl, outerRequests, outerResults, innerLoopCtrl, innerProcess0, innerProcess1,
            syOtrExtStart, syOtrLCLeft, syOtrLCRight, syOtrLCSchInit, syInrLCLeft, syInrLCRight,
            syInrLCSchInit, syTaskO0, syTaskL0, syTaskL0sus, syTaskL1, syTaskL1sus, syTaskL2,
            syTaskL3, syTaskL3sus, syTaskO1, syTaskO2, syTaskO3));

    std::vector<SynchronizerId> justLocked;
    Executor exec;
    exec.load(graph);

    int request = 0;
    int process = 0;
    int results = 0;

    exec.batch(ESyncAction::SetEnable, {
            syOtrExtStart,
            syOtrLCLeft, syOtrLCRight, syOtrLCSchInit,
            syInrLCLeft, syInrLCRight, syInrLCSchInit,
            syTaskO0,
            syTaskL0, syTaskL0sus, syTaskL1, syTaskL1sus, syTaskL2, syTaskL3, syTaskL3sus,
            syTaskO1, syTaskO2, syTaskO3}, graph);

    while (exec.update(justLocked, graph)) { }

    ASSERT_TRUE(is_locked({syOtrExtStart}, exec, justLocked, graph));
    exec.unlock(syOtrExtStart, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // Outer block starts. SYN_OuterLoopCtrl-Left
    ASSERT_TRUE(is_locked({syOtrLCLeft}, exec, justLocked, graph));
    exec.unlock(syOtrLCLeft, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // 'schedule init' assures that all children start (cycles set) at the same time by
    // aligning all the schedule stages.
    ASSERT_TRUE(is_locked({syOtrLCSchInit}, exec, justLocked, graph));
    exec.select_cycle(outerRequests, ospPipelineStages, graph);
    exec.select_cycle(outerResults,  ospPipelineStages, graph);
    exec.unlock(syOtrLCSchInit, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // why are these 3 syncs locked?
    // * syInrLCLeft - Entered the inner loop block
    // * syTaskL0    - Read results. does nothing as theres no results yet. In real OSP, we
    //                 could have schedule task that can check if there are any results before the
    //                 'Read' stage.
    // * syTaskO2    - Schedule inner loop
    ASSERT_TRUE(is_locked({syInrLCLeft, syTaskL0, syTaskO2}, exec, justLocked, graph));

    // ONLY unlock syTaskO2, which unlocks syTaskO3, then unlock that too
    exec.unlock(syTaskO2, graph);
    justLocked.clear();
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({syTaskO3}, exec, justLocked, graph));
    exec.unlock(syTaskO3, graph);
    justLocked.clear();

    // expect nothing to happen after
    while (exec.update(justLocked, graph)) { }
    ASSERT_TRUE(is_locked({}, exec, justLocked, graph));

    // now start the inner loop. unlock syInrLCLeft, locked but never unlocked previously.
    exec.unlock(syInrLCLeft, graph);
    // no justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // schedule init for inner loop
    ASSERT_TRUE(is_locked({syInrLCSchInit}, exec, justLocked, graph));
    exec.select_cycle(innerProcess0, ospPipelineStages, graph);
    exec.select_cycle(innerProcess1,  ospPipelineStages, graph);
    exec.unlock(syInrLCSchInit, graph);
    justLocked.clear();

    while (exec.update(justLocked, graph)) { }

    // nothing should happen since we're waiting for TaskL0 to complete
    ASSERT_TRUE(is_locked({syTaskL3}, exec, justLocked, graph));
    exec.unlock(syTaskL3, graph);
    justLocked.clear();
    while (exec.update(justLocked, graph)) { }

    ASSERT_TRUE(is_locked({}, exec, justLocked, graph));


    //exec.batch(ESyncAction::Unlock, {syInrLCLeft, syTaskL0, syTaskO2}, graph);


    //while (exec.update(justLocked, graph)) { }

    // syTaskO3 - clear results
    // syInrLCSchInit - part of initializing inner loop block
    //ASSERT_TRUE(is_locked({syInrLCLeft, syTaskL0, syTaskO2}, exec, justLocked, graph));



}
