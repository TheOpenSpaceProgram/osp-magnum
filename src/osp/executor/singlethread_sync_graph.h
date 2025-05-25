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

#include "sync_graph.h"

#include <longeron/id_management/id_set_stl.hpp>

#include <fstream>
#include <filesystem>

#include <chrono>

#include <iostream>
#include <optional>

namespace osp::exec
{





struct SyncGraphExecutor
{
    enum class ESyncState : std::int8_t { Inactive, WaitForAlign, WaitForUnlock, WaitForAdvance };

    enum class ESyncAction : std::int8_t { SetEnable, SetDisable, Unlock };
    enum class ESubgraphAction : std::int8_t { Reset };

    struct PerSubgraph
    {
        LocalCycleId activeCycle;
        LocalCycleId jumpNextCycle;
        std::uint8_t position;
        std::uint8_t jumpNextPos;
    };
    struct PerSync
    {
        lgrn::IdSetStl<SubgraphId>          needToAdvance;
        ESyncState                          state               {ESyncState::Inactive};
    };

    void load(SyncGraph const& graph) noexcept;

    bool update(std::vector<SynchronizerId> &rJustAlignedOut, SyncGraph const& graph) noexcept;

    bool is_locked(SynchronizerId syncId, SyncGraph const& graph) const noexcept
    {
        return perSync[syncId].state == ESyncState::WaitForUnlock;
    }

    bool select_cycle(SubgraphId const subgraphId, LocalCycleId const cycleId, SyncGraph const& graph) noexcept
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

    bool jump(SubgraphId const subgraphId, LocalCycleId const cycleId, std::uint8_t position, SyncGraph const& graph)
    {
        PerSubgraph             &rPerSubgraph = perSubgraph[subgraphId];
        rPerSubgraph.jumpNextCycle = cycleId;
        rPerSubgraph.jumpNextPos   = position;
//        Subgraph          const &rSubgraph    = graph.subgraphs[subgraphId];
//        PerSubgraph             &rPerSubgraph = perSubgraph[subgraphId];
//        SubgraphType      const &sgtype       = graph.sgtypes[rSubgraph.instanceOf];

//        [[maybe_unused]] auto const is_current_point_syncs_all_disabled = [&] () {
//            LocalPointId const currentPoint  = sgtype.cycles[rPerSubgraph.activeCycle].path[rPerSubgraph.position];

//            for (SynchronizerId const syncId : rSubgraph.points[currentPoint].connectedSyncs)
//            {
//                if (perSync[syncId].state != ESyncState::Inactive) { return false; }
//            }

//            return true;
//        };
//        LGRN_ASSERT(is_current_point_syncs_all_disabled());

//        rPerSubgraph.activeCycle = cycleId;
//        rPerSubgraph.position    = position;

        return true;
    }

    void batch(ESyncAction const action, osp::ArrayView<SynchronizerId const> const syncs, SyncGraph const& graph)
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

    void batch(ESyncAction const action, std::initializer_list<SynchronizerId const> const syncs, SyncGraph const& graph)
    {
        batch(action, osp::arrayView(syncs), graph);
    }

    void batch(ESubgraphAction const action, osp::ArrayView<SubgraphId const> const subgraphs, SyncGraph const& graph)
    {
        for (SubgraphId const subgraphId : subgraphs)
        {
            switch(action)
            {
            case ESubgraphAction::Reset:
                perSubgraph[subgraphId].position = 0;
                break;
            }
        }
    }

    lgrn::IdSetStl<SubgraphId>              toCycle;
    std::vector<SubgraphId>                 toCyclcErase;

    osp::KeyedVec<SubgraphId, PerSubgraph>  perSubgraph;
    osp::KeyedVec<SynchronizerId, PerSync>  perSync;

    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
};


class SyncGraphExecutorDebugInfo : public ISyncGraphDebugInfo
{
public:

    SyncGraphExecutorDebugInfo(SyncGraphExecutor const &exec) : m_exec{exec} { }

