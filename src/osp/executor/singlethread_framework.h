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

#include <osp/framework/framework.h>

#include "sync_graph.h"
#include "singlethread_sync_graph.h"


#include <spdlog/logger.h>
#include <spdlog/fmt/fmt.h>

#include <memory>

#include <iostream>



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

        std::vector<SynchronizerId> maybeCancels;

        SubgraphId          subgraph;

        SynchronizerId      schedule;
        //SynchronizerId      enterexit;
        SynchronizerId      checkstop;
        SynchronizerId      left;
        SynchronizerId      right;
        SynchronizerId      finish;
    };
    struct WtxLoopblk
    {
        enum class EState : std::uint8_t { NotRunning, ScheduledToRun, Running, Exiting };

        int pipelinesRunning{0};
        EState state{false};
    };

    struct RoxPipeline
    {
        std::vector<TaskId>     cancelsTasks;
        SubgraphId              main;
        SubgraphId              scheduleStatus;
        SynchronizerId          schedule;
        StageId                 initialStage;

    };
    struct WtxPipeline
    {
        bool isCanceled{true};
    };

    struct RoxTask
    {
        SynchronizerId      main;
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

    enum class ESyncType : std::int8_t { Invalid, BlkSchedule, /*BlkEnterExit,*/ BlkCheckStop, BlkLeft, BlkRight, BlkFinish, Task, PlSchedule, MaybeCancel };

    struct RoxSync
    {
        ESyncType tag{ESyncType::Invalid};

        std::uint32_t taskId; /// for Task, Schedule
        std::uint32_t pipelineId; /// for Schedule
        std::uint32_t loopBlk; /// for all Blk*
    };
    struct WtxSync
    {
        union
        {
            struct
            {
                int  inactiveBlocks; /// for Task, Schedule, and MaybeCancel
                union
                {
                    int canceledByPipelines; /// for Task
                };
            };

            std::uint32_t loopBlk;
        };
    };

    struct TaskWaitingToFinish
    {
        TaskId          taskId;
        TaskActions     status;
        SynchronizerId  syncId;
        PipelineId      pipeline;
        LoopBlockId     loopblk;
        bool            neverCancel;
    };

    struct Jump
    {
        SubgraphId      subgraphId;
        LocalCycleId    cycle;
        std::uint8_t    position;

    };

    constexpr static LocalPointId point(int in) { return LocalPointId::from_index(in); }
    constexpr static LocalCycleId cycle(int in) { return LocalCycleId::from_index(in); };


public:

    template<typename VEC_T, typename VAL_T>
    static constexpr bool contains(VEC_T const& vec, VAL_T const& searchfor)
    {
                // add taskSync.task to LoopBlock's associated tasks
            // minor optimization: task syncs are added in bunches per-task, so it's likely that
            // the previous iteration already pushed the same TaskId. This makes searching in
            // reverse much faster.
        auto const &first = vec.rbegin();
        auto const &last  = vec.rend();
        return std::find(first, last, searchfor) != last;
    }

    void load(osp::fw::Framework& rFW) override
    {
        Tasks const& tasks = rFW.m_tasks;

        osp::KeyedVec<PipelineId, StageId> stageChanges;

        auto const& pltypereg = PipelineTypeIdReg::instance();

        for (LoopBlockId const loopblkId : tasks.loopblkIds)
        {
            TaskOrderReport report;
            check_task_order(tasks, report, arrayView(&loopblkId, 1));

            std::ostringstream os;
            auto        it    = report.steps.begin();
            auto const  &last = report.steps.end();
            int         time  = 0;

            int count = 0;
            for(PipelineId const pipelineId : tasks.pipelineIds)
            {
                Pipeline const& pipeline = tasks.pipelineInst[pipelineId];
                if (pipeline.block == loopblkId)
                {
                    for (int i = 0; i < count; ++i)
                    {
                        std::cout << "\U00002502 ";
                    }

                    std::cout << "\U0000250D" << pipeline.name << "\n";
                    ++count;
                }

            }

            while(it != last)
            {
                stageChanges.clear();
                stageChanges.resize(tasks.pipelineIds.capacity());
                while(it != last && it->pipelineId.has_value() && it->time == time)
                {
                    stageChanges[it->pipelineId] = it->stageId;
                    ++it;
                }

                for(PipelineId const pipelineId : tasks.pipelineIds)
                {
                    if (tasks.pipelineInst[pipelineId].block == loopblkId)
                    {
                        StageId const stageChange = stageChanges[pipelineId];
                        if (stageChange.has_value())
                        {
                            std::cout << int(stageChange.value) << " ";
                        }
                        else
                        {
                            std::cout << "\U00002502 ";
                        }
                    }

                }

                std::cout << "\n";

                while(it != last && it->taskId.has_value() && it->time == time)
                {
                    for(PipelineId const pipelineId : tasks.pipelineIds)
                    {
                        if (tasks.pipelineInst[pipelineId].block == loopblkId)
                        {
                            if (tasks.pipelineInst[pipelineId].scheduleCondition == it->taskId)
                            {
                                std::cout << "\U000025C9\U00002500";
                            }
                            else if (std::find_if(
                                            tasks.syncs.begin(), tasks.syncs.end(),
                                            [taskId = it->taskId, pipelineId] (TaskSyncToPipeline const& sync)
                                            { return sync.task == taskId && sync.pipeline == pipelineId; })
                                     != tasks.syncs.end())
                            {
                                std::cout << "\U000025CF\U00002500";
                            }
                            else
                            {
                                std::cout << "\U0000253C\U00002500";
                            }
                        }
                    }
                    std::cout << "" <<  rFW.m_taskImpl[it->taskId].debugName << "\n";
                    ++it;
                }

                ++time;

            }

            bool noError = report.failedLocked.empty() && report.failedNotAdded.empty();

            if ( ! noError )
            {
                std::cout << "Deadlocked tasks:\n";
                for (TaskId const taskId : report.failedLocked)
                {
                    for(PipelineId const pipelineId : tasks.pipelineIds)
                    {
                        if (tasks.pipelineInst[pipelineId].block == loopblkId)
                        {
                            Pipeline const& pipeline = tasks.pipelineInst[pipelineId];
                            if (pipeline.scheduleCondition == taskId)
                            {
                                auto const &pltype = pltypereg.get(pipeline.type);
                                auto stageIndex = std::distance(
                                        std::find_if(pltype.stages.begin(), pltype.stages.end(),
                                                     [] (PipelineTypeInfo::StageInfo const &stage) { return stage.isSchedule; } ),
                                        pltype.stages.end());
                                std::cout << stageIndex << "\U00002500";
                            }
                            else if (auto const syncIt = std::find_if(
                                            tasks.syncs.begin(), tasks.syncs.end(),
                                            [taskId, pipelineId] (TaskSyncToPipeline const& sync)
                                            { return sync.task == taskId && sync.pipeline == pipelineId; });
                                    syncIt != tasks.syncs.end())
                            {
                                StageId const lastStage = std::find_if(
                                        report.steps.rbegin(), report.steps.rend(),
                                        [pipelineId] (TaskOrderReport::Step const& step)
                                        { return step.pipelineId == pipelineId; })->stageId;
                                if (lastStage == syncIt->stage)
                                {
                                    std::cout << "\U000025CF\U00002500";
                                }
                                else
                                {
                                    std::cout << int(syncIt->stage.value) << "\U00002500";
                                }
                            }
                            else
                            {
                                std::cout << "\U00002534\U00002500";
                            }
                        }
                    }
                    std::cout << "" <<  rFW.m_taskImpl[taskId].debugName << "\n";
                }

                std::cout << "Not reached:\n";

                for (TaskId const taskId : report.failedNotAdded)
                {
                    for(PipelineId const pipelineId : tasks.pipelineIds)
                    {
                        if (tasks.pipelineInst[pipelineId].block == loopblkId)
                        {
                            Pipeline const& pipeline = tasks.pipelineInst[pipelineId];
                            if (pipeline.scheduleCondition == taskId)
                            {
                                auto const &pltype = pltypereg.get(pipeline.type);
                                auto stageIndex = std::distance(
                                        std::find_if(pltype.stages.begin(), pltype.stages.end(),
                                                     [] (PipelineTypeInfo::StageInfo const &stage) { return stage.isSchedule; } ),
                                        pltype.stages.end());
                                std::cout << stageIndex << "\U00002500";
                            }
                            else if (auto const syncIt = std::find_if(
                                            tasks.syncs.begin(), tasks.syncs.end(),
                                            [taskId, pipelineId] (TaskSyncToPipeline const& sync)
                                            { return sync.task == taskId && sync.pipeline == pipelineId; });
                                    syncIt != tasks.syncs.end())
                            {
                                std::cout << int(syncIt->stage.value) << "\U00002500";
                            }
                            else
                            {
                                std::cout << "\U00002534\U00002500";
                            }
                        }
                    }
                    std::cout << "" <<  rFW.m_taskImpl[taskId].debugName << "\n";
                }

            }




            LGRN_ASSERTM(noError, "deadlock");
        }

        RoxSync sad;

        // IdRegistryStl doesn't have a clear() yet. just leak implmentation details and set all bits to 1.

        m_graph.sgtypeIds   .bitview().set();
        m_graph.sgtypes     .clear();
        m_graph.subgraphIds .bitview().set();
        m_graph.subgraphs   .clear();
        m_graph.syncIds     .bitview().set();
        m_graph.syncs       .clear();
        m_exec.perSubgraph  .clear();
        m_exec.perSync      .clear();
        m_exec.toCyclcErase .clear();
        m_exec.toCycle      .clear();
        m_roxLoopblkOf      .clear();
        m_roxPipelineOf     .clear();
        m_roxSyncOf         .clear();
        m_wtxSubgraphOf     .clear();
        m_wtxSyncOf         .clear();
        m_roxTaskOf         .clear();
        m_tasksWaiting      .clear();

        m_roxTaskOf         .resize(tasks.taskIds.capacity());
        m_roxLoopblkOf      .resize(tasks.loopblkIds.capacity());
        m_wtxLoopblkOf      .resize(tasks.loopblkIds.capacity());
        m_roxPipelineOf     .resize(tasks.pipelineIds.capacity());
        m_wtxPipelineOf     .resize(tasks.pipelineIds.capacity());



        // # Make SubgraphTypes: BlockController and Status

        SubgraphTypeId const sgtBlkCtrlId    = m_graph.sgtypeIds.create();
        SubgraphTypeId const sgtSingleStatId = m_graph.sgtypeIds.create();
        m_graph.sgtypes.resize(m_graph.sgtypeIds.capacity());

        LocalPointId const blkctrlStart     = point(0);
        LocalPointId const blkctrlSchedule  = point(1);
        LocalPointId const blkctrlRunning   = point(2);
        LocalPointId const blkctrlFinish    = point(3);

        {
            SubgraphType &rSgtype = m_graph.sgtypes[sgtBlkCtrlId];
            rSgtype.debugName = "BlockController";
            rSgtype.points = {{
                {.debugName = "Start"},     // point(0)
                {.debugName = "Schedule"},  // point(1)
                {.debugName = "Running"},   // point(2)
                {.debugName = "Finish"}     // point(3)
            }};
            rSgtype.cycles = {{
                {.debugName = "Control",  .path = {blkctrlStart, blkctrlSchedule, blkctrlRunning, blkctrlFinish}}
            }};
            rSgtype.initialCycle = cycle(0);
            rSgtype.initialPos   = 0;
        }

        {
            SubgraphType &rSgtype = m_graph.sgtypes[sgtSingleStatId];
            rSgtype.debugName = "SingleTaskStatus";
            rSgtype.points = {{
                {.debugName = "Run"},       // point(0)
                {.debugName = "Done"},      // point(1)
            }};
            rSgtype.cycles = {{
                {.debugName = "Control",  .path = {point(0), point(1)}}
            }};
            rSgtype.initialCycle = cycle(0);
            rSgtype.initialPos   = 0;
        }

        // # Add SubgraphType corresponding to each global PipelineType

        auto const &rPltypeReg = PipelineTypeIdReg::instance();

        KeyedVec<PipelineTypeId, RoxPltype> roxPltypeOf;
        roxPltypeOf.resize(rPltypeReg.ids().size());

        for (PipelineTypeId pltypeId : rPltypeReg.ids())
        {
            PipelineTypeInfo const &rPltypeInfo = rPltypeReg.get(pltypeId);
            auto             const stageCount   = rPltypeInfo.stages.size();

            SubgraphTypeId const sgtypeId = m_graph.sgtypeIds.create();
            m_graph.sgtypes.resize(m_graph.sgtypeIds.capacity());

            SubgraphType &rSgtype = m_graph.sgtypes[sgtypeId];
            rSgtype.debugName = rPltypeInfo.debugName;

            std::vector<LocalPointId> runningPath;
            runningPath.reserve(stageCount);

            LocalPointId const start  = point(0);
            LocalPointId const finish = point(stageCount+1u);
            LocalPointId schedulePoint;
            StageId      scheduleStage;

            // arrange as points as [Start, PipelineStage0, PipelineStage1, ..., Finish]
            rSgtype.points.resize(stageCount+2u);
            rSgtype.points[start].debugName = "Start";
            for (std::size_t i = 0; i < stageCount; ++i)
            {
                StageId      const stageId = StageId::from_index(i);
                LocalPointId const stagePoint = point(i + 1u);
                runningPath.push_back(stagePoint);
                rSgtype.points[stagePoint].debugName = rPltypeInfo.stages[stageId].name;

                if (rPltypeInfo.stages[stageId].isSchedule)
                {
                    LGRN_ASSERT(!schedulePoint.has_value());
                    schedulePoint = stagePoint;
                    scheduleStage = stageId;
                }
            }
            LGRN_ASSERTMV(schedulePoint.has_value(), "Missing Schedule stage", rPltypeInfo.debugName);
            rSgtype.points[finish].debugName = "Finish";

            rSgtype.cycles = {{
                {.debugName = "Control",  .path = {start, finish}}, // cycle(0)
                {.debugName = "Running",  .path = std::move(runningPath)},    // cycle(1)
                //{.debugName = "Canceled", .path = {schedule}},                // cycle(2)
            }};

            rSgtype.initialCycle = cycle(0);
            rSgtype.initialPos   = 0;

            roxPltypeOf[pltypeId] = {
                .sgtype         = sgtypeId,
                .schedulePoint  = schedulePoint,
                .scheduleStage  = scheduleStage
            };
        }

        // Done adding sgtypes. References from here are now stable.
        SubgraphType &rBlkCtrl = m_graph.sgtypes[sgtBlkCtrlId];

        // # Add BlockController Subgraph for each task LoopBlock


        for (LoopBlockId const loopblkId : tasks.loopblkIds)
        {
            LoopBlock     const &loopblk           = tasks.loopblkInst[loopblkId];
            bool          const hasDefaultSchedule = ! loopblk.scheduleCondition.has_value();

            m_roxLoopblkOf[loopblkId] = {
                .subgraph       = m_graph.subgraphIds.create(),
                .schedule       = hasDefaultSchedule ? m_graph.syncIds.create() : SynchronizerId{},
                //.enterexit      = m_graph.syncIds.create(),
                .checkstop      = m_graph.syncIds.create(),
                .left           = m_graph.syncIds.create(),
                .right          = m_graph.syncIds.create(),
                .finish         = m_graph.syncIds.create()
            };

        }

        resize_fit_syncs();
        resize_fit_subgraphs();

        for (LoopBlockId const loopblkId : tasks.loopblkIds)
        {
            LoopBlock     const &loopblk           = tasks.loopblkInst[loopblkId];
            bool          const hasDefaultSchedule = ! loopblk.scheduleCondition.has_value();
            RoxLoopblk    const &roxLoopblk     = m_roxLoopblkOf[loopblkId];

            Subgraph &rSubgraph = m_graph.subgraphs[roxLoopblk.subgraph];
            rSubgraph.instanceOf = sgtBlkCtrlId;
            rSubgraph.debugName = fmt::format("BC{}", loopblkId.value);
            rSubgraph.points.clear();
            rSubgraph.points.resize(m_graph.sgtypes[sgtBlkCtrlId].points.size());

            m_wtxSubgraphOf[roxLoopblk.subgraph] = {
                .tag        = WtxSubgraph::ETag::LoopBlock,
                .loopblkId  = loopblkId.value
            };

            if ( hasDefaultSchedule )
            {
                Synchronizer &rSchedule = m_graph.syncs[roxLoopblk.schedule];
                rSchedule    .debugName = fmt::format("BC{} DefaultSchedule", loopblkId.value);
                m_roxSyncOf[roxLoopblk.schedule] = {
                    .tag = ESyncType::BlkSchedule,
                    .taskId = TaskId{}.value,
                    .loopBlk = loopblkId.value
                };
                // connection happens later in "Connect LoopBlock schedule tasks" below
            }

            //Synchronizer &rEnterexit    = m_graph.syncs[roxLoopblk.enterexit];
            Synchronizer &rCheckstop    = m_graph.syncs[roxLoopblk.checkstop];
            Synchronizer &rLeft         = m_graph.syncs[roxLoopblk.left];
            Synchronizer &rRight        = m_graph.syncs[roxLoopblk.right];
            Synchronizer &rFinish       = m_graph.syncs[roxLoopblk.finish];

            //rEnterexit  .debugName = fmt::format("BC{} Enter/Exit", loopblkId.value);
            rCheckstop  .debugName = fmt::format("BC{} Check-Stop", loopblkId.value);
            rLeft       .debugName = fmt::format("BC{} Left",       loopblkId.value);
            rRight      .debugName = fmt::format("BC{} Right",      loopblkId.value);
            rFinish     .debugName = fmt::format("BC{} Finish",     loopblkId.value);
            rLeft       .debugGraphStraight = true;
            rCheckstop  .debugGraphLoose = true;
            rRight      .debugGraphLoose = true;
            //rEnterexit  .debugGraphLoose = true;

            //m_roxSyncOf[roxLoopblk.enterexit]   = {.tag = ESyncType::BlkEnterExit, .loopBlk = loopblkId.value};
            m_roxSyncOf[roxLoopblk.checkstop]   = {.tag = ESyncType::BlkCheckStop, .loopBlk = loopblkId.value};
            m_roxSyncOf[roxLoopblk.left]        = {.tag = ESyncType::BlkLeft,      .loopBlk = loopblkId.value};
            m_roxSyncOf[roxLoopblk.right]       = {.tag = ESyncType::BlkRight,     .loopBlk = loopblkId.value};
            m_roxSyncOf[roxLoopblk.finish]      = {.tag = ESyncType::BlkFinish,    .loopBlk = loopblkId.value};

            m_graph.connect({.sync = roxLoopblk.left,   .subgraphPoint = {roxLoopblk.subgraph, blkctrlRunning}});
            m_graph.connect({.sync = roxLoopblk.right,  .subgraphPoint = {roxLoopblk.subgraph, blkctrlRunning}});
            m_graph.connect({.sync = roxLoopblk.finish, .subgraphPoint = {roxLoopblk.subgraph, blkctrlFinish}});
        }

        // TODO: set parents between loop blocks
        for (LoopBlockId const loopblk : tasks.loopblkIds)
        {

        }

        // # Add pipelines


        for (PipelineId const pipelineId : tasks.pipelineIds)
        {
            Pipeline  const &pipeline       = tasks.pipelineInst[pipelineId];
            RoxPltype const &roxPltype      = roxPltypeOf[pipeline.type];

            bool     const hasDefaultSchedule = ! pipeline.scheduleCondition.has_value();

            LGRN_ASSERTM(pipeline.initialStage.has_value(), "pipeline has no initial stage set");

            m_roxPipelineOf[pipelineId] = {
                .main           = m_graph.subgraphIds.create(),
                .scheduleStatus = m_graph.subgraphIds.create(),
                .schedule       = hasDefaultSchedule ? m_graph.syncIds.create() : SynchronizerId{},
                .initialStage   = pipeline.initialStage
            };
        }

        resize_fit_subgraphs();
        resize_fit_syncs();

        for (PipelineId const pipelineId : tasks.pipelineIds)
        {
            RoxPipeline   const &roxPl          = m_roxPipelineOf[pipelineId];
            Pipeline      const &pipeline       = tasks.pipelineInst[pipelineId];
            RoxPltype     const &roxPltype      = roxPltypeOf[pipeline.type];
            SubgraphType  const &sgtype         = m_graph.sgtypes[roxPltype.sgtype];
            bool          const hasDefaultSchedule = ! pipeline.scheduleCondition.has_value();
            RoxLoopblk          &rRoxLoopblk    = m_roxLoopblkOf[pipeline.block];

            rRoxLoopblk.pipelineChildren.push_back(pipelineId);

            Subgraph            &rSgMain         = m_graph.subgraphs[roxPl.main];
            Subgraph            &rSgScheduleStat = m_graph.subgraphs[roxPl.scheduleStatus];
            auto          const pointCount      = sgtype.points.size();
            auto          const stageCount      = pointCount-2u;
            LocalPointId  const start           = point(0);
            LocalPointId  const finish          = point(stageCount+1u);

            rSgMain        .debugName       = fmt::format("PL{} {}", pipelineId.value, pipeline.name);
            rSgScheduleStat.debugName       = fmt::format("for PL{}", pipelineId.value);
            rSgMain        .instanceOf      = roxPltype.sgtype;
            rSgScheduleStat.instanceOf      = sgtSingleStatId;
            rSgMain        .points.resize(pointCount);
            rSgScheduleStat.points.resize(2);

            // Connect pipeline main subgraph to its parent BlockCtrl's subgraph
            m_graph.connect({.sync = rRoxLoopblk.left,      .subgraphPoint = {roxPl.main, start}});
            m_graph.connect({.sync = rRoxLoopblk.right,     .subgraphPoint = {roxPl.main, finish}});
            //m_graph.connect({.sync = rRoxLoopblk.enterexit, .subgraphPoint = {roxPl.main, roxPltype.schedule}});
            m_graph.connect({.sync = rRoxLoopblk.checkstop, .subgraphPoint = {roxPl.scheduleStatus, point(1)}});

            m_wtxSubgraphOf[roxPl.main] = {
                .tag        = WtxSubgraph::ETag::Pipeline,
                .pipelineId = pipelineId.value
            };

            m_wtxSubgraphOf[roxPl.scheduleStatus] = {
                .tag        = WtxSubgraph::ETag::ScheduleStatus
            };

            if (hasDefaultSchedule)
            {
                m_roxSyncOf[roxPl.schedule] = {
                    .tag            = ESyncType::PlSchedule,
                    .taskId         = TaskId{}.value,
                    .pipelineId     = pipelineId.value,
                };
                m_wtxSyncOf[roxPl.schedule] = {
                    .inactiveBlocks = 0
                };

                m_graph.syncs[roxPl.schedule].debugName = fmt::format("PL{} DefaultSchedule", pipelineId.value);
            }
        }

        // # Add tasks


        for (TaskId const task : tasks.taskIds)
        {
            SynchronizerId const syncId = m_graph.syncIds.create();
            m_roxTaskOf[task].main = syncId;
        }

        resize_fit_syncs();

        for (TaskId const taskId : tasks.taskIds)
        {
            RoxTask &rRoxTask = m_roxTaskOf[taskId];
            Synchronizer &rTaskMain = m_graph.syncs[rRoxTask.main];
            rTaskMain.debugName = fmt::format("Task{} {}", taskId.value, rFW.m_taskImpl[taskId].debugName);

            m_roxSyncOf[rRoxTask.main] = {
                .tag                    = ESyncType::Task,
                .taskId                 = taskId.value,
            };

            m_wtxSyncOf[rRoxTask.main] = {
                .canceledByPipelines    = 0,
                .inactiveBlocks         = 0
            };
        }

        // Connect LoopBlock schedule tasks
        for (LoopBlockId const loopblkId : tasks.loopblkIds)
        {
            LoopBlock     const &loopBlk        = tasks.loopblkInst[loopblkId];
            bool          const hasScheduleTask = loopBlk.scheduleCondition.has_value();
            RoxLoopblk          &rRoxLoopblk    = m_roxLoopblkOf[loopblkId];
            Subgraph            &rSubgraph      = m_graph.subgraphs[rRoxLoopblk.subgraph];

            if (hasScheduleTask)
            {
                LGRN_ASSERT( ! rRoxLoopblk.schedule.has_value() );

                RoxTask         &rRoxTask           = m_roxTaskOf[loopBlk.scheduleCondition];
                RoxSync         &rTaskMainRoxSync   = m_roxSyncOf[rRoxTask.main];
                Synchronizer    &rTaskMainSync      = m_graph.syncs[rRoxTask.main];

                // Convert existing task to a Schedule
                rRoxLoopblk     .schedule   = rRoxTask.main;
                rTaskMainRoxSync.tag        = ESyncType::BlkSchedule;
                rTaskMainRoxSync.loopBlk    = loopblkId.value;
                rTaskMainSync   .debugName  = fmt::format("BC{} Schedule{}", loopblkId.value, rTaskMainSync.debugName);
            }
            else
            {
                LGRN_ASSERT(rRoxLoopblk.schedule.has_value());
            }

            // { sync: schedule,    point: BlockCtrl.Schedule }
            m_graph.connect({.sync = rRoxLoopblk.schedule, .subgraphPoint = {rRoxLoopblk.subgraph, blkctrlSchedule}});
        }

        // Connect pipeline schedule tasks
        for (PipelineId const pipelineId : tasks.pipelineIds)
        {
            Pipeline      const &pipeline       = tasks.pipelineInst[pipelineId];
            RoxPltype     const &roxPltype      = roxPltypeOf[pipeline.type];
            RoxPipeline         &rRoxPipeline   = m_roxPipelineOf[pipelineId];
            RoxLoopblk          &rRoxLoopblk    = m_roxLoopblkOf[pipeline.block];
            bool          const hasScheduleTask = pipeline.scheduleCondition.has_value();

            // Assign rRoxPipeline.schedule
            // this can either be an existing Schedule task, or a dummy sync

            if (hasScheduleTask)
            {
                LGRN_ASSERT( ! rRoxPipeline.schedule.has_value() );

                RoxTask         &rRoxTask           = m_roxTaskOf[pipeline.scheduleCondition];
                RoxSync         &rTaskMainRoxSync   = m_roxSyncOf[rRoxTask.main];
                Synchronizer    &rTaskMainSync      = m_graph.syncs[rRoxTask.main];

                // Convert existing task to a Schedule
                rRoxPipeline    .schedule   = rRoxTask.main;
                rTaskMainRoxSync.tag        = ESyncType::PlSchedule;
                rTaskMainRoxSync.pipelineId = pipelineId.value;
                rTaskMainSync   .debugName  = fmt::format("PL{} Schedule{}", pipelineId.value, rTaskMainSync.debugName);
            }
            else
            {
                LGRN_ASSERT(rRoxPipeline.schedule.has_value() );
            }

            Subgraph            &rSgMain        = m_graph.subgraphs[rRoxPipeline.main];
            Synchronizer        &rScheduleSync  = m_graph.syncs[rRoxPipeline.schedule];

            m_graph.connect({.sync = rRoxPipeline.schedule, .subgraphPoint = {rRoxPipeline.main, roxPltype.schedulePoint}});
            m_graph.connect({.sync = rRoxPipeline.schedule, .subgraphPoint = {rRoxPipeline.scheduleStatus, point(0)}});

            ++m_wtxSyncOf[rRoxPipeline.schedule].inactiveBlocks;
            rRoxLoopblk.associatedTasks.push_back(rRoxPipeline.schedule);
        }

        auto const& pltypeinfoReg = PipelineTypeIdReg::instance();

        for (TaskSyncToPipeline const& taskSync : tasks.syncs)
        {
            Pipeline          const &pipeline       = tasks.pipelineInst[taskSync.pipeline];
            PipelineTypeInfo  const &pltypeinfo     = pltypeinfoReg.get(pipeline.type);
            auto              const &stageInfo      = pltypeinfo.stages[taskSync.stage];

            RoxPipeline             &rRoxPipeline   = m_roxPipelineOf[taskSync.pipeline];
            RoxTask                 &rRoxTask       = m_roxTaskOf[taskSync.task];
            RoxLoopblk              &rRoxLoopblk    = m_roxLoopblkOf[pipeline.block];
            Subgraph                &rSgMain        = m_graph.subgraphs[rRoxPipeline.main];
            Synchronizer            &rTaskMainSync  = m_graph.syncs[rRoxTask.main];
            RoxSync                 &rTaskRoxSync   = m_roxSyncOf[rRoxTask.main];
            WtxSync                 &rTaskWtxSync   = m_wtxSyncOf[rRoxTask.main];

            LocalPointId const stagePoint = point(1u + taskSync.stage.value);

            m_graph.connect({.sync = rRoxTask.main, .subgraphPoint = {rRoxPipeline.main, stagePoint}});

            if ( ! contains(rRoxLoopblk.associatedTasks, rRoxTask.main) )
            {
                rRoxLoopblk.associatedTasks.push_back(rRoxTask.main);
                ++rTaskWtxSync.inactiveBlocks;
            }


            bool isScheduleTask = m_roxSyncOf[rRoxTask.main].tag == ESyncType::PlSchedule;

            if (!isScheduleTask && stageInfo.useCancel && ! contains(rRoxPipeline.cancelsTasks, taskSync.task))
            {
                rRoxPipeline.cancelsTasks.push_back(taskSync.task);
                ++rTaskWtxSync.canceledByPipelines;
            }
        }

        // cancel-sync between task and pipeline
        // cancel-sync syncs-with all of the same tasks as the original, except modify

        // we find a task synced to a pipeline with modify
        // copy all the other connections

        lgrn::IdSetStl<LoopBlockId> maybeCancelAlreadyAdded;
        maybeCancelAlreadyAdded.resize(tasks.loopblkIds.capacity());

        for (TaskSyncToPipeline const& taskSync : tasks.syncs)
        {
            Pipeline          const &pipeline       = tasks.pipelineInst[taskSync.pipeline];
            RoxPipeline       const &roxPipeline    = m_roxPipelineOf[taskSync.pipeline];
            RoxPltype         const &roxPltype      = roxPltypeOf[pipeline.type];
            PipelineTypeInfo  const &pltypeinfo     = pltypeinfoReg.get(pipeline.type);
            auto              const &stageInfo      = pltypeinfo.stages[taskSync.stage];

            RoxTask                 &rRoxTask       = m_roxTaskOf[taskSync.task];
            RoxLoopblk              &rRoxLoopblk    = m_roxLoopblkOf[pipeline.block];

            LocalPointId const plpoint = point(1u + taskSync.stage.value);

            // Add MaybeCancel sync for each task that might be canceled
            if (stageInfo.useCancel)
            {
                SynchronizerId const maybeCancelId = m_graph.syncIds.create();
                resize_fit_syncs();

                WtxSync   const &rTaskWtxSync       = m_wtxSyncOf[rRoxTask.main];
                Synchronizer    &rMaybeCancel       = m_graph.syncs[maybeCancelId];
                RoxSync         &rRoxSync           = m_roxSyncOf[maybeCancelId];
                WtxSync         &rWtxSync           = m_wtxSyncOf[maybeCancelId];

                rMaybeCancel.debugName = fmt::format("MaybeCancel: {}", rFW.m_taskImpl[taskSync.task].debugName);
                //rMaybeCancel.debugGraphLongAndUgly = true;

                rRoxSync = {
                    .tag = ESyncType::MaybeCancel
                };
                rWtxSync = {
                    .inactiveBlocks = rTaskWtxSync.inactiveBlocks
                };

                maybeCancelAlreadyAdded.clear();
                rRoxLoopblk.maybeCancels.push_back(maybeCancelId);
                maybeCancelAlreadyAdded.emplace(pipeline.block);

                // copy connections from target task main sync to maybeCancel
                Synchronizer const& mainCopySrc = m_graph.syncs[rRoxTask.main];
                rMaybeCancel.connectedPoints.reserve(mainCopySrc.connectedPoints.size());
                for (SubgraphPointAddr const& addr : mainCopySrc.connectedPoints)
                {
                    if (addr.subgraph == roxPipeline.main && addr.point == plpoint) { continue; }

                    WtxSubgraph const& connectedWtxSubgraph = m_wtxSubgraphOf[addr.subgraph];

                    if (connectedWtxSubgraph.tag != WtxSubgraph::ETag::Pipeline) { continue; }

                    m_graph.connect({.sync = maybeCancelId, .subgraphPoint = {addr.subgraph, addr.point}});

                    LoopBlockId const connectedBlkId = tasks.pipelineInst[PipelineId{connectedWtxSubgraph.pipelineId}].block;
                    if ( ! maybeCancelAlreadyAdded.contains(connectedBlkId) )
                    {
                        m_roxLoopblkOf[connectedBlkId].maybeCancels.push_back(maybeCancelId);
                        maybeCancelAlreadyAdded.emplace(connectedBlkId);
                    }
                }

                m_graph.connect({.sync = maybeCancelId, .subgraphPoint = {roxPipeline.scheduleStatus, point(1)}});
            }
        }

        // keep connectedSyncs and connectedPoints sorted
        for (SynchronizerId const syncId : m_graph.syncIds)
        {
            std::sort(m_graph.syncs[syncId].connectedPoints.begin(),
                      m_graph.syncs[syncId].connectedPoints.end());
        }
        for (SubgraphId const subgraphId : m_graph.subgraphIds)
        {
            Subgraph &rSubgraph = m_graph.subgraphs[subgraphId];
            for (Subgraph::Point &rPoint : rSubgraph.points)
            {
                std::sort(rPoint.connectedSyncs.begin(), rPoint.connectedSyncs.end());
            }
        }

        // verify there isn't any schedule tasks being cancelled
        for (PipelineId const pipelineId : tasks.pipelineIds)
        {
            for (TaskId const taskId : m_roxPipelineOf[pipelineId].cancelsTasks)
            {
                LGRN_ASSERTMV(m_roxSyncOf[m_roxTaskOf[taskId].main].tag != ESyncType::PlSchedule,
                              "schedule tasks can't be cancellable",
                              rFW.m_taskImpl[taskId].debugName);
            }

        }

        // Done making the sync graph. If we have a way to visualize it, put it here.

        m_graph.debug_verify();



        //std::cout << "\n\n" << SyncGraphDOTVisualizer{m_graph} << "\n\n";

        m_exec.load(m_graph);

        using ESyncAction = SyncGraphExecutor::ESyncAction;

        // enable top-level loop blocks
        for (LoopBlockId const loopblkId : tasks.loopblkIds)
        {
            if ( ! tasks.loopblkInst[loopblkId].parent.has_value() )
            {
                RoxLoopblk const& rRoxLoopblk = m_roxLoopblkOf[loopblkId];
                m_exec.batch(ESyncAction::SetEnable, {rRoxLoopblk.schedule, rRoxLoopblk.finish}, m_graph);
            }
        }

        wait(rFW);
    }

    void on_sync_locked(SynchronizerId const syncId) {};

    void task_finish(osp::fw::Framework &rFW, osp::TaskId taskId, bool overrideStatus = false, TaskActions status = {}) override
    {
        auto const &first = m_tasksWaiting.begin();
        auto const &last  = m_tasksWaiting.end();
        auto const found  = std::find_if(first, last, [taskId] (TaskWaitingToFinish const& check)
        {
            return check.taskId == taskId;
        });
        LGRN_ASSERT(found != last);

        TaskWaitingToFinish const wait = *found;

        m_tasksWaiting.erase(found);

        if (wait.loopblk.has_value())
        {
            finish_schedule_block(wait.loopblk, overrideStatus ? status : wait.status);
        }
        else if (wait.pipeline.has_value())
        {
            finish_schedule_pipeline(wait.pipeline, overrideStatus ? status : wait.status, wait.neverCancel, rFW);
        }

        m_exec.batch(Unlock, {wait.syncId}, m_graph);
    }


    void wait(osp::fw::Framework& rFW) override
    {
        while (true)
        {
            for (int i = 0; i < 42; ++i)
            {
                LGRN_ASSERTM(i != 41, "Task graph updates not stopping; likely a pipeline is infinite looping.");

                bool const somethingChanged = m_exec.update(m_justAligned, m_graph);
                if ( somethingChanged )
                {
                    (void)0;
                }
                else
                {
                    break;
                }
            }

            if (m_justAligned.empty())
            {
                break;
            }

            for (SynchronizerId const syncAlignedId : m_justAligned)
            {
                process_aligned_sync(syncAlignedId, rFW);
            }

            m_exec.batch(SetDisable, m_disableSyncs, m_graph);
            m_disableSyncs.clear();

            m_exec.batch(Reset, m_reset, m_graph);
            m_reset.clear();

            for (Jump const& jump : m_jump)
            {
                m_exec.jump(jump.subgraphId, jump.cycle, jump.position, m_graph);
            }
            m_jump.clear();

            m_justAligned.clear();
        }
    }

    bool is_running(osp::fw::Framework const& rFW, LoopBlockId loopblkId) override
    {
        for (LoopBlockId const loopblkId : rFW.m_tasks.loopblkIds)
        {
            if (m_wtxLoopblkOf[loopblkId].state != WtxLoopblk::EState::NotRunning)
            {
                return true;
            }
        }
        return false;
    }

    std::shared_ptr<spdlog::logger> m_log;

private:

    void resize_fit_syncs()
    {
        auto const capacity = m_graph.syncIds.capacity();
        m_graph.syncs       .resize(capacity);
        m_roxSyncOf         .resize(capacity);
        m_wtxSyncOf         .resize(capacity);
    }

    void resize_fit_subgraphs()
    {
        auto const capacity = m_graph.subgraphIds.capacity();
        m_graph.subgraphs   .resize(capacity);
        m_wtxSubgraphOf     .resize(capacity);
    }

    struct TaskStuff
    {
        TaskActions status;
        bool externalFinish{false};
        bool nullTask{false};
    };

    TaskStuff run_task(TaskId const taskId, osp::fw::Framework& rFW)
    {
        if( ! taskId.has_value() ) { return { .nullTask = true }; }

        fw::TaskImpl const &taskImpl = rFW.m_taskImpl[taskId];

        if (taskImpl.func == nullptr) { return {.externalFinish = taskImpl.externalFinish, .nullTask = true }; }

        m_argumentRefs.clear();
        m_argumentRefs.reserve(taskImpl.args.size());
        for (fw::DataId const dataId : taskImpl.args)
        {
            entt::any data = dataId.has_value() ? rFW.m_data[dataId].as_ref() : entt::any{};
            LGRN_ASSERTV(data.data() != nullptr, taskImpl.debugName, taskId.value, dataId.value);
            m_argumentRefs.push_back(std::move(data));
        }

        return {
            .status         = taskImpl.func(fw::WorkerContext{}, m_argumentRefs),
            .externalFinish = taskImpl.externalFinish,
            .nullTask       = false
        };
    }

    void finish_schedule_block(LoopBlockId const loopblkId, TaskActions const status)
    {
        WtxLoopblk &rWtxLoopblk = m_wtxLoopblkOf[loopblkId];
        if (status.cancel)
        {
            rWtxLoopblk.state = WtxLoopblk::EState::NotRunning;
        }
        else
        {
            RoxLoopblk const& roxLoopblk = m_roxLoopblkOf[loopblkId];
            rWtxLoopblk.state = WtxLoopblk::EState::ScheduledToRun;
            m_exec.batch(SetEnable, {/*roxLoopblk.enterexit,*/ roxLoopblk.left, roxLoopblk.right}, m_graph);
        }
    }

    void finish_schedule_pipeline(PipelineId const pipelineId, TaskActions const status, bool neverCancels, osp::fw::Framework const& rFW)
    {
        WtxPipeline &rWtxPipeline = m_wtxPipelineOf[pipelineId];
        if (rWtxPipeline.isCanceled && ! status.cancel)
        {
            if ( ! neverCancels )
            {
                WtxLoopblk &rWtxLoopblk = m_wtxLoopblkOf[rFW.m_tasks.pipelineInst[pipelineId].block];
                ++rWtxLoopblk.pipelinesRunning;
            }

            RoxPipeline &rRoxPipeline = m_roxPipelineOf[pipelineId];
            rWtxPipeline.isCanceled = false;
            for (TaskId const cancelTaskId : rRoxPipeline.cancelsTasks)
            {
                SynchronizerId const cancelSyncId = m_roxTaskOf[cancelTaskId].main;
                WtxSync &rCancelWtxSync = m_wtxSyncOf[cancelSyncId];
                --rCancelWtxSync.canceledByPipelines;
                //std::cout << "decrement " << pipelineId.value << " canceledByPipelines=" << rCancelWtxSync.canceledByPipelines << " inactiveBlocks=" << rCancelWtxSync.inactiveBlocks << "\n";
                if (rCancelWtxSync.canceledByPipelines == 0 && rCancelWtxSync.inactiveBlocks == 0)
                {
                    std::cout << "enable task " << cancelTaskId.value << " " << rFW.m_taskImpl[cancelTaskId].debugName << "\n";
                    m_exec.batch(SetEnable, {cancelSyncId}, m_graph);
                }
            }
        }
        else if ( ! rWtxPipeline.isCanceled && status.cancel )
        {
            RoxPipeline &rRoxPipeline = m_roxPipelineOf[pipelineId];
            rWtxPipeline.isCanceled = true;

            WtxLoopblk &rWtxLoopblk = m_wtxLoopblkOf[rFW.m_tasks.pipelineInst[pipelineId].block];
            --rWtxLoopblk.pipelinesRunning;

            for (TaskId const cancelTaskId : rRoxPipeline.cancelsTasks)
            {
                SynchronizerId const cancelSyncId = m_roxTaskOf[cancelTaskId].main;
                WtxSync &rCancelWtxSync = m_wtxSyncOf[cancelSyncId];
                if (rCancelWtxSync.canceledByPipelines == 0 && rCancelWtxSync.inactiveBlocks == 0)
                {
                    std::cout << "disable task " << cancelTaskId.value << " " << rFW.m_taskImpl[cancelTaskId].debugName << "\n";
                    m_exec.batch(SetDisable, {cancelSyncId}, m_graph);
                }
                ++rCancelWtxSync.canceledByPipelines;
            }
        }
    }




    void process_aligned_sync(SynchronizerId const syncAlignedId, osp::fw::Framework& rFW)
    {
        //std::cout << "locked SY" << syncAlignedId.value << " -- ";
        RoxSync const &rRoxSync = m_roxSyncOf[syncAlignedId];
        WtxSync       &rWtxSync = m_wtxSyncOf[syncAlignedId];

        switch(rRoxSync.tag)
        {
        case ESyncType::BlkSchedule:
        {
            auto const taskId = TaskId{rRoxSync.taskId};
            //std::cout << "schedule task! " << taskId.value << " " << taskId.has_value() << "\n";

            auto const [status, externalFinish, nullTask] = run_task(taskId, rFW);

            if (externalFinish)
            {
                m_tasksWaiting.push_back({
                    .taskId         = taskId,
                    .status         = status,
                    .syncId         = syncAlignedId,
                    .pipeline       = {},
                    .loopblk        = LoopBlockId{rRoxSync.loopBlk},
                    .neverCancel    = false
                });
            }
            else
            {
                finish_schedule_block(LoopBlockId{rRoxSync.loopBlk}, status);
                m_exec.batch(Unlock, {syncAlignedId}, m_graph);
            }

            break;
        }
        case ESyncType::BlkLeft:
        {
            //std::cout << "left!\n";

            RoxLoopblk const &roxLoopblk  = m_roxLoopblkOf[LoopBlockId{rRoxSync.loopBlk}];

            m_disableSyncs.push_back(syncAlignedId);
            for (PipelineId const pipeline : roxLoopblk.pipelineChildren)
            {
                RoxPipeline const &roxPipeline = m_roxPipelineOf[pipeline];
                m_jump.push_back(Jump{.subgraphId = roxPipeline.main, .cycle = cycle(1), .position = std::uint8_t(roxPipeline.initialStage)});
            }

            WtxLoopblk       &rWtxLoopblk = m_wtxLoopblkOf[LoopBlockId{rRoxSync.loopBlk}];

            rWtxLoopblk.state = WtxLoopblk::EState::Running;

            for (SynchronizerId const syncId : roxLoopblk.associatedTasks)
            {
                WtxSync &rTaskWtxSync = m_wtxSyncOf[syncId];
                --rTaskWtxSync.inactiveBlocks;

                if (rTaskWtxSync.inactiveBlocks == 0 && rTaskWtxSync.canceledByPipelines == 0)
                {
                    m_exec.batch(SetEnable, {syncId}, m_graph);
                }
            }

            for (SynchronizerId const maybeCancelId : roxLoopblk.maybeCancels)
            {
                WtxSync &rMaybeCancelRole = m_wtxSyncOf[maybeCancelId];
                --rMaybeCancelRole.inactiveBlocks;
                if (rMaybeCancelRole.inactiveBlocks == 0)
                {
                    m_exec.batch(SetEnable, {maybeCancelId}, m_graph);
                }
            }

//            for (PipelineId const pipeline : roxLoopblk.pipelineChildren)
//            {
//                RoxPipeline const &roxPipeline = m_roxPipelineOf[pipeline];
//                m_exec.select_cycle(roxPipeline.main, cycle(1) /*Running*/, m_graph);
//            }
            m_exec.batch(SetEnable, {roxLoopblk.checkstop}, m_graph);


            break;
        }
         #if 0
        case ESyncType::BlkEnterExit:
        {
            //std::cout << "enter/exit\n";
            WtxLoopblk &rWtxLoopblk = m_wtxLoopblkOf[LoopBlockId{rRoxSync.loopBlk}];
            if (rWtxLoopblk.state == WtxLoopblk::EState::ScheduledToRun)
            {
                // Enter block
                rWtxLoopblk.state = WtxLoopblk::EState::Running;

                RoxLoopblk const &roxLoopblk  = m_roxLoopblkOf[LoopBlockId{rRoxSync.loopBlk}];
                WtxLoopblk       &rWtxLoopblk = m_wtxLoopblkOf[LoopBlockId{rRoxSync.loopBlk}];
                for (SynchronizerId const syncId : roxLoopblk.associatedTasks)
                {
                    WtxSync &rTaskWtxSync = m_wtxSyncOf[syncId];
                    --rTaskWtxSync.inactiveBlocks;

                    if (rTaskWtxSync.inactiveBlocks == 0 && rTaskWtxSync.canceledByPipelines == 0)
                    {
                        m_exec.batch(SetEnable, {syncId}, m_graph);
                    }
                }

                for (SynchronizerId const maybeCancelId : roxLoopblk.maybeCancels)
                {
                    WtxSync &rMaybeCancelRole = m_wtxSyncOf[maybeCancelId];
                    --rMaybeCancelRole.inactiveBlocks;
                    if (rMaybeCancelRole.inactiveBlocks == 0)
                    {
                        m_exec.batch(SetEnable, {maybeCancelId}, m_graph);
                    }
                }

                for (PipelineId const pipeline : roxLoopblk.pipelineChildren)
                {
                    RoxPipeline const &roxPipeline = m_roxPipelineOf[pipeline];
                    m_exec.select_cycle(roxPipeline.main, cycle(1) /*Running*/, m_graph);
                }
                m_exec.batch(SetEnable, {roxLoopblk.checkstop}, m_graph);
            }
            else if (rWtxLoopblk.state == WtxLoopblk::EState::Exiting)
            {
                rWtxLoopblk.state = WtxLoopblk::EState::NotRunning;
                RoxLoopblk const& roxLoopblk = m_roxLoopblkOf[LoopBlockId{rRoxSync.loopBlk}];
                for (PipelineId const pipeline : roxLoopblk.pipelineChildren)
                {
                    RoxPipeline const &roxPipeline = m_roxPipelineOf[pipeline];
                    m_exec.select_cycle(roxPipeline.main, cycle(0) /*Control*/, m_graph);
                }
            }
            else
            {
                LGRN_ASSERT(false);
            }

            m_disableSyncs.push_back(syncAlignedId);
            break;
        }
        #endif
        case ESyncType::BlkCheckStop:
        {
            //std::cout << "checkstop!\n";
            RoxLoopblk const &roxLoopblk  = m_roxLoopblkOf[LoopBlockId{rRoxSync.loopBlk}];
            WtxLoopblk       &rWtxLoopblk = m_wtxLoopblkOf[LoopBlockId{rRoxSync.loopBlk}];
            if (rWtxLoopblk.pipelinesRunning == 0)
            {
                // it's time to stop. it's time to stop, okay..
                rWtxLoopblk.state = WtxLoopblk::EState::Exiting;

                //m_exec.batch(SetEnable, {roxLoopblk.enterexit}, m_graph);

                // disable every sync
                for (SynchronizerId const syncId : roxLoopblk.associatedTasks)
                {
                    WtxSync &rTaskWtxSync = m_wtxSyncOf[syncId];

                    if (rTaskWtxSync.inactiveBlocks == 0 && rTaskWtxSync.canceledByPipelines == 0)
                    {
                        m_disableSyncs.push_back(syncId);
                    }

                    ++rTaskWtxSync.inactiveBlocks;
                }

                for (SynchronizerId const maybeCancelId : roxLoopblk.maybeCancels)
                {
                    WtxSync &rMaybeCancelRole = m_wtxSyncOf[maybeCancelId];

                    if (rMaybeCancelRole.inactiveBlocks == 0)
                    {
                        m_disableSyncs.push_back(maybeCancelId);
                    }
                    ++rMaybeCancelRole.inactiveBlocks;
                }

                for (PipelineId const pipelineId : roxLoopblk.pipelineChildren)
                {
                    RoxPipeline const &roxPipeline  = m_roxPipelineOf[pipelineId];
                    WtxPipeline       &rWtxPipeline = m_wtxPipelineOf[pipelineId];

                    if ( ! rWtxPipeline.isCanceled )
                    {
                        rWtxPipeline.isCanceled = true;

                        for (TaskId const cancelTaskId : roxPipeline.cancelsTasks)
                        {
                            SynchronizerId const cancelSyncId = m_roxTaskOf[cancelTaskId].main;
                            WtxSync &rCancelWtxSync = m_wtxSyncOf[cancelSyncId];
                            if (rCancelWtxSync.canceledByPipelines == 0 && rCancelWtxSync.inactiveBlocks == 0)
                            {
                                std::cout << "disable task " << cancelTaskId.value << "\n";
                                m_disableSyncs.push_back(cancelSyncId);
                            }
                            ++rCancelWtxSync.canceledByPipelines;
                        }
                    }
                }
                m_disableSyncs.push_back(syncAlignedId);

                rWtxLoopblk.state = WtxLoopblk::EState::NotRunning;
                RoxLoopblk const& roxLoopblk = m_roxLoopblkOf[LoopBlockId{rRoxSync.loopBlk}];
                for (PipelineId const pipeline : roxLoopblk.pipelineChildren)
                {
                    RoxPipeline const &roxPipeline = m_roxPipelineOf[pipeline];
                    m_jump.push_back(Jump{.subgraphId = roxPipeline.main, .cycle = cycle(0), .position = 1});
                }
            }
            else
            {
                m_exec.batch(Unlock, {syncAlignedId}, m_graph);
            }
            break;
        }
        case ESyncType::Task:
        {
            //std::cout << "task!\n";
            auto const taskId = TaskId{rRoxSync.taskId};
            auto const [status, externalFinish, nullTask] = run_task(taskId, rFW);

            if (externalFinish)
            {
                m_tasksWaiting.push_back({
                    .taskId         = taskId,
                    .status         = status,
                    .syncId         = syncAlignedId,
                    .pipeline       = {},
                    .loopblk        = {},
                    .neverCancel    = false
                });
            }
            else
            {
                m_exec.batch(Unlock, {syncAlignedId}, m_graph);
            }
            break;
        }
        case ESyncType::PlSchedule:
        {
            auto const taskId = TaskId{rRoxSync.taskId};
            //std::cout << "schedule task! " << taskId.value << " " << taskId.has_value() << "\n";

            auto const [status, externalFinish, nullTask] = run_task(taskId, rFW);

            if (externalFinish)
            {
                m_tasksWaiting.push_back({
                    .taskId         = taskId,
                    .status         = status,
                    .syncId         = syncAlignedId,
                    .pipeline       = PipelineId{rRoxSync.pipelineId},
                    .loopblk        = {},
                    .neverCancel    = false
                });
            }
            else
            {
                finish_schedule_pipeline(PipelineId{rRoxSync.pipelineId}, status, nullTask, rFW);
                m_exec.batch(Unlock, {syncAlignedId}, m_graph);
            }
            break;
        }

        case ESyncType::BlkRight:
        {
            //std::cout << "right!\n";
            m_disableSyncs.push_back(syncAlignedId);
            break;
        }
        case ESyncType::BlkFinish:
        {
            RoxLoopblk const& roxLoopblk = m_roxLoopblkOf[LoopBlockId{rRoxSync.loopBlk}];
            for (PipelineId const pipeline : roxLoopblk.pipelineChildren)
            {
                RoxPipeline const &roxPipeline = m_roxPipelineOf[pipeline];
                // set subgraphs back to their initial point for next iteration
                m_reset.push_back(roxPipeline.main);
                m_reset.push_back(roxPipeline.scheduleStatus);
            }
        }
        case ESyncType::MaybeCancel:
            //std::cout << "maybecan!\n";
            m_exec.batch(Unlock, {syncAlignedId}, m_graph);
            break;
        case ESyncType::Invalid:
            LGRN_ASSERTV(false, syncAlignedId.value, m_graph.syncs[syncAlignedId].debugName);
            break;
        }
    }

    SyncGraph                               m_graph;
    KeyedVec<LoopBlockId, RoxLoopblk>       m_roxLoopblkOf;
    KeyedVec<LoopBlockId, WtxLoopblk>       m_wtxLoopblkOf;
    KeyedVec<PipelineId, RoxPipeline>       m_roxPipelineOf;
    KeyedVec<TaskId, RoxTask>               m_roxTaskOf;
    KeyedVec<SynchronizerId, RoxSync>       m_roxSyncOf;

    SyncGraphExecutor                       m_exec;
    std::vector<SynchronizerId>             m_justAligned;
    std::vector<SynchronizerId>             m_disableSyncs;
    std::vector<SubgraphId>                 m_reset;
    std::vector<Jump>                       m_jump;
    std::vector<TaskWaitingToFinish>        m_tasksWaiting;

    std::vector<entt::any>                  m_argumentRefs;
    KeyedVec<SynchronizerId, WtxSync>       m_wtxSyncOf;
    KeyedVec<SubgraphId, WtxSubgraph>       m_wtxSubgraphOf;
    KeyedVec<PipelineId, WtxPipeline>       m_wtxPipelineOf;

};

}
