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

#include <gtest/gtest.h>
#include <osp/executor/sync_graph.h>

namespace test_graph
{

using namespace osp::exec;

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
    bool debugGraphStraight = false;
    bool debugGraphLongAndUgly = false;
    std::initializer_list<ArgConnectToPoint> connections;
};

struct Args
{
    std::initializer_list<ArgSubgraphType> types;
    std::initializer_list<ArgSubgraph> subgraphs;
    std::initializer_list<ArgSync> syncs;
};

inline SubgraphId find_subgraph(std::string_view debugName, SyncGraph const& graph)
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

inline LocalCycleId find_cycle(std::string_view debugName, SubgraphTypeId sgtypeId, SyncGraph const& graph)
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

inline SubgraphTypeId find_sgtype(std::string_view debugName, SyncGraph const& graph)
{
    auto const &sgtypeFirstIt = graph.sgtypes.begin();
    auto const &sgtypeLastIt = graph.sgtypes.end();
    auto const foundIt = std::find_if(sgtypeFirstIt, sgtypeLastIt,
            [debugName] (SubgraphType const& sgtype)
            { return sgtype.debugName == debugName; });
    return SubgraphTypeId::from_index(std::distance(sgtypeFirstIt, foundIt));
}

inline SynchronizerId find_sync(std::string_view debugName, SyncGraph const& graph)
{
    auto const &syncFirstIt = graph.syncs.begin();
    auto const &syncLastIt = graph.syncs.end();
    auto const foundIt = std::find_if(syncFirstIt, syncLastIt,
            [debugName] (Synchronizer const& sync)
            { return sync.debugName == debugName; });
    return SynchronizerId::from_index(std::distance(syncFirstIt, foundIt));
}

inline SyncGraph make_test_graph(Args args)
{
    SyncGraph out;

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
        //rSgtype.pointCount = std::uint8_t(pointCount);
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

        rSubgraph.points.resize(out.sgtypes[rSubgraph.instanceOf].points.size());
    }

    // Make synchronizers
    for (ArgSync const& argSync : args.syncs)
    {
        SynchronizerId const syncId = out.syncIds.create();
        Synchronizer &rSync = out.syncs[syncId];

        rSync.debugName             = argSync.name;
        rSync.debugGraphStraight    = argSync.debugGraphStraight;
        rSync.debugGraphLongAndUgly = argSync.debugGraphLongAndUgly;

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

    for (SynchronizerId const syncId : out.syncIds)
    {
        std::sort(out.syncs[syncId].connectedPoints.begin(),
                  out.syncs[syncId].connectedPoints.end());
    }
    for (SubgraphId const subgraphId : out.subgraphIds)
    {
        Subgraph &rSubgraph = out.subgraphs[subgraphId];
        for (Subgraph::Point &rPoint : rSubgraph.points)
        {
            std::sort(rPoint.connectedSyncs.begin(), rPoint.connectedSyncs.end());
        }
    }

    out.debug_verify();

    std::cout << "\n\n" << SyncGraphDOTVisualizer{out} << "\n\n";


    return out;
} // make_test_graph


} // namespace test_graph
