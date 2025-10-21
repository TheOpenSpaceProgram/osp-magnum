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
#include "singlethread_framework.h"

#include <spdlog/spdlog.h>

namespace osp::exec
{

template<typename VEC_T, typename VAL_T>
static constexpr bool vec_contains(VEC_T const& vec, VAL_T const& searchfor)
{
    // minor optimization: using reverse as it's more likely to search for recently added elements.
    auto const &first = vec.rbegin();
    auto const &last  = vec.rend();
    return std::find(first, last, searchfor) != last;
}

static constexpr LocalPointId point(std::size_t index) { return LocalPointId::from_index(index); }
static constexpr LocalCycleId cycle(std::size_t index) { return LocalCycleId::from_index(index); }

void SinglethreadFWExecutor::load(osp::fw::Framework& rFW)
{
    Tasks const &tasks      = rFW.m_tasks;
    auto  const &pltypereg  = PipelineTypeIdReg::instance();

    for (LoopBlockId const loopblkId : tasks.loopblkIds)
    {
        if (tasks.loopblkInst[loopblkId].parent.has_value())
        {
            continue;
        }

        lgrn::IdSetStl<LoopBlockId> loopblkFamily;
        loopblkFamily.resize(tasks.loopblkIds.capacity());
        loopblkFamily.insert(loopblkId);

        for (LoopBlockId const mightBeChild : tasks.loopblkIds)
        {
            if (tasks.loopblkInst[mightBeChild].parent == loopblkId)
            {
                loopblkFamily.insert(mightBeChild);
            }
        }

        TaskOrderReport report;
        check_task_order(tasks, report, std::move(loopblkFamily));

        if (m_log)
        {
            SPDLOG_LOGGER_INFO(m_log, "LoopBlock{} task order report\n{}\n", loopblkId.value, visualize_task_order(report, tasks));
        }

        LGRN_ASSERTM(report.failedNotAdded.empty() && report.failedLocked.empty(), "deadlock");
    }

    // IdRegistryStl doesn't have a clear() yet. just leak implmentation details and set all bits to 1.

    m_graph.sgtypeIds   .bitview().set();
    m_graph.sgtypes     .clear();
    m_graph.subgraphIds .bitview().set();
    m_graph.subgraphs   .clear();
    m_graph.syncIds     .bitview().set();
    m_graph.syncs       .clear();
    m_exec.perSubgraph  .clear();
    m_exec.perSync      .clear();
    m_exec.subgraphsMoving.clear();
    m_exec.justMoved    .clear();
    m_roxLoopblkOf      .clear();
    m_roxPipelineOf     .clear();
    m_roxSyncOf         .clear();
    m_wtxSubgraphOf     .clear();
    m_wtxSyncOf         .clear();
    m_roxTaskOf         .clear();
    m_tasksWaiting      .clear();

    m_roxTaskOf         .resize(tasks.taskIds.capacity());
    m_wtxTaskOf         .resize(tasks.taskIds.capacity());
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
    LocalPointId const blkctrlBlkRun    = point(2);
    LocalPointId const blkctrlBlkExit   = point(3);
    LocalPointId const blkctrlFinish    = point(4);

    {
        SubgraphType &rSgtype = m_graph.sgtypes[sgtBlkCtrlId];
        rSgtype.debugName = "BlockController";
        rSgtype.points = {{
            {.debugName = "Start"},     // point(0)
            {.debugName = "Schedule"},  // point(1)
            {.debugName = "BlockRun"},  // point(2)
            {.debugName = "BlockExit"}, // point(3)
            {.debugName = "Finish"}     // point(4)
        }};
        rSgtype.cycles = {{
            {.debugName = "Control",    .path = {blkctrlStart,    blkctrlFinish}},
            {.debugName = "Run",        .path = {blkctrlSchedule, blkctrlBlkRun, blkctrlBlkExit}}
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
        //LGRN_ASSERTMV(schedulePoint.has_value(), "Missing Schedule stage", rPltypeInfo.debugName);
        rSgtype.points[finish].debugName = "Finish";

        rSgtype.cycles = {{
            {.debugName = "Control",  .path = {start, finish}},         // cycle(0)
            {.debugName = "Run",  .path = std::move(runningPath)},      // cycle(1)
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
            .scheduleStatus = m_graph.subgraphIds.create(),
            .schedule       = hasDefaultSchedule ? m_graph.syncIds.create() : SynchronizerId{},
            .checkstop      = m_graph.syncIds.create(),
            .left           = m_graph.syncIds.create(),
            .right          = m_graph.syncIds.create()
        };

    }

    resize_fit_syncs();
    resize_fit_subgraphs();

    for (LoopBlockId const loopblkId : tasks.loopblkIds)
    {
        LoopBlock     const &loopblk           = tasks.loopblkInst[loopblkId];
        bool          const hasDefaultSchedule = ! loopblk.scheduleCondition.has_value();
        RoxLoopblk    const &roxLoopblk     = m_roxLoopblkOf[loopblkId];

        Subgraph &rSubgraph         = m_graph.subgraphs[roxLoopblk.subgraph];
        rSubgraph.instanceOf        = sgtBlkCtrlId;
        rSubgraph.debugName         = fmt::format("BC{}", loopblkId.value);
        rSubgraph.points.clear();
        rSubgraph.points.resize(m_graph.sgtypes[sgtBlkCtrlId].points.size());
        m_wtxSubgraphOf[roxLoopblk.subgraph] = {
            .tag        = WtxSubgraph::ETag::LoopBlock,
            .loopblkId  = loopblkId.value
        };

        Subgraph &rSgScheduleStat   = m_graph.subgraphs[roxLoopblk.scheduleStatus];
        rSgScheduleStat.debugName   = fmt::format("for BC{}", loopblkId.value);
        rSgScheduleStat.instanceOf  = sgtSingleStatId;
        rSgScheduleStat.points.resize(2);
        m_wtxSubgraphOf[roxLoopblk.scheduleStatus] = { .tag = WtxSubgraph::ETag::ScheduleStatus };

        if ( hasDefaultSchedule )
        {
            Synchronizer &rSchedule = m_graph.syncs[roxLoopblk.schedule];
            rSchedule    .debugName = fmt::format("BC{} DefaultSchedule", loopblkId.value);
            m_roxSyncOf[roxLoopblk.schedule] = {
                .tag = ESyncType::BlkSchedule,
                .taskId = TaskId{},
                .loopBlk = loopblkId
            };
            // connection happens later in "Connect LoopBlock schedule tasks" below
        }

        Synchronizer &rCheckstop    = m_graph.syncs[roxLoopblk.checkstop];
        Synchronizer &rLeft         = m_graph.syncs[roxLoopblk.left];
        Synchronizer &rRight        = m_graph.syncs[roxLoopblk.right];

        rCheckstop  .debugName = fmt::format("BC{} Check-Stop", loopblkId.value);
        rLeft       .debugName = fmt::format("BC{} Left",       loopblkId.value);
        rRight      .debugName = fmt::format("BC{} Right",      loopblkId.value);
        rLeft       .debugGraphStraight = true;
        rCheckstop  .debugGraphLoose = true;
        rRight      .debugGraphLoose = true;

        m_roxSyncOf[roxLoopblk.checkstop]   = {.tag = ESyncType::BlkCheckStop, .loopBlk = loopblkId};
        m_roxSyncOf[roxLoopblk.left]        = {.tag = ESyncType::BlkLeft,      .loopBlk = loopblkId};
        m_roxSyncOf[roxLoopblk.right]       = {.tag = ESyncType::BlkRight,     .loopBlk = loopblkId};

        m_graph.connect({.sync = roxLoopblk.left,   .subgraphPoint = {roxLoopblk.subgraph, blkctrlBlkRun}});
        m_graph.connect({.sync = roxLoopblk.right,  .subgraphPoint = {roxLoopblk.subgraph, blkctrlBlkRun}});
    }

    // Add loopblock parent - loopblock child connections
    for (LoopBlockId const loopblkId : tasks.loopblkIds)
    {
        LoopBlock const &loopblk  = tasks.loopblkInst[loopblkId];

        if (loopblk.parent.has_value())
        {
            RoxLoopblk const &child  = m_roxLoopblkOf[loopblkId];
            RoxLoopblk const &parent = m_roxLoopblkOf[loopblk.parent];

            m_graph.connect({.sync = parent.left,      .subgraphPoint = {child.subgraph, blkctrlStart}});
            m_graph.connect({.sync = parent.right,     .subgraphPoint = {child.subgraph, blkctrlFinish}});
            m_graph.connect({.sync = parent.checkstop, .subgraphPoint = {child.subgraph, blkctrlBlkExit}});
            m_graph.connect({.sync = parent.checkstop, .subgraphPoint = {child.scheduleStatus, point(1)}});
        }
    }

    // # Add pipelines

    // Count number syncs to each pipeline
    for (TaskSyncToPipeline const& taskSync : tasks.syncs)
    {
        ++ m_roxPipelineOf[taskSync.pipeline].syncCount;
    }

    // reserve new pipeline subgraph IDs
    for (PipelineId const pipelineId : tasks.pipelineIds)
    {
        Pipeline  const &pipeline       = tasks.pipelineInst[pipelineId];
        RoxPltype const &roxPltype      = roxPltypeOf[pipeline.type];
        RoxPipeline     &rRoxPl         = m_roxPipelineOf[pipelineId];

        bool      const hasSchedulePoint   = roxPltype.schedulePoint.has_value();
        bool      const hasDefaultSchedule = hasSchedulePoint && ! pipeline.scheduleCondition.has_value();

        LGRN_ASSERTM(pipeline.initialStage.has_value(), "pipeline has no initial stage set");

        if (pipeline.scheduleCondition.has_value())
        {
            rRoxPl.syncCount += 1;
        }

        if (rRoxPl.syncCount != 0)
        {
            rRoxPl.main             = m_graph.subgraphIds.create();
            rRoxPl.scheduleStatus   = hasSchedulePoint ? m_graph.subgraphIds.create() : SubgraphId{};
            rRoxPl.schedule         = hasDefaultSchedule ? m_graph.syncIds.create() : SynchronizerId{};
            rRoxPl.initialStage     = pipeline.initialStage;
        }
        // else, this pipeline has no tasks. Don't create it as it will just infinite loop and hang
    }

    resize_fit_subgraphs();
    resize_fit_syncs();

    for (PipelineId const pipelineId : tasks.pipelineIds)
    {
        RoxPipeline   const &roxPl          = m_roxPipelineOf[pipelineId];

        if ( ! roxPl.main.has_value() ) { continue; }

        Pipeline      const &pipeline       = tasks.pipelineInst[pipelineId];
        RoxPltype     const &roxPltype      = roxPltypeOf[pipeline.type];
        SubgraphType  const &sgtype         = m_graph.sgtypes[roxPltype.sgtype];
        bool          const hasDefaultSchedule = roxPltype.schedulePoint.has_value() && ! pipeline.scheduleCondition.has_value();
        RoxLoopblk          &rRoxLoopblk    = m_roxLoopblkOf[pipeline.block];

        rRoxLoopblk.pipelineChildren.push_back(pipelineId);

        Subgraph            &rSgMain        = m_graph.subgraphs[roxPl.main];
        auto          const pointCount      = sgtype.points.size();
        auto          const stageCount      = pointCount-2u;
        LocalPointId  const start           = point(0);
        LocalPointId  const finish          = point(stageCount+1u);

        rSgMain        .debugName           = fmt::format("PL{} {}", pipelineId.value, pipeline.name);
        rSgMain        .instanceOf          = roxPltype.sgtype;
        rSgMain        .points.resize(pointCount);

        // Connect pipeline main subgraph to its parent BlockCtrl's subgraph
        m_graph.connect({.sync = rRoxLoopblk.left,      .subgraphPoint = {roxPl.main, start}});
        m_graph.connect({.sync = rRoxLoopblk.right,     .subgraphPoint = {roxPl.main, finish}});

        m_wtxSubgraphOf[roxPl.main] = {
            .tag        = WtxSubgraph::ETag::Pipeline,
            .pipelineId = pipelineId.value
        };

        if (roxPltype.schedulePoint.has_value())
        {
            Subgraph &rSgScheduleStat       = m_graph.subgraphs[roxPl.scheduleStatus];
            rSgScheduleStat.debugName       = fmt::format("for PL{}", pipelineId.value);
            rSgScheduleStat.instanceOf      = sgtSingleStatId;
            rSgScheduleStat.points.resize(2);

            m_graph.connect({.sync = rRoxLoopblk.checkstop, .subgraphPoint = {roxPl.scheduleStatus, point(1)}});

            m_wtxSubgraphOf[roxPl.scheduleStatus] = {
                .tag        = WtxSubgraph::ETag::ScheduleStatus
            };
        }
        // else, no schedule point. No scheduleStatus graph was created either.

        if (hasDefaultSchedule)
        {
            m_roxSyncOf[roxPl.schedule] = {
                .tag            = ESyncType::PlSchedule,
                .taskId         = TaskId{},
                .pipelineId     = pipelineId,
            };
            m_wtxSyncOf[roxPl.schedule] = {  };

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
        rTaskMain.debugName = fmt::format("Task{} {}", taskId.value, rFW.m_tasks.taskInst[taskId].debugName);

        m_roxSyncOf[rRoxTask.main] = {
            .tag                    = ESyncType::Task,
            .taskId                 = taskId,
        };

        m_wtxSyncOf[rRoxTask.main] = {
            .canceledByPipelines    = 0
        };
    }

    // Connect LoopBlock schedule tasks
    for (LoopBlockId const loopblkId : tasks.loopblkIds)
    {
        LoopBlock     const &loopBlk        = tasks.loopblkInst[loopblkId];
        bool          const hasCustomScheduleTask = loopBlk.scheduleCondition.has_value();
        RoxLoopblk          &rRoxLoopblk    = m_roxLoopblkOf[loopblkId];
        Subgraph            &rSubgraph      = m_graph.subgraphs[rRoxLoopblk.subgraph];

        if (hasCustomScheduleTask)
        {
            LGRN_ASSERT( ! rRoxLoopblk.schedule.has_value() );

            RoxTask         &rRoxTask           = m_roxTaskOf[loopBlk.scheduleCondition];
            RoxSync         &rTaskMainRoxSync   = m_roxSyncOf[rRoxTask.main];
            Synchronizer    &rTaskMainSync      = m_graph.syncs[rRoxTask.main];

            // Convert existing task to a Schedule
            rRoxTask        .parent     = loopblkId;
            rRoxLoopblk     .schedule   = rRoxTask.main;
            rTaskMainRoxSync.tag        = ESyncType::BlkSchedule;
            rTaskMainRoxSync.loopBlk    = loopblkId;
            rTaskMainSync   .debugName  = fmt::format("BC{} Schedule{}", loopblkId.value, rTaskMainSync.debugName);
        }
        else
        {
            LGRN_ASSERT(rRoxLoopblk.schedule.has_value());
        }

        m_graph.connect({.sync = rRoxLoopblk.schedule, .subgraphPoint = {rRoxLoopblk.subgraph, blkctrlSchedule}});
        m_graph.connect({.sync = rRoxLoopblk.schedule, .subgraphPoint = {rRoxLoopblk.scheduleStatus, point(0)}});

        if (loopBlk.parent.has_value())
        {
            RoxLoopblk &rParentRoxLoopblk = m_roxLoopblkOf[loopBlk.parent];

            rParentRoxLoopblk.loopblkChildren .push_back(loopblkId);
            rParentRoxLoopblk.associatedTasks .push_back(rRoxLoopblk.schedule);
            rParentRoxLoopblk.associatedOthers.push_back(rRoxLoopblk.left);
            rParentRoxLoopblk.associatedOthers.push_back(rRoxLoopblk.right);
        }
    }

    // Connect pipeline schedule tasks
    for (PipelineId const pipelineId : tasks.pipelineIds)
    {
        Pipeline      const &pipeline       = tasks.pipelineInst[pipelineId];
        RoxPltype     const &roxPltype      = roxPltypeOf[pipeline.type];
        RoxPipeline         &rRoxPipeline   = m_roxPipelineOf[pipelineId];

        if (rRoxPipeline.scheduleStatus.has_value())
        {
            RoxLoopblk          &rRoxLoopblk            = m_roxLoopblkOf[pipeline.block];
            bool          const hasCustomScheduleTask   = pipeline.scheduleCondition.has_value();

            if (hasCustomScheduleTask)
            {
                LGRN_ASSERT( ! rRoxPipeline.schedule.has_value() );

                RoxTask         &rRoxTask           = m_roxTaskOf[pipeline.scheduleCondition];
                RoxSync         &rTaskMainRoxSync   = m_roxSyncOf[rRoxTask.main];
                Synchronizer    &rTaskMainSync      = m_graph.syncs[rRoxTask.main];

                // Convert existing task to a Schedule
                rRoxTask        .parent     = pipeline.block;
                rRoxPipeline    .schedule   = rRoxTask.main;
                rTaskMainRoxSync.tag        = ESyncType::PlSchedule;
                rTaskMainRoxSync.pipelineId = pipelineId;
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

            rRoxLoopblk.associatedTasks.push_back(rRoxPipeline.schedule);
        }
    }

    // assign rRoxTask.parent loopblocks, and detect tasks that span across multiple loopblocks
    for (TaskSyncToPipeline const& taskSync : tasks.syncs)
    {
        RoxTask                 &rRoxTask       = m_roxTaskOf[taskSync.task];
        Pipeline          const &pipeline       = tasks.pipelineInst[taskSync.pipeline];
        RoxLoopblk              &rRoxLoopblk    = m_roxLoopblkOf[pipeline.block];
        WtxSync                 &rTaskWtxSync   = m_wtxSyncOf[rRoxTask.main];

        if ( ! vec_contains(rRoxLoopblk.associatedTasks, rRoxTask.main) )
        {
            rRoxLoopblk.associatedTasks.push_back(rRoxTask.main);
        }

        if (rRoxTask.parent.has_value())
        {
            if (   ! rRoxTask.sustainer.has_value()
                && rRoxTask.parent != pipeline.block)
            {
                // Detected that task spans across multiple loopblocks
                LoopBlockId inner;
                LoopBlockId outer;
                if (tasks.loopblkInst[rRoxTask.parent].parent == pipeline.block)
                {
                    inner = rRoxTask.parent;
                    outer = pipeline.block;
                }
                else if (tasks.loopblkInst[pipeline.block].parent == rRoxTask.parent)
                {
                    inner = pipeline.block;
                    outer = rRoxTask.parent;
                    rRoxTask.parent = pipeline.block;
                }
                else
                {
                    LGRN_ASSERTMV(false, "", taskSync.task.value);
                }

                rRoxTask.external  = m_graph.syncIds.create();
                rRoxTask.sustainer = m_graph.syncIds.create();

                m_roxLoopblkOf[outer].associatedOthers.push_back(rRoxTask.sustainer);
                m_roxLoopblkOf[inner].externals       .push_back(rRoxTask.external);
            }
        }
        else
        {
            rRoxTask.parent = pipeline.block;
        }
    }

    resize_fit_syncs();

    for (TaskId const taskId : tasks.taskIds)
    {
        RoxTask const &rRoxTask = m_roxTaskOf[taskId];

        if (rRoxTask.is_spanning_nested_loopblocks())
        {
            RoxLoopblk const &rRoxLoopblk = m_roxLoopblkOf[rRoxTask.parent];
            bool const isSchedule = m_roxSyncOf[rRoxTask.main].tag == ESyncType::PlSchedule;

            Synchronizer    &rSustainer       = m_graph.syncs[rRoxTask.sustainer];
            rSustainer.debugName = fmt::format("Task{} Sustainer", taskId.value);
            rSustainer.debugGraphLongAndUgly = true;
            m_roxSyncOf[rRoxTask.sustainer] = {
                .tag        = ESyncType::TaskSus,
                .taskId     = taskId,
            };

            Synchronizer    &rExternal       = m_graph.syncs[rRoxTask.external];
            rExternal.debugName = fmt::format("Task{} External", taskId.value);
            rExternal.debugGraphLongAndUgly = true;
            m_roxSyncOf[rRoxTask.external] = {
                .tag        = isSchedule ? ESyncType::PlScheduleExt : ESyncType::TaskExt,
                .taskId     = taskId,
            };
        }
    }

    auto const& pltypeinfoReg = PipelineTypeIdReg::instance();

    // connect rRoxTask.main sync to pipeline stage points
    for (TaskSyncToPipeline const& taskSync : tasks.syncs)
    {
        RoxTask           const &rRoxTask       = m_roxTaskOf[taskSync.task];
        Pipeline          const &pipeline       = tasks.pipelineInst[taskSync.pipeline];

        PipelineTypeInfo  const &pltypeinfo     = pltypeinfoReg.get(pipeline.type);
        auto              const &stageInfo      = pltypeinfo.stages[taskSync.stage];

        RoxPipeline             &rRoxPipeline   = m_roxPipelineOf[taskSync.pipeline];
        RoxLoopblk              &rRoxLoopblk    = m_roxLoopblkOf[pipeline.block];
        WtxSync                 &rTaskWtxSync   = m_wtxSyncOf[rRoxTask.main];

        LocalPointId const stagePoint = point(1u + taskSync.stage.value);

        if (   stageInfo.useCancel
            && pipeline.block == rRoxTask.parent
            && m_roxSyncOf[rRoxTask.main].tag != ESyncType::PlSchedule
            && ! vec_contains(rRoxPipeline.cancelsTasks, taskSync.task))
        {
            rRoxPipeline.cancelsTasks.push_back(taskSync.task);
            ++rTaskWtxSync.canceledByPipelines;
        }

        if (pipeline.block == rRoxTask.parent)
        {
            m_graph.connect({.sync = rRoxTask.main, .subgraphPoint = {rRoxPipeline.main, stagePoint}});
        }

        if (rRoxTask.is_spanning_nested_loopblocks())
        {
            m_graph.connect({.sync = rRoxTask.external, .subgraphPoint = {rRoxPipeline.main, stagePoint}});

            if (pipeline.block == rRoxTask.parent)
            {
                RoxPltype     const &roxPltype      = roxPltypeOf[pipeline.type];
                SubgraphType  const &sgtype         = m_graph.sgtypes[roxPltype.sgtype];
                auto          const pointCount      = sgtype.points.size();
                auto          const stageCount      = pointCount-2u;
                LocalPointId  const finish          = point(stageCount+1u);

                if ( ! vec_contains(m_graph.subgraphs[rRoxPipeline.main].points[finish].connectedSyncs, rRoxTask.sustainer ) )
                {
                    m_graph.connect({.sync = rRoxTask.sustainer, .subgraphPoint = {rRoxPipeline.main, finish}});
                }
            }
            else
            {
                m_graph.connect({.sync = rRoxTask.sustainer, .subgraphPoint = {rRoxPipeline.main, stagePoint}});
            }
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
                          rFW.m_tasks.taskInst[taskId].debugName);
        }
    }

    m_graph.debug_verify();

    m_exec.load(m_graph);

    using ESyncAction = SyncGraphExecutor::ESyncAction;

    // enable top-level loop blocks
    for (LoopBlockId const loopblkId : tasks.loopblkIds)
    {
        if ( ! tasks.loopblkInst[loopblkId].parent.has_value() )
        {
            RoxLoopblk const& rRoxLoopblk = m_roxLoopblkOf[loopblkId];
            m_exec.batch(ESyncAction::SetEnable, {rRoxLoopblk.schedule, rRoxLoopblk.left, rRoxLoopblk.right}, m_graph);
            m_exec.jump(rRoxLoopblk.subgraph, cycle(1) /*Running*/, 0, m_graph);
        }
    }

    wait(rFW);
}

void SinglethreadFWExecutor::task_finish(osp::fw::Framework &rFW, osp::TaskId taskId, bool overrideStatus, TaskActions status)
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
        finish_schedule_block(wait.loopblk, overrideStatus ? status : wait.status, rFW);
    }
    else if (wait.pipeline.has_value())
    {
        finish_schedule_pipeline(wait.pipeline, taskId, overrideStatus ? status : wait.status, true, rFW);
    }

    m_exec.batch(Unlock, {wait.syncId}, m_graph);
}

void SinglethreadFWExecutor::wait(osp::fw::Framework& rFW)
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

        for (SynchronizerId const alignedSyncId : m_justAligned)
        {
            process_aligned_sync(alignedSyncId, rFW);
        }

        m_exec.batch(SetDisable, m_disableSyncs, m_graph);
        m_disableSyncs.clear();

        m_exec.batch(Reset, m_reset, m_graph);
        m_reset.clear();

        m_justAligned.clear();
    }
}

bool SinglethreadFWExecutor::is_running(osp::fw::Framework const& rFW, LoopBlockId loopblkId)
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

void SinglethreadFWExecutor::resize_fit_syncs()
{
    auto const capacity = m_graph.syncIds.capacity();
    m_graph.syncs       .resize(capacity);
    m_roxSyncOf         .resize(capacity);
    m_wtxSyncOf         .resize(capacity);
}

void SinglethreadFWExecutor::resize_fit_subgraphs()
{
    auto const capacity = m_graph.subgraphIds.capacity();
    m_graph.subgraphs   .resize(capacity);
    m_wtxSubgraphOf     .resize(capacity);
}

void SinglethreadFWExecutor::finish_schedule_block(LoopBlockId const loopblkId, TaskActions const status, osp::fw::Framework const& rFW)
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
    }
}

