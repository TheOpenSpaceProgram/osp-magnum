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

void exec_resize(Tasks const& tasks, TaskGraph const& graph, ExecContext &rOut)
{
    std::size_t const maxTasks      = tasks.m_taskIds.capacity();
    std::size_t const maxPipeline   = tasks.m_pipelineIds.capacity();

    rOut.tasksQueuedRun    .reserve(maxTasks);
    rOut.tasksQueuedBlocked.reserve(maxTasks);
    bitvector_resize(rOut.tasksTryRun, maxTasks);

    rOut.plData.resize(maxPipeline);
    bitvector_resize(rOut.plDirty,     maxPipeline);

    rOut.plNextStage.resize(maxPipeline);
    bitvector_resize(rOut.plDirtyNext,  maxPipeline);

    rOut.anystgReqByTaskCount.resize(graph.anystgToPipeline.size(), 0);
}

static StageId pipeline_next_stage(TaskGraph const& graph, ExecContext const &exec, ExecPipeline const &execPl, PipelineId const pipeline) noexcept
{
    StageId out = execPl.currentStage;

    if (    execPl.tasksQueuedBlocked  != 0
         || execPl.tasksQueuedRun      != 0
         || execPl.stageReqTaskCount   != 0)
    {
        return out; // Can't advance. This stage still has (or requires other) tasks to complete
    }

    int const stageCount = fanout_size(graph.pipelineToFirstAnystg, pipeline);

    while(true)
    {
        if (exec.anystgReqByTaskCount[anystg_from(graph, pipeline, out)])
        {
            return out; // Can't advance, there are queued tasks that require this stage
        }

        out = stage_next(out, stageCount);

        if (execPl.triggered.test(std::size_t(out)))
        {
            return out; // Stop on this stage to run tasks
        }

        // No triggered and no waiting tasks means infinite loop. Stage should still move by 1 though.
        if (   ( ! execPl.triggered.any() )
            && ( execPl.stageReqTaskCount == 0 )
            && ( execPl.taskReqStageCount == 0 ) )
        {
            return out;
        }
    }
}

static void apply_pipeline_stage(TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
{
    ExecPipeline  &rPlExec = rExec.plData[pipeline];
    StageId       &rStage  = rPlExec.currentStage;
    StageId const oldStage = rStage;
    StageId const newStage = rExec.plNextStage[pipeline];

    if (oldStage == newStage)
    {
        return; // no change
    }

    rStage = newStage;

    LGRN_ASSERTV(rPlExec.stageReqTaskCount == 0, rPlExec.stageReqTaskCount);

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

    rPlExec.stageReqTaskCount = stageReqTaskCount;

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
    ExecPipeline        &rPlExec    = rExec.plData[pipeline];
    StageId const       stage       = rExec.plNextStage[pipeline];
    StageBits_t const   stageBit    = 1 << int(stage);

    LGRN_ASSERT(rPlExec.tasksQueuedBlocked == 0);
    LGRN_ASSERT(rPlExec.tasksQueuedRun == 0);

    if ( (rPlExec.triggered & stageBit) == 0 )
    {
        return; // Not triggered
    }

    rPlExec.triggered &= ~stageBit;

    // Enqueue all tasks

    AnyStageId const    anystg      = anystg_from(graph, pipeline, stage);

    for (TaskId task : fanout_view(graph.anystgToFirstRuntask, graph.runtaskToTask, anystg))
    {
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

        if (reqsRemaining != 0)
        {
            rExec.tasksQueuedBlocked.emplace(task, BlockedTask{reqsRemaining, pipeline});
            ++ rPlExec.tasksQueuedBlocked;
        }
        else
        {
            // Task can run right away
            rExec.tasksQueuedRun.emplace(task);
            ++ rPlExec.tasksQueuedRun;
        }
    }
}


void enqueue_dirty(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec) noexcept
{
    auto const plDirtyOnes = rExec.plDirty.ones();

    while (plDirtyOnes.begin() != plDirtyOnes.end())
    {
        // Calculate next stages
        for (std::size_t const pipelineInt : plDirtyOnes)
        {
            auto const   pipeline       = PipelineId(pipelineInt);
            ExecPipeline &rPlExec       = rExec.plData[pipeline];
            rExec.plNextStage[pipeline] = pipeline_next_stage(graph, rExec, rPlExec, pipeline);
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
}

void mark_completed_task(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, TaskId const task, FulfillDirty_t dirty) noexcept
{
    LGRN_ASSERT(rExec.tasksQueuedRun.contains(task));
    rExec.tasksQueuedRun.erase(task);

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

            plExec.triggered |= 1 << int(stage);
            try_set_dirty(plExec, pipeline);
        }
    }
}

} // namespace osp