    bool            is_sync_enabled(SyncGraph const& graph, SynchronizerId syncId) override
    {
        return m_exec.perSync[syncId].state != SyncGraphExecutor::ESyncState::Inactive;
    }
    bool            is_sync_locked(SyncGraph const& graph, SynchronizerId syncId) override
    {
        return m_exec.is_locked(syncId, graph);
    }
    LocalPointId    current_point(SyncGraph const& graph, SubgraphId subgraphId) override
    {
        SyncGraphExecutor::PerSubgraph const &rPerSubgraph = m_exec.perSubgraph[subgraphId];
        return graph.sgtypes[graph.subgraphs[subgraphId].instanceOf].cycles[rPerSubgraph.activeCycle].path[rPerSubgraph.position];
    }

    SyncGraphExecutor const &m_exec;
};

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
            os << "<NEW_GRAPH>\n" << rExec.startTime.time_since_epoch() << "\n" << visualizer << "\n</NEW_GRAPH>\n";
            *stream << os.view();
            stream->flush();
        }
        catch(const std::exception &e)
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
            os << "<UPDATE_GRAPH>\n" << rExec.startTime.time_since_epoch() << "\n" << visualizer << "\n</UPDATE_GRAPH>\n";
            *stream << os.view();
            stream->flush();
        }
        catch(const std::exception &e)
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

    startTime = std::chrono::high_resolution_clock::now();

    SyncGraphExecutorDebugger::instance().write_new(*this, graph);
}

bool SyncGraphExecutor::update(std::vector<SynchronizerId> &rJustAlignedOut, SyncGraph const& graph) noexcept
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
        PerSync &rExecSync = perSync[syncId];

        if (rExecSync.state == ESyncState::WaitForAlign)
        {
            bool aligned = true;
            for (SubgraphPointAddr addr : rSync.connectedPoints)
            {
                Subgraph                const &rSubgraph = graph.subgraphs[addr.subgraph];
                PerSubgraph             const &rExecSubgraph = perSubgraph[addr.subgraph];
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
                rExecSync.state = ESyncState::WaitForUnlock;
                rJustAlignedOut.push_back(syncId);
                somethingHappened = true;
            }
        }
        else if (rExecSync.state == ESyncState::WaitForAdvance)
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
        PerSubgraph const &rExecSubgraph = perSubgraph[subgraphId];
        SubgraphType          const &sgtype        = graph.sgtypes[rSubgraph.instanceOf];
        LocalPointId          const point          = sgtype.cycles[rExecSubgraph.activeCycle].path[rExecSubgraph.position];

        for (SynchronizerId const syncId : rSubgraph.points[point].connectedSyncs)
        {
            Synchronizer const &rSync = graph.syncs[syncId];
            PerSync const& rExecSync = perSync[syncId];

            if (rExecSync.state == ESyncState::WaitForAlign)
            {
                // Sync is aligned with the current point, and wants this subgraph to stay at
                // its current position and wait for other subgraphs to align
                toCyclcErase.push_back(subgraphId);
            }
            else if (rExecSync.state == ESyncState::WaitForUnlock)
            {
                // Sync is locked (task in progress). don't move!
                toCyclcErase.push_back(subgraphId);
            }
            else if (     rExecSync.state == ESyncState::WaitForAdvance
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
        Subgraph                const &rSubgraph     = graph.subgraphs[subgraphId];
        SubgraphType            const &sgtype        = graph.sgtypes[rSubgraph.instanceOf];
        PerSubgraph                   &rExecSubgraph = perSubgraph[subgraphId];
        LocalPointId            const point          = sgtype.cycles[rExecSubgraph.activeCycle].path[rExecSubgraph.position];

        // subgraph is moving to the next point. clear self from 'needToAdvance' from all connected points
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


        if (rExecSubgraph.jumpNextCycle.has_value())
        {
            rExecSubgraph.activeCycle   = rExecSubgraph.jumpNextCycle;
            rExecSubgraph.position      = rExecSubgraph.jumpNextPos;

            rExecSubgraph.jumpNextCycle = {};
        }
        else
        {
            rExecSubgraph.position ++;
            if (rExecSubgraph.position == sgtype.cycles[rExecSubgraph.activeCycle].path.size())
            {
                rExecSubgraph.position = 0;
            }
        }


    }

    somethingHappened |= !toCycle.empty();

    if (somethingHappened)
    {
        SyncGraphExecutorDebugger::instance().write_update(*this, graph);
    }

    return somethingHappened;
}


} // namespace osp::exec