void SinglethreadFWExecutor::finish_schedule_pipeline(PipelineId const pipelineId, TaskId const taskId, TaskActions const status, bool calledExternally, osp::fw::Framework const& rFW)
{
    // Default schedule tasks with no .ext_finish(true) can't ever be canceled, so they're excluded
    // from 'pipelinesRunning', to allow loopblocks to exit when all other pipelines are canceled.
    bool const canBeCancelled = taskId.has_value() || calledExternally;

    WtxPipeline &rWtxPipeline = m_wtxPipelineOf[pipelineId];
    if (rWtxPipeline.isCanceled && ! status.cancel)
    {
        if (canBeCancelled)
        {
            WtxLoopblk &rWtxLoopblk = m_wtxLoopblkOf[rFW.m_tasks.pipelineInst[pipelineId].block];
            ++rWtxLoopblk.pipelinesRunning;
        }

        RoxPipeline &rRoxPipeline = m_roxPipelineOf[pipelineId];
        rWtxPipeline.isCanceled = false;
        for (TaskId const cancelTaskId : rRoxPipeline.cancelsTasks)
        {
            SynchronizerId const cancelSyncId    = m_roxTaskOf[cancelTaskId].main;
            WtxSync              &rCancelWtxSync = m_wtxSyncOf[cancelSyncId];
            --rCancelWtxSync.canceledByPipelines;
            if (m_log && rCancelWtxSync.canceledByPipelines == 0)
            {
                SPDLOG_LOGGER_TRACE(m_log, "Enable Task{}: {}", cancelTaskId.value, rFW.m_tasks.taskInst[cancelTaskId].debugName);
            }
        }
    }
    else if ( ! rWtxPipeline.isCanceled && status.cancel )
    {
        LGRN_ASSERT(canBeCancelled);
        RoxPipeline &rRoxPipeline = m_roxPipelineOf[pipelineId];
        WtxLoopblk  &rWtxLoopblk  = m_wtxLoopblkOf[rFW.m_tasks.pipelineInst[pipelineId].block];
        rWtxPipeline.isCanceled = true;
        --rWtxLoopblk.pipelinesRunning;

        for (TaskId const cancelTaskId : rRoxPipeline.cancelsTasks)
        {
            SynchronizerId const cancelSyncId    = m_roxTaskOf[cancelTaskId].main;
            WtxSync              &rCancelWtxSync = m_wtxSyncOf[cancelSyncId];
            if (rCancelWtxSync.canceledByPipelines == 0)
            {
                SPDLOG_LOGGER_TRACE(m_log, "Disable Task{}: {}", cancelTaskId.value, rFW.m_tasks.taskInst[cancelTaskId].debugName);
            }
            ++rCancelWtxSync.canceledByPipelines;
        }
    }
}


