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
    bitvector_resize(rOut.plAdvance,     maxPipeline);
    bitvector_resize(rOut.plAdvanceNext, maxPipeline);
    bitvector_resize(rOut.plRequestRun,  maxPipeline);
}

void exec_resize(Tasks const& tasks, TaskGraph const& graph, ExecContext &rOut)
{
    exec_resize(tasks, rOut);
}

void exec_run(ExecContext &rExec, PipelineId const pipeline)
{
    rExec.plRequestRun.set(std::size_t(pipeline));
    rExec.hasRequestRun = true;
}


static void exec_log(ExecContext &rExec, ExecContext::LogMsg_t msg)
{
    if (rExec.doLogging)
    {
        rExec.logMsg.push_back(msg);
    }
}

static constexpr bool pipeline_can_advance(TaskGraph const& graph, ExecContext const &rExec, ExecPipeline &rExecPl) noexcept
{
    int const tasksQueued = rExecPl.tasksQueuedBlocked + rExecPl.tasksQueuedRun;

    return    tasksQueued           == 0   // Tasks done
           && rExecPl.reqTasksLeft  == 0   // Tasks required by stage are done
           && rExecPl.reqByTaskLeft == 0;  // Not required by any tasks

}

static void pipeline_advance_stage(TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    LGRN_ASSERT(pipeline_can_advance(graph, rExec, rExecPl));

    int const stageCount = fanout_size(graph.pipelineToFirstAnystg, pipeline);
    LGRN_ASSERTM(stageCount != 0, "Pipelines with 0 stages shouldn't be running");

    bool const justStarting = rExecPl.stage == lgrn::id_null<StageId>();

    auto const nextStage = StageId( justStarting ? 0 : (int(rExecPl.stage)+1) );

    if (nextStage == StageId(stageCount))
    {
        // Next stage is 1 past the last stage. Finished running
        rExecPl.stage = lgrn::id_null<StageId>();
        rExecPl.running = false;
        return;
    }

    rExecPl.stage       = nextStage;
    rExecPl.tasksQueued = false;

    // asserted by pipeline_can_advance:
    // * rExecPl.reqTasksLeft == 0;
    // * rExecPl.reqByTaskLeft == 0;
}

