/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

#include "execute.h"

namespace osp
{

void exec_resize(Tasks const& tasks, ExecContext &rOut)
{
    std::size_t const maxTasks      = tasks.m_taskIds.capacity();
    std::size_t const maxPipeline   = tasks.m_pipelineIds.capacity();

    rOut.tasksQueuedRun    .reserve(maxTasks);
    rOut.tasksQueuedBlocked.reserve(maxTasks);
    rOut.plData.resize(maxPipeline);
    bitvector_resize(rOut.plDirty,     maxPipeline);

    bitvector_resize(rOut.plDirtyNext,  maxPipeline);
}

void exec_resize(Tasks const& tasks, TaskGraph const& graph, ExecContext &rOut)
{
    exec_resize(tasks, rOut);

    rOut.anystgReqByTaskCount.resize(graph.anystgToPipeline.size(), 0);

    for (std::size_t pipelineInt : tasks.m_pipelineIds.bitview().zeros())
    {
        int const stageCount = fanout_size(graph.pipelineToFirstAnystg, PipelineId(pipelineInt));
        if (stageCount == 0)
        {
            rOut.plData[PipelineId(pipelineInt)].currentStage = lgrn::id_null<StageId>();
        }
    }
}

static void exec_log(ExecContext &rExec, ExecContext::LogMsg_t msg)
{
    if (rExec.doLogging)
    {
        rExec.logMsg.push_back(msg);
    }
}

void exec_trigger(ExecContext &rExec, TplPipelineStage tpl)
{
    StageBits_t &rTriggered = rExec.plData[tpl.pipeline].triggered;

    if ( ! rTriggered.test(std::size_t(tpl.stage)) )
    {
        rExec.plDirty.set(std::size_t(tpl.pipeline));
        rTriggered.set(std::size_t(tpl.stage));

        exec_log(rExec, ExecContext::ExternalTrigger{tpl.pipeline, tpl.stage});
    }
}

static void calculate_next_stage(TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept
{
    ExecPipeline    &rExecPl    = rExec.plData[pipeline];

    rExecPl.nextStage = rExecPl.currentStage;
    rExecPl.stageChanged = false;

    if (    rExecPl.tasksQueuedBlocked  != 0
         || rExecPl.tasksQueuedRun      != 0
         || rExecPl.stageReqTaskCount   != 0
         || rExecPl.currentStage        == lgrn::id_null<StageId>())
    {
        // Can't advance. This stage still has (or requires other) tasks to complete
        // or pipeline has no stages.
        return;
    }

    int const stageCount = fanout_size(graph.pipelineToFirstAnystg, pipeline);

    if (stageCount == 0)
    {
        return;
    }

    while(true)
    {
        if (rExec.anystgReqByTaskCount[anystg_from(graph, pipeline, rExecPl.nextStage)] != 0)
        {
            // Stop here, there are queued tasks that require this stage
            return;
        }

        rExecPl.nextStage = stage_next(rExecPl.nextStage, stageCount);
        rExecPl.stageChanged = true;

        if (rExecPl.triggered.test(std::size_t(rExecPl.nextStage)))
        {
            // Stop on this stage to run tasks
            return;
        }

        // No triggered and no waiting tasks means infinite loop. Stage should still move by 1 though.
        if (   ( ! rExecPl.triggered.any() )
            && ( rExecPl.stageReqTaskCount == 0 )
            && ( rExecPl.taskReqStageCount == 0 ) )
        {
            return;
        }
    }
}

static void apply_pipeline_stage(TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
{
    ExecPipeline  &rExecPl = rExec.plData[pipeline];

    StageId const oldStage = rExecPl.currentStage;
    StageId const newStage = rExecPl.nextStage;

    if ( ! rExecPl.stageChanged )
    {
        return;
    }

    rExecPl.triggerUsed = rExecPl.triggered.test(std::size_t(newStage));
    rExecPl.triggered.reset(std::size_t(newStage));

    exec_log(rExec, ExecContext::StageChange{pipeline, oldStage, newStage});

    rExecPl.currentStage = newStage;

    LGRN_ASSERTV(rExecPl.stageReqTaskCount == 0, rExecPl.stageReqTaskCount);

    // Evaluate Stage-requires-Tasks
    // * Calculate stageReqTaskCount as the number of required task that are currently queued
    AnyStageId const    anystg              = anystg_from(graph, pipeline, newStage);
    int                 stageReqTaskCount   = 0;

    for (StageRequiresTask const& stgreqtask : fanout_view(graph.anystgToFirstStgreqtask, graph.stgreqtaskData, anystg))
    {
        if    (rExec.tasksQueuedBlocked.contains(stgreqtask.reqTask)
            || rExec.tasksQueuedRun    .contains(stgreqtask.reqTask))
        {
            ++ stageReqTaskCount;
        }
    }

    rExecPl.stageReqTaskCount = stageReqTaskCount;

    // Evaluate Task-requires-Stages
    // * Increment counts for queued tasks that depend on this stage. This unblocks tasks

    for (TaskId const& task : fanout_view(graph.anystgToFirstRevTaskreqstg, graph.revTaskreqstgToTask, anystg))
    {
        if (rExec.tasksQueuedBlocked.contains(task))
        {
            BlockedTask &rBlocked = rExec.tasksQueuedBlocked.get(task);
            -- rBlocked.remainingTaskReqStg;
            if (rBlocked.remainingTaskReqStg == 0)
            {
                exec_log(rExec, ExecContext::UnblockTask{task});
                ExecPipeline &rTaskPlExec = rExec.plData[rBlocked.pipeline];
                -- rTaskPlExec.tasksQueuedBlocked;
                ++ rTaskPlExec.tasksQueuedRun;
                rExec.tasksQueuedRun.emplace(task);
                rExec.tasksQueuedBlocked.erase(task);
            }
        }
    }
}

static void run_pipeline_tasks(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
{
    ExecPipeline        &rExecPl    = rExec.plData[pipeline];

    if ( ! rExecPl.stageChanged )
    {
        return;
    }

    if (rExecPl.nextStage == lgrn::id_null<StageId>())
    {
        return;
    }

    LGRN_ASSERT(rExecPl.tasksQueuedBlocked == 0);
    LGRN_ASSERT(rExecPl.tasksQueuedRun == 0);

    if ( ! rExecPl.triggerUsed )
    {
        return; // Not triggered
    }

    // Enqueue all tasks

    AnyStageId const    anystg      = anystg_from(graph, pipeline, rExecPl.nextStage);

    auto const runTasks = ArrayView<TaskId const>{fanout_view(graph.anystgToFirstRuntask, graph.runtaskToTask, anystg)};

    if (runTasks.size() == 0)
    {
        // No tasks to run. RunTasks are responsible for setting this pipeline dirty once they're
        // all done. If there is none, then this pipeline may get stuck if nothing sets it dirty,
        // so set dirty right away.
        rExec.plDirtyNext.set(std::size_t(pipeline));
        return;
    }

    for (TaskId task : runTasks)
    {
        bool const runsOnManyPipelines = fanout_size(graph.taskToFirstRunstage, task) > 1;
        bool const alreadyBlocked = rExec.tasksQueuedBlocked.contains(task);
        bool const alreadyRunning = rExec.tasksQueuedRun    .contains(task);
        bool const alreadyQueued  = alreadyBlocked || alreadyRunning;

        LGRN_ASSERTM( ( ! alreadyQueued ) || runsOnManyPipelines,
                     "Attempt to enqueue a single-stage task that is already running. This is impossible!");

        if (alreadyBlocked)
        {
            ++ rExecPl.tasksQueuedBlocked;
            continue;
        }

        // Evaluate Stage-requires-Tasks
        // * Increment counts for currently running stages that require this task
        for (AnyStageId const& reqTaskAnystg : fanout_view(graph.taskToFirstRevStgreqtask, graph.revStgreqtaskToStage, task))
        {
            PipelineId const    reqTaskPl       = graph.anystgToPipeline[reqTaskAnystg];
            StageId const       reqTaskStg      = stage_from(graph, reqTaskPl, reqTaskAnystg);
            ExecPipeline        &rReqTaskPlData = rExec.plData[reqTaskPl];

            if (rReqTaskPlData.currentStage == reqTaskStg)
            {
                ++ rReqTaskPlData.stageReqTaskCount;
            }
        }

        // Evaluate Task-requires-Stages
        // * Increment counts for each required stage
        // * Determine which requirements are already satisfied
        auto const  taskreqstageView    = ArrayView<const TaskRequiresStage>(fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task));
        int         reqsRemaining       = taskreqstageView.size();
        for (TaskRequiresStage const& req : taskreqstageView)
        {
            AnyStageId const    reqAnystg   = anystg_from(graph, req.reqPipeline, req.reqStage);
            ExecPipeline        &rReqPlData = rExec.plData[req.reqPipeline];

            ++ rExec.anystgReqByTaskCount[reqAnystg];
            ++ rReqPlData.taskReqStageCount;

            if (rReqPlData.currentStage == req.reqStage)
            {
                reqsRemaining --;
            }
            else if (   rReqPlData.tasksQueuedRun     == 0
                     && rReqPlData.tasksQueuedBlocked == 0 )
            {
                rExec.plDirtyNext.set(std::size_t(req.reqPipeline));
            }
        }

        bool const blocked = reqsRemaining != 0;

        if (blocked)
        {
            rExec.tasksQueuedBlocked.emplace(task, BlockedTask{reqsRemaining, pipeline});
            ++ rExecPl.tasksQueuedBlocked;
        }
        else
        {
            // Task can run right away
            rExec.tasksQueuedRun.emplace(task);
            ++ rExecPl.tasksQueuedRun;
        }

        exec_log(rExec, ExecContext::EnqueueTask{pipeline, rExecPl.nextStage, task, blocked});
        if (blocked)
        {
            for (TaskRequiresStage const& req : taskreqstageView)
            {
                ExecPipeline const  &reqPlData  = rExec.plData[req.reqPipeline];

                if (reqPlData.currentStage != req.reqStage)
                {
                    exec_log(rExec, ExecContext::EnqueueTaskReq{req.reqPipeline, req.reqStage});
                }
            }
        }
    }
}


void enqueue_dirty(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec) noexcept
{
    auto const plDirtyOnes = rExec.plDirty.ones();

    exec_log(rExec, ExecContext::EnqueueStart{});

    while (plDirtyOnes.begin() != plDirtyOnes.end())
    {
        exec_log(rExec, ExecContext::EnqueueCycle{});

        // Calculate next stages
        for (std::size_t const pipelineInt : plDirtyOnes)
        {
            calculate_next_stage(graph, rExec, PipelineId(pipelineInt));
        }

        // Apply next stages
        for (std::size_t const pipelineInt : plDirtyOnes)
        {
            apply_pipeline_stage(graph, rExec, PipelineId(pipelineInt));
        }

        // Run tasks. writes to plDirtyNext
        for (std::size_t const pipelineInt : plDirtyOnes)
        {
            run_pipeline_tasks(tasks, graph, rExec, PipelineId(pipelineInt));
        }

        // Select next dirty pipelines
        std::copy(rExec.plDirtyNext.ints().begin(),
                  rExec.plDirtyNext.ints().end(),
                  rExec.plDirty    .ints().begin());
        rExec.plDirtyNext.reset();
    }

    exec_log(rExec, ExecContext::EnqueueEnd{});
}

bool conditions_satisfied(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, TaskId const task) noexcept
{
    for (auto const [pipeline, stage] : fanout_view(graph.taskToFirstCondition, graph.conditionToPlStage, task))
    {
        ExecPipeline &rExecPl = rExec.plData[pipeline];
        if (   rExecPl.currentStage != stage
            || ! rExecPl.triggerUsed )
        {
            return false;
        }
    }

    return true;
}

void complete_task(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, TaskId const task, TriggerOut_t dirty) noexcept
{
    LGRN_ASSERT(rExec.tasksQueuedRun.contains(task));
    rExec.tasksQueuedRun.erase(task);

    exec_log(rExec, ExecContext::CompleteTask{task});

    auto const try_set_dirty = [&rExec] (ExecPipeline &plExec, PipelineId pipeline)
    {
        if (   plExec.tasksQueuedRun     == 0
            && plExec.tasksQueuedBlocked == 0
            && plExec.stageReqTaskCount  == 0)
        {
            rExec.plDirty.set(std::size_t(pipeline));
        }
    };

    // Handle task running on stage
    for (AnyStageId const anystg : fanout_view(graph.taskToFirstRunstage, graph.runstageToAnystg, task))
    {
        PipelineId const pipeline = graph.anystgToPipeline[anystg];
        ExecPipeline     &plExec  = rExec.plData[pipeline];

        -- plExec.tasksQueuedRun;
        try_set_dirty(plExec, pipeline);
    }

    // Handle stages requiring this task
    for (AnyStageId const& reqTaskAnystg : fanout_view(graph.taskToFirstRevStgreqtask, graph.revStgreqtaskToStage, task))
    {
        PipelineId const    reqTaskPl       = graph.anystgToPipeline[reqTaskAnystg];
        StageId const       reqTaskStg      = stage_from(graph, reqTaskPl, reqTaskAnystg);
        ExecPipeline        &rReqTaskPlData = rExec.plData[reqTaskPl];

        if (rReqTaskPlData.currentStage == reqTaskStg)
        {
            -- rReqTaskPlData.stageReqTaskCount;

            try_set_dirty(rReqTaskPlData, reqTaskPl);
        }
    }

    // Handle this task requiring stages
    for (TaskRequiresStage const& req : fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task))
    {
        AnyStageId const reqAnystg = anystg_from(graph, req.reqPipeline, req.reqStage);
        -- rExec.anystgReqByTaskCount[reqAnystg];
        -- rExec.plData[req.reqPipeline].taskReqStageCount;
    }

    // Trigger specified stages based on return value
    auto const triggersView = ArrayView<const TplPipelineStage>(fanout_view(graph.taskToFirstTrigger, graph.triggerToPlStage, task));
    for (int i = 0; i < triggersView.size(); ++i)
    {
        if (dirty.test(i))
        {
            auto const [pipeline, stage] = triggersView[i];
            ExecPipeline &plExec  = rExec.plData[pipeline];

            if ( ! plExec.triggered.test(std::size_t(stage)) )
            {
                plExec.triggered.set(std::size_t(stage));
                exec_log(rExec, ExecContext::CompleteTaskTrigger{pipeline, stage});
            }

            try_set_dirty(plExec, pipeline);
        }
    }
}

} // namespace osp