void SinglethreadFWExecutor::process_aligned_sync(SynchronizerId const alignedSyncId, osp::fw::Framework& rFW)
{
    RoxSync const &alignedRoxSync = m_roxSyncOf[alignedSyncId];
    WtxSync       &alignedWtxSync = m_wtxSyncOf[alignedSyncId];

    bool const isPlSchedule     = alignedRoxSync.tag == ESyncType::PlSchedule;
    bool const isBlkSchedule    = alignedRoxSync.tag == ESyncType::BlkSchedule;
    bool const isTask           = (alignedRoxSync.tag == ESyncType::Task) || isPlSchedule || isBlkSchedule;

    SynchronizerId runTaskSync;

    if (isTask)
    {
        if (alignedWtxSync.canceledByPipelines != 0)
        {
            m_exec.batch(Unlock, {alignedSyncId}, m_graph);
            return;
        }
        auto const taskId = TaskId{alignedRoxSync.taskId};
        if (taskId.has_value())
        {
            WtxTask &rWtxTask = m_wtxTaskOf[taskId];
            RoxTask const& roxTask = m_roxTaskOf[taskId];
            if (rWtxTask.blockedByExternal)
            {
                // task will run once the external sync goes through process_aligned_sync
                rWtxTask.waitingForExternal = true;
                return;
            }
            else
            {
                runTaskSync = alignedSyncId;
            }
        }
        else
        {
            runTaskSync = alignedSyncId;
        }
    }
    else if (alignedRoxSync.tag == ESyncType::TaskExt || alignedRoxSync.tag == ESyncType::PlScheduleExt)
    {
        auto const taskId = TaskId{alignedRoxSync.taskId};
        WtxTask &rWtxTask = m_wtxTaskOf[taskId];

        LGRN_ASSERT(rWtxTask.blockedByExternal);

        m_disableSyncs.push_back(alignedSyncId);

        rWtxTask.blockedByExternal = false;

        if (rWtxTask.waitingForExternal)
        {
            rWtxTask.waitingForExternal = false;
            rWtxTask.blockedByExternal  = false;

            runTaskSync = m_roxTaskOf[taskId].main;
        }
        else
        {
            return;
        }
    }

    if (runTaskSync.has_value())
    {
        RoxSync const &runTaskRoxSync = m_roxSyncOf[runTaskSync];

        auto const taskId = TaskId{runTaskRoxSync.taskId};

        bool externalFinish = false;

        TaskActions status;

        if (taskId.has_value())
        {
            fw::TaskImpl const &taskImpl = rFW.m_taskImpl[taskId];

            externalFinish = taskImpl.externalFinish;

            if (taskImpl.func != nullptr)
            {
                m_argumentRefs.clear();
                m_argumentRefs.reserve(taskImpl.args.size());
                for (fw::DataId const dataId : taskImpl.args)
                {
                    entt::any data = dataId.has_value() ? rFW.m_data[dataId].as_ref() : entt::any{};
                    LGRN_ASSERTV(data.data() != nullptr, rFW.m_tasks.taskInst[taskId].debugName, taskId.value, dataId.value);
                    m_argumentRefs.push_back(std::move(data));
                }

                status = taskImpl.func(fw::WorkerContext{}, m_argumentRefs);
            }
        }

        if (externalFinish)
        {
            m_tasksWaiting.push_back({
                .taskId         = taskId,
                .status         = status,
                .syncId         = runTaskSync,
                .pipeline       = isPlSchedule  ? PipelineId{runTaskRoxSync.pipelineId} : PipelineId{},
                .loopblk        = isBlkSchedule ? LoopBlockId{runTaskRoxSync.loopBlk}   : LoopBlockId{}
            });
        }
        else
        {
            if (isPlSchedule || alignedRoxSync.tag == ESyncType::PlScheduleExt)
            {
                finish_schedule_pipeline(PipelineId{runTaskRoxSync.pipelineId}, taskId,  status, false, rFW);
            }
            else if (isBlkSchedule)
            {
                finish_schedule_block(LoopBlockId{runTaskRoxSync.loopBlk}, status, rFW);
            }

            m_exec.batch(Unlock, {runTaskSync}, m_graph);
        }

        return;
    }

    switch(alignedRoxSync.tag)
    {

    case ESyncType::BlkLeft:
    {
        RoxLoopblk const &roxLoopblk  = m_roxLoopblkOf[LoopBlockId{alignedRoxSync.loopBlk}];
        WtxLoopblk       &rWtxLoopblk = m_wtxLoopblkOf[LoopBlockId{alignedRoxSync.loopBlk}];

        if (rWtxLoopblk.state == WtxLoopblk::EState::ScheduledToRun)
        {
            rWtxLoopblk.state = WtxLoopblk::EState::Running;

            for (PipelineId const pipeline : roxLoopblk.pipelineChildren)
            {
                RoxPipeline const &roxPipeline = m_roxPipelineOf[pipeline];
                m_exec.jump(roxPipeline.main, cycle(1), std::uint8_t(roxPipeline.initialStage), m_graph);
            }

            for (LoopBlockId const childLoopblkId : roxLoopblk.loopblkChildren)
            {
                RoxLoopblk const &roxLoopblkChild = m_roxLoopblkOf[childLoopblkId];
                m_exec.jump(roxLoopblkChild.subgraph, cycle(1), 0, m_graph);
            }

            for (SynchronizerId const syncId : roxLoopblk.externals)
            {
                WtxTask &rWtxTask = m_wtxTaskOf[m_roxSyncOf[syncId].taskId];
                rWtxTask.blockedByExternal  = true;
                rWtxTask.waitingForExternal = false;
            }
            m_exec.batch(SetEnable, roxLoopblk.externals,        m_graph);
            m_exec.batch(SetEnable, roxLoopblk.associatedTasks,  m_graph);
            m_exec.batch(SetEnable, roxLoopblk.associatedOthers, m_graph);
            m_exec.batch(SetEnable, {roxLoopblk.checkstop},      m_graph);

        }

        m_exec.batch(Unlock, {alignedSyncId}, m_graph);
        break;
    }
    case ESyncType::BlkCheckStop:
    {
        RoxLoopblk const &roxLoopblk  = m_roxLoopblkOf[LoopBlockId{alignedRoxSync.loopBlk}];
        WtxLoopblk       &rWtxLoopblk = m_wtxLoopblkOf[LoopBlockId{alignedRoxSync.loopBlk}];
        if (rWtxLoopblk.pipelinesRunning == 0)
        {
            // disable every sync

            for (SynchronizerId const syncId : roxLoopblk.externals)
            {
                WtxTask &rWtxTask = m_wtxTaskOf[m_roxSyncOf[syncId].taskId];
                rWtxTask.blockedByExternal  = false;
                rWtxTask.waitingForExternal = false;
            }
            m_disableSyncs.insert(m_disableSyncs.end(), roxLoopblk.externals.begin(),
                                                        roxLoopblk.externals.end());
            m_disableSyncs.insert(m_disableSyncs.end(), roxLoopblk.associatedTasks.begin(),
                                                        roxLoopblk.associatedTasks.end());
            m_disableSyncs.insert(m_disableSyncs.end(), roxLoopblk.associatedOthers.begin(),
                                                        roxLoopblk.associatedOthers.end());

            for (LoopBlockId const childLoopblkId : roxLoopblk.loopblkChildren)
            {
                RoxLoopblk const &roxLoopblkChild = m_roxLoopblkOf[childLoopblkId];
                m_exec.jump(roxLoopblkChild.subgraph, cycle(0), 1, m_graph);

                LGRN_ASSERTM(m_wtxLoopblkOf[childLoopblkId].state == WtxLoopblk::EState::NotRunning,
                             "child pipelines must have all exited before checkstop sync");
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
                        if (m_log && rCancelWtxSync.canceledByPipelines == 0)
                        {
                            SPDLOG_LOGGER_TRACE(m_log, "Disable Task{}: {}", cancelTaskId.value, rFW.m_tasks.taskInst[cancelTaskId].debugName);
                        }
                        ++rCancelWtxSync.canceledByPipelines;
                    }
                }

                m_exec.jump(roxPipeline.main, cycle(0), 1, m_graph);
            }
            m_disableSyncs.push_back(alignedSyncId);
        }
        else
        {
            m_exec.batch(Unlock, {alignedSyncId}, m_graph);
        }
        break;
    }
    case ESyncType::BlkRight:
    {
        RoxLoopblk const &roxLoopblk  = m_roxLoopblkOf[LoopBlockId{alignedRoxSync.loopBlk}];
        WtxLoopblk       &rWtxLoopblk = m_wtxLoopblkOf[LoopBlockId{alignedRoxSync.loopBlk}];

        if (rWtxLoopblk.state == WtxLoopblk::EState::Running)
        {
            rWtxLoopblk.state = WtxLoopblk::EState::NotRunning;

            // set subgraphs back to their initial point for next iteration
            for (PipelineId const pipeline : roxLoopblk.pipelineChildren)
            {
                RoxPipeline const &roxPipeline = m_roxPipelineOf[pipeline];
                if (roxPipeline.scheduleStatus.has_value())
                {
                    m_reset.push_back(roxPipeline.scheduleStatus);
                }
            }

            for (LoopBlockId const childLoopblkId : roxLoopblk.loopblkChildren)
            {
                RoxLoopblk const &roxLoopblkChild = m_roxLoopblkOf[childLoopblkId];
                m_reset.push_back(roxLoopblkChild.scheduleStatus);
            }

        }
        m_exec.batch(Unlock, {alignedSyncId}, m_graph);

        break;
    }
    case ESyncType::TaskSus:
    case ESyncType::MaybeCancel:
        m_exec.batch(Unlock, {alignedSyncId}, m_graph);
        break;
    default:
        LGRN_ASSERTV(false, alignedSyncId.value, m_graph.syncs[alignedSyncId].debugName);
        break;
    }
}


}; // namespace osp::exec
