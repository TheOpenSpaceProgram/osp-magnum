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

#include <chrono>

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

    void jump(SubgraphId const subgraphId, LocalCycleId const cycleId, std::uint8_t position, SyncGraph const& graph)
    {
        PerSubgraph &rPerSubgraph = perSubgraph[subgraphId];
        rPerSubgraph.jumpNextCycle = cycleId;
        rPerSubgraph.jumpNextPos   = position;
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


} // namespace osp::exec
