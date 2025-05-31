/**
 * Open Space Program
 * Copyright © 2019-2025 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and  documentation files (the "Software"), to deal
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
#include "singlethread_sync_graph.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>

namespace osp::exec
{

class SyncGraphExecutorDebugger
{
    static SyncGraphExecutorDebugger make()
    {
        char const* path = std::getenv("OSP_FRAMEWORK_DEBUG_FILE");
        SyncGraphExecutorDebugger out;
        if (path == nullptr)
        {
            return {};
        }
        try
        {
            out.stream.emplace();
            out.stream->open(path, std::ios_base::out | std::ios_base::app);
            out.file = path;
        }
        catch(const std::exception &e)
        {
            std::cerr << "Failed to open file at OSP_FRAMEWORK_DEBUG_FILE\n" << e.what() << "\n";
        }
        return out;
    }

public:

    void write_new(SyncGraphExecutor &rExec, SyncGraph const& graph)
    {
        if (file.empty()) { return; }

        SyncGraphDOTVisualizer visualizer{.graph = graph, .pDebugInfo = nullptr};

        try
        {
            std::ostringstream os;
            os << "<NEW_GRAPH>\n" << rExec.startTime.time_since_epoch().count() << "\n" << visualizer << "\n</NEW_GRAPH>\n";
            *stream << os.str();
            stream->flush();
        }
        catch([[maybe_unused]] std::exception const& e)
        {
            std::cerr << "Failed to open file at OSP_FRAMEWORK_DEBUG_FILE\n";
        }
    };

    void write_update(SyncGraphExecutor &rExec, SyncGraph const& graph)
    {
        if (file.empty()) { return; }
        SyncGraphExecutorDebugInfo info(rExec);
        SyncGraphDOTVisualizer visualizer{.graph = graph, .pDebugInfo = &info};
        try
        {
            std::ostringstream os;
            os << "<UPDATE_GRAPH>\n" << rExec.startTime.time_since_epoch().count() << "\n" << visualizer << "\n</UPDATE_GRAPH>\n";
            *stream << os.str();
            stream->flush();
        }
        catch([[maybe_unused]] std::exception const& e)
        {
            std::cerr << "Failed to open file at OSP_FRAMEWORK_DEBUG_FILE\n";
        }
    };

    static SyncGraphExecutorDebugger& instance()
    {
        static SyncGraphExecutorDebugger instance = make();
        return instance;
    }

private:
    std::filesystem::path file;
    std::optional<std::ofstream> stream;
};


void SyncGraphExecutor::load(SyncGraph const& graph) noexcept
{
    auto const subgraphCapacity = graph.subgraphIds.capacity();
    perSubgraph     .resize(subgraphCapacity);
    subgraphsMoving .resize(subgraphCapacity);
    justMoved       .resize(subgraphCapacity);

    auto const syncCapacity = graph.syncIds.capacity();
    perSync         .resize(syncCapacity);

    for (SynchronizerId const syncId : graph.syncIds)
    {
        perSync[syncId].needToAdvance.resize(syncCapacity);
    }

    for(SubgraphId const subgraphId : graph.subgraphIds)
    {
        Subgraph      const &rSubgraph     = graph.subgraphs[subgraphId];
        PerSubgraph         &rExecSubgraph = perSubgraph[subgraphId];
        SubgraphType  const &sgtype        = graph.sgtypes[rSubgraph.instanceOf];

        rExecSubgraph.activeCycle = sgtype.initialCycle;
        rExecSubgraph.position    = sgtype.initialPos;
        rExecSubgraph.point       = sgtype.cycles[sgtype.initialCycle].path[sgtype.initialPos];

        justMoved.insert(subgraphId);
    }

    startTime = std::chrono::high_resolution_clock::now();

    SyncGraphExecutorDebugger::instance().write_new(*this, graph);
}

bool SyncGraphExecutor::update(std::vector<SynchronizerId> &rJustAlignedOut, SyncGraph const& graph) noexcept
{
    bool somethingHappened = false;

    somethingHappened |= ! alignedDuringEnable.empty();

    rJustAlignedOut.insert(rJustAlignedOut.end(), alignedDuringEnable.begin(), alignedDuringEnable.end());
    alignedDuringEnable.clear();

    for (SubgraphId const subgraphId : justMoved)
    {
        Subgraph      const &rSubgraph     = graph.subgraphs[subgraphId];
        PerSubgraph   const &rExecSubgraph = perSubgraph[subgraphId];
        SubgraphType  const &sgtype        = graph.sgtypes[rSubgraph.instanceOf];
        LocalPointId  const point          = sgtype.cycles[rExecSubgraph.activeCycle].path[rExecSubgraph.position];

        for (SynchronizerId const syncId : rSubgraph.points[point].connectedSyncs)
        {
            PerSync &rExecSync = perSync[syncId];
            if (rExecSync.state != ESyncState::Inactive)
            {
                subgraphsMoving.erase(subgraphId);
                --rExecSync.pointsNotAligned;
                if (rExecSync.pointsNotAligned == 0)
                {
                    // lock
                    rJustAlignedOut.push_back(syncId);
                    rExecSync.state = ESyncState::WaitForUnlock;
                    somethingHappened = true;
                }
            }
        }
    }
    justMoved.clear();

    // Eliminate invalid subgraphs from tryToAdvance
    for (SubgraphId const subgraphId : subgraphsMoving)
    {
        Subgraph      const &rSubgraph     = graph.subgraphs[subgraphId];
        PerSubgraph   const &rExecSubgraph = perSubgraph[subgraphId];

        LGRN_ASSERTMV(rExecSubgraph.activeSyncs != 0, "all syncs disabled, sync should have already been removed from subgraphsMoving in batch(SetDisable, ...)", subgraphId.value);

        SubgraphType  const &sgtype        = graph.sgtypes[rSubgraph.instanceOf];
        LocalPointId  const point          = sgtype.cycles[rExecSubgraph.activeCycle].path[rExecSubgraph.position];

        for (SynchronizerId const syncId : rSubgraph.points[point].connectedSyncs)
        {
            Synchronizer const &rSync     = graph.syncs[syncId];
            PerSync      const &rExecSync = perSync[syncId];

            if (rExecSync.state == ESyncState::WaitForAlign)
            {
                // Sync is aligned with the current point, and wants this subgraph to stay at
                // its current position and wait for other subgraphs to align
                subgraphsMoving.erase(subgraphId);
            }
            else if (rExecSync.state == ESyncState::WaitForUnlock)
            {
                // Sync is locked (task in progress). don't move!
                subgraphsMoving.erase(subgraphId);
            }
            else if (     rExecSync.state == ESyncState::WaitForAdvance
                     && ! rExecSync.needToAdvance.contains(subgraphId))
            {
                // only happens when a cycle has only 1 state to loop through
                subgraphsMoving.erase(subgraphId);
            }
        }
    }

    // Advance subgraphs to next point
    for (SubgraphId const subgraphId : subgraphsMoving)
    {
        Subgraph      const &rSubgraph     = graph.subgraphs[subgraphId];
        SubgraphType  const &sgtype        = graph.sgtypes[rSubgraph.instanceOf];
        PerSubgraph         &rExecSubgraph = perSubgraph[subgraphId];
        LocalPointId  const point          = sgtype.cycles[rExecSubgraph.activeCycle].path[rExecSubgraph.position];

        // subgraph is moving to the next point. clear self from 'needToAdvance' from all connected points
        for (SynchronizerId const syncId : graph.subgraphs[subgraphId].points[point].connectedSyncs)
        {
            PerSync &rExecSync = perSync[syncId];

            if (rExecSync.state == ESyncState::WaitForAdvance)
            {
                rExecSync.needToAdvance.erase(subgraphId);

                if (rExecSync.needToAdvance.empty())
                {
                    // done advancing all connected subgraphs
                    rExecSync.state = ESyncState::WaitForAlign;
                }
            }
        }

        justMoved.insert(subgraphId);

        if (rExecSubgraph.jumpNextCycle.has_value())
        {
            // Advance to next point based on rExecSubgraph.jumpNext*
            rExecSubgraph.activeCycle   = rExecSubgraph.jumpNextCycle;
            rExecSubgraph.position      = rExecSubgraph.jumpNextPos;

            SubgraphType::Cycle const& cycle = sgtype.cycles[rExecSubgraph.activeCycle];
            rExecSubgraph.point         = cycle.path[rExecSubgraph.position];
            rExecSubgraph.jumpNextCycle = {};
        }
        else
        {
            // Advance to next point following the active selected cycle
            SubgraphType::Cycle const& cycle = sgtype.cycles[rExecSubgraph.activeCycle];

            rExecSubgraph.position ++;
            if (rExecSubgraph.position == cycle.path.size())
            {
                rExecSubgraph.position = 0;
            }
            rExecSubgraph.point = cycle.path[rExecSubgraph.position];
        }
    }

    somethingHappened |= !subgraphsMoving.empty();

    if (somethingHappened)
    {
        SyncGraphExecutorDebugger::instance().write_update(*this, graph);
    }

    return somethingHappened;
}

void SyncGraphExecutor::batch(ESyncAction const action, osp::ArrayView<SynchronizerId const> const syncs, SyncGraph const& graph)
{
    for (SynchronizerId const syncId : syncs)
    {
        Synchronizer const &rSync = graph.syncs[syncId];
        PerSync &rExecSync = perSync[syncId];
        switch(action)
        {
        case ESyncAction::SetEnable:
            if (rExecSync.state == ESyncState::Inactive)
            {
                rExecSync.state            = ESyncState::WaitForAlign;
                rExecSync.pointsNotAligned = int(rSync.connectedPoints.size());

                for (SubgraphPointAddr const addr : rSync.connectedPoints)
                {
                    PerSubgraph &rExecSubgraph = perSubgraph[addr.subgraph];
                    if (rExecSubgraph.activeSyncs == 0)
                    {
                        subgraphsMoving.insert(addr.subgraph);
                    }
                    ++rExecSubgraph.activeSyncs;

                    if ( ! justMoved.contains(addr.subgraph) && rExecSubgraph.point == addr.point )
                    {
                        --rExecSync.pointsNotAligned;
                    }
                }
                if (rExecSync.pointsNotAligned == 0)
                {
                    rExecSync.state = ESyncState::WaitForUnlock;
                    alignedDuringEnable.push_back(syncId);
                }
            }
            break;
        case ESyncAction::SetDisable:
            rExecSync.pointsNotAligned = 0;
            if (rExecSync.state == ESyncState::WaitForAdvance)
            {
                rExecSync.needToAdvance.clear();
            }
            if (rExecSync.state != ESyncState::Inactive)
            {
                rExecSync.state = ESyncState::Inactive;

                for (SubgraphPointAddr const& addr : graph.syncs[syncId].connectedPoints)
                {
                    PerSubgraph &rExecSubgraph = perSubgraph[addr.subgraph];
                    --rExecSubgraph.activeSyncs;
                    if (rExecSubgraph.activeSyncs == 0)
                    {
                        subgraphsMoving.erase(addr.subgraph);
                    }
                    else
                    {
                        subgraphsMoving.insert(addr.subgraph);
                    }
                }
            }
            break;
        case ESyncAction::Unlock:
            LGRN_ASSERT(rExecSync.state == ESyncState::WaitForUnlock);
            rExecSync.state            = ESyncState::WaitForAdvance;
            rExecSync.pointsNotAligned = int(rSync.connectedPoints.size());

            for (SubgraphPointAddr const addr : graph.syncs[syncId].connectedPoints)
            {
                rExecSync.needToAdvance.insert(addr.subgraph);
                subgraphsMoving.insert(addr.subgraph);
            }
            break;
        }
    }
}

void SyncGraphExecutor::batch(ESubgraphAction const action, osp::ArrayView<SubgraphId const> const subgraphs, SyncGraph const& graph)
{
    for (SubgraphId const subgraphId : subgraphs)
    {
        Subgraph    const &rSubgraph     = graph.subgraphs[subgraphId];
        PerSubgraph       &rExecSubgraph = perSubgraph[subgraphId];
        switch(action)
        {
        case ESubgraphAction::Reset:
            rExecSubgraph.position = 0;
            rExecSubgraph.point = graph.sgtypes[rSubgraph.instanceOf].cycles[rExecSubgraph.activeCycle].path[0];
            break;
        }
    }
}

bool SyncGraphExecutor::select_cycle(SubgraphId const subgraphId, LocalCycleId const cycleId, SyncGraph const& graph) noexcept
{
    Subgraph          const &rSubgraph    = graph.subgraphs[subgraphId];
    PerSubgraph             &rPerSubgraph = perSubgraph[subgraphId];
    SubgraphType      const &sgtype       = graph.sgtypes[rSubgraph.instanceOf];
    LocalPointId      const currentPoint  = sgtype.cycles[rPerSubgraph.activeCycle].path[rPerSubgraph.position];

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


}; // namespace osp::exec
