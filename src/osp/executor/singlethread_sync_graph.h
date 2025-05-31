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


class SyncGraphExecutor
{
public:
    enum class ESyncState : std::int8_t { Inactive, WaitForAlign, WaitForUnlock, WaitForAdvance };

    enum class ESyncAction : std::int8_t { SetEnable, SetDisable, Unlock };
    enum class ESubgraphAction : std::int8_t { Reset };

    struct PerSubgraph
    {
        int          activeSyncs    {0};
        LocalCycleId activeCycle;
        LocalCycleId jumpNextCycle;
        LocalPointId point;
        std::uint8_t position       {};
        std::uint8_t jumpNextPos    {};
    };
    struct PerSync
    {
        lgrn::IdSetStl<SubgraphId>  needToAdvance;
        ESyncState                  state           {ESyncState::Inactive};
        int                         pointsNotAligned;
    };

    void load(SyncGraph const& graph) noexcept;

    bool update(std::vector<SynchronizerId> &rJustAlignedOut, SyncGraph const& graph) noexcept;

    void batch(ESyncAction const action, osp::ArrayView<SynchronizerId const> const syncs, SyncGraph const& graph);

    void batch(ESyncAction const action, std::initializer_list<SynchronizerId const> const syncs, SyncGraph const& graph)
    {
        batch(action, osp::arrayView(syncs), graph);
    }

    void batch(ESubgraphAction const action, osp::ArrayView<SubgraphId const> const subgraphs, SyncGraph const& graph);

    bool select_cycle(SubgraphId const subgraphId, LocalCycleId const cycleId, SyncGraph const& graph) noexcept;

    void jump(SubgraphId const subgraphId, LocalCycleId const cycleId, std::uint8_t position, SyncGraph const& graph)
    {
        PerSubgraph &rPerSubgraph = perSubgraph[subgraphId];
        rPerSubgraph.jumpNextCycle = cycleId;
        rPerSubgraph.jumpNextPos   = position;
    }

    bool is_locked(SynchronizerId syncId, SyncGraph const& graph) const noexcept
    {
        return perSync[syncId].state == ESyncState::WaitForUnlock;
    }

    lgrn::IdSetStl<SubgraphId>              subgraphsMoving;
    lgrn::IdSetStl<SubgraphId>              justMoved;

    osp::KeyedVec<SubgraphId, PerSubgraph>  perSubgraph;
    osp::KeyedVec<SynchronizerId, PerSync>  perSync;

    std::vector<SynchronizerId>             alignedDuringEnable;

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
        return m_exec.perSync[syncId].state == SyncGraphExecutor::ESyncState::WaitForUnlock;
    }

    LocalPointId    current_point(SyncGraph const& graph, SubgraphId subgraphId) override
    {
        return m_exec.perSubgraph[subgraphId].point;
    }

    SyncGraphExecutor const &m_exec;
};


} // namespace osp::exec
