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
#pragma once

#include "sync_graph.h"
#include "singlethread_sync_graph.h"

#include <osp/framework/framework.h>
#include <osp/core/keyed_vector.h>

#include <spdlog/logger.h>
#include <spdlog/fmt/fmt.h>

namespace osp::exec
{

class SinglethreadFWExecutor final : public osp::fw::IExecutor
{
    using MaybeCancelId = osp::StrongId<std::uint32_t, struct DummyForMaybeCancelId>;
    using enum SyncGraphExecutor::ESyncAction;
    using enum SyncGraphExecutor::ESubgraphAction;

    // Rox - Read-only during execution
    // Wtx - Written-to as part of execution

    struct RoxPltype
    {
        SubgraphTypeId      sgtype;
        LocalPointId        schedulePoint;
        StageId             scheduleStage;
    };

    struct RoxLoopblk
    {
        std::vector<PipelineId>     pipelineChildren;
        std::vector<LoopBlockId>    loopblkChildren;

        std::vector<SynchronizerId> associatedTasks;
        std::vector<SynchronizerId> associatedOthers;
        std::vector<SynchronizerId> externals;

        SubgraphId          subgraph;
        SubgraphId          scheduleStatus;

        SynchronizerId      schedule;
        SynchronizerId      checkstop;
        SynchronizerId      left;
        SynchronizerId      right;
    };
    struct WtxLoopblk
    {
        enum class EState : std::uint8_t { NotRunning, ScheduledToRun, Running };

        int     pipelinesRunning{0};
        EState  state{false};
    };

    struct RoxPipeline
    {
        std::vector<TaskId>     cancelsTasks;
        SubgraphId              main;
        SubgraphId              scheduleStatus;
        SynchronizerId          schedule;
        StageId                 initialStage;
        unsigned int            syncCount{0};
    };
    struct WtxPipeline
    {
        bool isCanceled{true};
    };

    struct RoxTask
    {
        LoopBlockId         parent;
        SynchronizerId      main;

        SynchronizerId      sustainer;
        SynchronizerId      external;

        bool const is_spanning_nested_loopblocks() const { return sustainer.has_value(); }
    };
    struct WtxTask
    {
        bool blockedByExternal{false};
        bool waitingForExternal{false};
    };

    struct WtxSubgraph
    {
        enum class ETag : std::int8_t { Invalid, LoopBlock, Pipeline, ScheduleStatus };

        ETag tag{ETag::Invalid};

        union
        {
            std::uint32_t loopblkId{0xFFFFFFFF};
            std::uint32_t pipelineId;
        };
    };

    enum class ESyncType : std::int8_t { Invalid, BlkSchedule, BlkCheckStop, BlkLeft, BlkRight, Task, TaskExt, TaskSus, PlSchedule, PlScheduleExt, MaybeCancel };

    struct RoxSync
    {
        ESyncType       tag{ESyncType::Invalid};
        TaskId          taskId;
        PipelineId      pipelineId;
        LoopBlockId     loopBlk;
    };
    union WtxSync
    {
        struct
        {
            //int  inactiveBlocks;
            int  canceledByPipelines;
        };

        std::uint32_t loopBlk;
    };

    struct TaskWaitingForExternal
    {
        SynchronizerId  syncId;
    };

    struct TaskWaitingToFinish
    {
        TaskId          taskId;
        TaskActions     status;
        SynchronizerId  syncId;
        PipelineId      pipeline;
        LoopBlockId     loopblk;
    };

public:

    void load(osp::fw::Framework& rFW) override;

    void task_finish(osp::fw::Framework &rFW, osp::TaskId taskId, bool overrideStatus = false, TaskActions status = {}) override;

    void wait(osp::fw::Framework& rFW) override;

    bool is_running(osp::fw::Framework const& rFW, LoopBlockId loopblkId) override;

    std::shared_ptr<spdlog::logger> m_log;

private:

    void resize_fit_syncs();

    void resize_fit_subgraphs();

    void finish_schedule_block(LoopBlockId loopblkId, TaskActions status, osp::fw::Framework const& rFW);

    void finish_schedule_pipeline(PipelineId pipelineId, TaskId taskId, TaskActions const status, bool calledExternally, osp::fw::Framework const& rFW);

    void process_aligned_sync(SynchronizerId alignedSyncId, osp::fw::Framework& rFW);

    SyncGraph                               m_graph;
    KeyedVec<LoopBlockId, RoxLoopblk>       m_roxLoopblkOf;
    KeyedVec<PipelineId, RoxPipeline>       m_roxPipelineOf;
    KeyedVec<TaskId, RoxTask>               m_roxTaskOf;
    KeyedVec<SynchronizerId, RoxSync>       m_roxSyncOf;

    SyncGraphExecutor                       m_exec;
    std::vector<SynchronizerId>             m_justAligned;
    std::vector<SynchronizerId>             m_disableSyncs;
    std::vector<SubgraphId>                 m_reset;
    std::vector<TaskWaitingToFinish>        m_tasksWaiting;

    std::vector<entt::any>                  m_argumentRefs;
    KeyedVec<LoopBlockId, WtxLoopblk>       m_wtxLoopblkOf;
    KeyedVec<PipelineId, WtxPipeline>       m_wtxPipelineOf;
    KeyedVec<TaskId, WtxTask>               m_wtxTaskOf;
    KeyedVec<SubgraphId, WtxSubgraph>       m_wtxSubgraphOf;
    KeyedVec<SynchronizerId, WtxSync>       m_wtxSyncOf;
};

}