static void pipeline_advance_reqs(TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    if ( ! rExecPl.running )
    {
        return;
    }

    AnyStageId const anystg = anystg_from(graph, pipeline, rExecPl.stage);

    // Evaluate Task-requires-Stages
    // These are tasks from other pipelines that require nextStage

    auto const revTaskReqStageView = ArrayView<TaskId const>(fanout_view(graph.anystgToFirstRevTaskreqstg, graph.revTaskreqstgToTask, anystg));

    // Number of tasks that require nextStage. This is decremented only when tasks finish
    rExecPl.reqByTaskLeft = revTaskReqStageView.size();

    // Unblock tasks that are alredy queued
    for (TaskId const task : revTaskReqStageView)
    {
        if (rExec.tasksQueuedBlocked.contains(task))
        {
            BlockedTask &rBlocked = rExec.tasksQueuedBlocked.get(task);
            -- rBlocked.reqStagesLeft;
            if (rBlocked.reqStagesLeft == 0)
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

    // Evaluate Stage-requires-Tasks
    // To be allowed to advance to the next-stage, these tasks must be complete.

    auto const stgreqtaskView = ArrayView<StageRequiresTask const>(fanout_view(graph.anystgToFirstStgreqtask, graph.stgreqtaskData, anystg));

    rExecPl.reqTasksLeft = stgreqtaskView.size();

    // Decrement reqTasksLeft, as some of these tasks might already be complete
    for (StageRequiresTask const& stgreqtask : stgreqtaskView)
    {
        ExecPipeline &rReqTaskExecPl = rExec.plData[stgreqtask.reqPipeline];

        bool const reqTaskDone = [&rReqTaskExecPl, &stgreqtask, &rExec] () noexcept -> bool
        {
            if ( ! rReqTaskExecPl.running )
            {
                return true; // Not running, which means the whole pipeline finished already
            }
            else if (int(rReqTaskExecPl.stage) < int(stgreqtask.reqStage))
            {
                return false; // Not yet reached required stage. Required task didn't run yet
            }
            else if (int(rReqTaskExecPl.stage) > int(stgreqtask.reqStage))
            {
                return true; // Passed required stage. Required task finished
            }
            else if ( ! rReqTaskExecPl.tasksQueued )
            {
                return false; // Required tasks not queued yet
            }
            else if (   rExec.tasksQueuedBlocked.contains(stgreqtask.reqTask)
                     || rExec.tasksQueuedRun    .contains(stgreqtask.reqTask))
            {
                return false; // Required task is queued and not yet finished running
            }
            else
            {
                return true; // On the right stage and task not running. This means it's done
            }
        }();

        if (reqTaskDone)
        {
            -- rExecPl.reqTasksLeft;
        }
    }
}

static void pipeline_advance_run(TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    if ( ! rExecPl.running )
    {
        return;
    }

    AnyStageId const anystg = anystg_from(graph, pipeline, rExecPl.stage);

    auto const runTasks = ArrayView<TaskId const>{fanout_view(graph.anystgToFirstRuntask, graph.runtaskToTask, anystg)};

    for (TaskId task : runTasks)
    {
        LGRN_ASSERTM( ! rExec.tasksQueuedBlocked.contains(task), "Impossible to queue a task that's already queued");
        LGRN_ASSERTM( ! rExec.tasksQueuedRun    .contains(task), "Impossible to queue a task that's already queued");

        // Evaluate Task-requires-Stages
        // Some requirements may already be satisfied
        auto const taskreqstageView = ArrayView<const TaskRequiresStage>(fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task));
        int reqStagesLeft = taskreqstageView.size();

        for (TaskRequiresStage const& req : taskreqstageView)
        {
            ExecPipeline const &rReqPlData = rExec.plData[req.reqPipeline];

            if (rReqPlData.stage == req.reqStage)
            {
                reqStagesLeft --;
            }
        }

        if (reqStagesLeft == 0)
        {
            // Task can run right away
            rExec.tasksQueuedRun.emplace(task);
            ++ rExecPl.tasksQueuedRun;
        }
        else
        {
            rExec.tasksQueuedBlocked.emplace(task, BlockedTask{reqStagesLeft, pipeline});
            ++ rExecPl.tasksQueuedBlocked;
        }
    }

    rExecPl.tasksQueued = true;

    if (runTasks.size() == 0 && pipeline_can_advance(graph, rExec, rExecPl))
    {
        // No tasks to run. RunTasks are responsible for setting this pipeline dirty once they're
        // all done. If there is none, then this pipeline may get stuck if nothing sets it dirty,
        // so set dirty right away.
        rExec.plAdvanceNext.set(std::size_t(pipeline));
        rExec.hasPlAdvance = true;
    }
}


static void run_pipeline_recurse(TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    if (fanout_size(graph.pipelineToFirstAnystg, pipeline) != 0)
    {
        rExecPl.running              = true;

        if (rExecPl.reqTasksLeft == 0)
        {
            rExec.plAdvance.set(std::size_t(pipeline));
            rExec.hasPlAdvance = true;
        }
    }

    auto const subscribers = ArrayView<PipelineId const>{fanout_view(graph.pipelineToFirstSub, graph.subToPipeline, pipeline)};

    for (PipelineId const plSub : subscribers)
    {
        run_pipeline_recurse(graph, rExec, PipelineId(plSub));
    }
}

void enqueue_dirty(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec) noexcept
{

    exec_log(rExec, ExecContext::EnqueueStart{});

    [[maybe_unused]] std::size_t const queuedTasks = rExec.tasksQueuedRun.size() + rExec.tasksQueuedBlocked.size();

    LGRN_ASSERTM( ! ( queuedTasks!=0 && rExec.plRequestRun.count()!=0 ),
                 "Running new pipelines while already running is not yet supported ROFL!");

    if (rExec.hasRequestRun)
    {
        for (PipelineInt const plInt : rExec.plRequestRun.ones())
        {
            auto const pipeline = PipelineId(plInt);
            run_pipeline_recurse(graph, rExec, pipeline);
        }
        rExec.plRequestRun.reset();
        rExec.hasRequestRun = false;
    }


    while (rExec.hasPlAdvance)
    {
        exec_log(rExec, ExecContext::EnqueueCycle{});

        rExec.hasPlAdvance = false;

        for (PipelineInt const plInt : rExec.plAdvance.ones())
        {
            pipeline_advance_stage(graph, rExec, PipelineId(plInt));
        }

        for (PipelineInt const plInt : rExec.plAdvance.ones())
        {
            pipeline_advance_reqs(graph, rExec, PipelineId(plInt));
        }

        for (PipelineInt const plInt : rExec.plAdvance.ones())
        {
            pipeline_advance_run(graph, rExec, PipelineId(plInt));
        }

        std::copy(rExec.plAdvanceNext.ints().begin(),
                  rExec.plAdvanceNext.ints().end(),
                  rExec.plAdvance    .ints().begin());
        rExec.plAdvanceNext.reset();
    }

    exec_log(rExec, ExecContext::EnqueueEnd{});
}

void complete_task(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, TaskId const task, TriggerOut_t dirty) noexcept
{
    auto const try_advance = [&graph, &rExec] (PipelineId const pipeline, ExecPipeline &rExecPl)
    {
        if (pipeline_can_advance(graph, rExec, rExecPl))
        {
            rExec.plAdvance.set(std::size_t(pipeline));
            rExec.hasPlAdvance = true;
        }
    };

    LGRN_ASSERT(rExec.tasksQueuedRun.contains(task));
    rExec.tasksQueuedRun.erase(task);

    exec_log(rExec, ExecContext::CompleteTask{task});

    auto const [pipeline, stage] = tasks.m_taskRunOn[task];
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    -- rExecPl.tasksQueuedRun;

    try_advance(pipeline, rExecPl);

    // Handle stages requiring this task
    for (AnyStageId const& reqTaskAnystg : fanout_view(graph.taskToFirstRevStgreqtask, graph.revStgreqtaskToStage, task))
    {
        PipelineId const    reqPl       = graph.anystgToPipeline[reqTaskAnystg];
        StageId const       reqStg      = stage_from(graph, reqPl, reqTaskAnystg);
        ExecPipeline        &rReqExecPl = rExec.plData[reqPl];

        if (rReqExecPl.stage == reqStg)
        {
            -- rReqExecPl.reqTasksLeft;

            try_advance(reqPl, rReqExecPl);
        }
        else
        {
            LGRN_ASSERTMV(int(rReqExecPl.stage) < int(reqStg) && rReqExecPl.stage != lgrn::id_null<StageId>(),
                          "Stage-requires-Task means that rReqExecPl.stage cannot advance any further than reqStg until task completes.",
                          int(task), int(rReqExecPl.stage), int(reqStg));
        }
    }

    // Handle this task requiring stages
    for (TaskRequiresStage const& req : fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task))
    {
        ExecPipeline &rReqExecPl = rExec.plData[req.reqPipeline];

        LGRN_ASSERTMV(rReqExecPl.stage == req.reqStage,
                      "Task-requires-Stage means this task should have not run unless the stage is selected",
                      int(task), int(rReqExecPl.stage), int(req.reqStage));

        -- rReqExecPl.reqByTaskLeft;

        try_advance(req.reqPipeline, rReqExecPl);
    }

}

} // namespace osp



#if 0


    // Step 2. Run pipelines requested to run
    for (PipelineInt const plInt : rExec.plRunning.ones())
    {
        auto const pipeline = PipelineId(plInt);

        StageInt const stgCount = fanout_size(graph.pipelineToFirstAnystg, pipeline);

        // Run all tasks?
        for (StageInt stgInt = 0; stgInt < stgCount; ++stgInt)
        {
            auto const  stg         = StageId(stgInt);
            auto const  anystg      = anystg_from(graph, pipeline, stg);
            auto const  runTasks    = ArrayView<TaskId const>{fanout_view(graph.anystgToFirstRuntask, graph.runtaskToTask, anystg)};

            for (TaskId const task : runTasks)
            {
                // there's nothing to do here?
            }
        }
    }
    rExec.plRunning.reset();



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


    }
}




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

#endif


