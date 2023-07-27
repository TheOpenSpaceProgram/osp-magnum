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

#include <Corrade/Containers/ArrayViewStl.h>

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

void pipeline_run(ExecContext &rExec, PipelineId const pipeline)
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

static bool pipeline_is_ancestor_descendant(Tasks const& tasks, TaskGraph const& graph, PipelineId const ancestor, PipelineId const descendant)
{
    //tasks.
}

static constexpr bool pipeline_can_advance(TaskGraph const& graph, ExecContext const &rExec, ExecPipeline &rExecPl) noexcept
{
    return    rExecPl.ownStageReqTasksLeft == 0   // Tasks required by stage are done
           && rExecPl.tasksReqOwnStageLeft == 0   // Not required by any tasks
           && (rExecPl.tasksQueuedBlocked+rExecPl.tasksQueuedRun) == 0; // Tasks done

}

static inline void pipeline_try_advance(TaskGraph const& graph, ExecContext &rExec, ExecPipeline &rExecPl, PipelineId const pipeline)
{
    if (pipeline_can_advance(graph, rExec, rExecPl))
    {
        rExec.plAdvance.set(std::size_t(pipeline));
        rExec.hasPlAdvance = true;
    }
};

static void pipeline_advance_stage(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    LGRN_ASSERT(pipeline_can_advance(graph, rExec, rExecPl));
    // * rExecPl.ownStageReqTasksLeft == 0;
    // * rExecPl.tasksReqOwnStageLeft == 0;

    int const stageCount = fanout_size(graph.pipelineToFirstAnystg, pipeline);
    LGRN_ASSERTM(stageCount != 0, "Pipelines with 0 stages shouldn't be running");

    bool const justStarting = rExecPl.stage == lgrn::id_null<StageId>();

    auto const nextStage = StageId( justStarting ? 0 : (int(rExecPl.stage)+1) );

    rExecPl.tasksQueueDone = false;

    if (nextStage != StageId(stageCount))
    {
        // Proceed to next stage since its valid
        rExecPl.stage = nextStage;
    }
    else if ( rExecPl.loop && ! rExecPl.canceled )
    {
        // Loop back to first stage
        rExecPl.stage = StageId(0);
    }
    else
    {
        // Finished running
        rExecPl.stage    = lgrn::id_null<StageId>();
        rExecPl.running  = false;
        rExecPl.canceled = false;
        rExecPl.loop     = false;
    }
}

static void pipeline_advance_reqs(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
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

    // Number of tasks that require this stage. This is decremented when required tasks finish
    rExecPl.tasksReqOwnStageLeft = revTaskReqStageView.size();

    for (TaskId const task : revTaskReqStageView)
    {
        if (rExec.tasksQueuedBlocked.contains(task))
        {
            // Unblock tasks that are alredy queued
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
        else
        {
            auto const [reqPipeline, reqStage] = tasks.m_taskRunOn[task];
            ExecPipeline &rReqExecPl = rExec.plData[reqPipeline];
            if ( ! rReqExecPl.running || rReqExecPl.canceled)
            {
                // Task is done or cancelled
                -- rExecPl.tasksReqOwnStageLeft;
            }
        }
    }

    // Evaluate Stage-requires-Tasks
    // To be allowed to advance to the next-stage, these tasks must be complete.

    auto const stgreqtaskView = ArrayView<StageRequiresTask const>(fanout_view(graph.anystgToFirstStgreqtask, graph.stgreqtaskData, anystg));

    rExecPl.ownStageReqTasksLeft = stgreqtaskView.size();

    // Decrement ownStageReqTasksLeft, as some of these tasks might already be complete
    for (StageRequiresTask const& stgreqtask : stgreqtaskView)
    {
        ExecPipeline &rReqTaskExecPl = rExec.plData[stgreqtask.reqPipeline];

        bool const reqTaskDone = [&tasks, &rReqTaskExecPl, &stgreqtask, &rExec] () noexcept -> bool
        {
            if ( ! rReqTaskExecPl.running )
            {
                return true; // Not running, which means the whole pipeline finished already
            }
            else if (rReqTaskExecPl.canceled)
            {
                return true; // Stage cancelled. Required task is considered finish and will never run
            }
            else if (int(rReqTaskExecPl.stage) < int(stgreqtask.reqStage))
            {
                return false; // Not yet reached required stage. Required task didn't run yet
            }
            else if (int(rReqTaskExecPl.stage) > int(stgreqtask.reqStage))
            {
                return true; // Passed required stage. Required task finished
            }
            else if ( ! rReqTaskExecPl.tasksQueueDone )
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
            -- rExecPl.ownStageReqTasksLeft;
        }
    }
}

static void pipeline_advance_run(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    if ( ! rExecPl.running )
    {
        return;
    }

    bool noTasksRun = true;

    if ( ! rExecPl.canceled )
    {
        auto const anystg   = anystg_from(graph, pipeline, rExecPl.stage);
        auto const runTasks = ArrayView<TaskId const>{fanout_view(graph.anystgToFirstRuntask, graph.runtaskToTask, anystg)};

        noTasksRun = runTasks.size() == 0;

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
    }

    rExecPl.tasksQueueDone = true;

    if (noTasksRun && pipeline_can_advance(graph, rExec, rExecPl))
    {
        // No tasks to run. RunTasks are responsible for setting this pipeline dirty once they're
        // all done. If there is none, then this pipeline may get stuck if nothing sets it dirty,
        // so set dirty right away.
        rExec.plAdvanceNext.set(std::size_t(pipeline));
        rExec.hasPlAdvance = true;
    }
}


static void run_pipeline_recurse(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline, bool loop = false)
{


    //auto const children = ArrayView<PipelineId const>{fanout_view(graph.pipelineToFirstChild, graph.childPlToParent, pipeline)};

    //for (PipelineId const plSub : children)
    //{
    //    run_pipeline_recurse(tasks, graph, rExec, PipelineId(plSub), loop);
    //}
}

static void cancel_stage(TaskGraph const& graph, ExecContext& rExec, AnyStageId const anystg)
{
    auto const runTasks = ArrayView<TaskId const>{fanout_view(graph.anystgToFirstRuntask, graph.runtaskToTask, anystg)};

    for (TaskId task : runTasks)
    {
        // Stages depend on this RunTask (reverse Stage-requires-Task)
        for (AnyStageId const& reqTaskAnystg : fanout_view(graph.taskToFirstRevStgreqtask, graph.revStgreqtaskToStage, task))
        {
            PipelineId const    reqPl       = graph.anystgToPipeline[reqTaskAnystg];
            StageId const       reqStg      = stage_from(graph, reqPl, reqTaskAnystg);
            ExecPipeline        &rReqExecPl = rExec.plData[reqPl];

            if (rReqExecPl.stage == reqStg)
            {
                LGRN_ASSERT(rReqExecPl.ownStageReqTasksLeft != 0);
                -- rReqExecPl.ownStageReqTasksLeft;
                pipeline_try_advance(graph, rExec, rReqExecPl, reqPl);
            }
        }

        // RunTask depends on stages (Task-requires-Stage)
        for (TaskRequiresStage const& req : fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task))
        {
            ExecPipeline &rReqExecPl = rExec.plData[req.reqPipeline];

            if (rReqExecPl.stage == req.reqStage)
            {
                LGRN_ASSERT(rReqExecPl.tasksReqOwnStageLeft != 0);
                -- rReqExecPl.tasksReqOwnStageLeft;
                pipeline_try_advance(graph, rExec, rReqExecPl, req.reqPipeline);
            }
        }
    }
}

void pipeline_cancel(Tasks const& tasks, TaskGraph const& graph, ExecContext& rExec, ExecPipeline& rExecPl, PipelineId pipeline)
{
    if (rExecPl.canceled)
    {
        return; // already cancelled
    }

    int const         stageCount     = fanout_size(graph.pipelineToFirstAnystg, pipeline);

    auto anystgInt = uint32_t(anystg_from(graph, pipeline, rExecPl.stage));
    for (auto stgInt = int(rExecPl.stage); stgInt < stageCount; ++stgInt, ++anystgInt)
    {
        cancel_stage(graph, rExec, AnyStageId(anystgInt));
    }

    rExecPl.canceled = true;
}

void pipeline_cancel_loop(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId pipeline)
{

}

void enqueue_dirty(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec) noexcept
{
    exec_log(rExec, ExecContext::EnqueueStart{});

    auto run_pipeline = [&graph, &rExec] (PipelineId const pipeline, bool const loop)
    {
        ExecPipeline &rExecPl = rExec.plData[pipeline];
        if (fanout_size(graph.pipelineToFirstAnystg, pipeline) != 0)
        {
            rExecPl.running  = true;
            rExecPl.loop     = loop;

            if (rExecPl.ownStageReqTasksLeft == 0)
            {
                rExec.plAdvance.set(std::size_t(pipeline));
                rExec.hasPlAdvance = true;
            }
        }
    };

    if (rExec.hasRequestRun)
    {
        for (ExecPipeline const& rExecPl : rExec.plData)
        {
            LGRN_ASSERTM( ! rExecPl.running, "Running new pipelines while already running is not yet supported ROFL!");
        }

        for (PipelineInt const plInt : rExec.plRequestRun.ones())
        {
            auto const pipeline = PipelineId(plInt);

            PipelineTreePos_t const treePos = graph.pipelineToPltree[pipeline];

            if (treePos == lgrn::id_null<PipelineTreePos_t>())
            {
                // Pipeline not in tree
                run_pipeline(pipeline, tasks.m_pipelineControl[pipeline].loops);
            }
            else
            {
                uint32_t const          descendants = graph.pltreeDescendantCounts[treePos];
                PipelineTreePos_t const lastPos     = treePos + 1 + descendants;

                uint32_t loopNextN = 0;

                // Iterate descendents
                for (PipelineTreePos_t run = treePos; run < lastPos; ++run)
                {
                    PipelineId const runPipeline = graph.pltreeToPipeline[run];

                    if (tasks.m_pipelineControl[runPipeline].loops)
                    {
                        // Loop this pipeline and all of its descendants
                        loopNextN = std::max(loopNextN, 1 + graph.pltreeDescendantCounts[run]);
                    }

                    run_pipeline(runPipeline, loopNextN != 0);

                    loopNextN -= (loopNextN != 0);
                }
            }
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
            pipeline_advance_stage(tasks, graph, rExec, PipelineId(plInt));
        }

        for (PipelineInt const plInt : rExec.plAdvance.ones())
        {
            pipeline_advance_reqs(tasks, graph, rExec, PipelineId(plInt));
        }

        for (PipelineInt const plInt : rExec.plAdvance.ones())
        {
            pipeline_advance_run(tasks, graph, rExec, PipelineId(plInt));
        }

        std::copy(rExec.plAdvanceNext.ints().begin(),
                  rExec.plAdvanceNext.ints().end(),
                  rExec.plAdvance    .ints().begin());
        rExec.plAdvanceNext.reset();
    }

    exec_log(rExec, ExecContext::EnqueueEnd{});
}

void complete_task(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, TaskId const task, TaskActions actions) noexcept
{
    LGRN_ASSERT(rExec.tasksQueuedRun.contains(task));
    rExec.tasksQueuedRun.erase(task);

    exec_log(rExec, ExecContext::CompleteTask{task});

    auto const [pipeline, stage] = tasks.m_taskRunOn[task];
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    -- rExecPl.tasksQueuedRun;

    pipeline_try_advance(graph, rExec, rExecPl, pipeline);

    // Handle stages requiring this task
    for (AnyStageId const& reqTaskAnystg : fanout_view(graph.taskToFirstRevStgreqtask, graph.revStgreqtaskToStage, task))
    {
        PipelineId const    reqPl       = graph.anystgToPipeline[reqTaskAnystg];
        StageId const       reqStg      = stage_from(graph, reqPl, reqTaskAnystg);
        ExecPipeline        &rReqExecPl = rExec.plData[reqPl];

        if (rReqExecPl.stage == reqStg)
        {
            -- rReqExecPl.ownStageReqTasksLeft;

            pipeline_try_advance(graph, rExec, rReqExecPl, reqPl);
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

        -- rReqExecPl.tasksReqOwnStageLeft;

        pipeline_try_advance(graph, rExec, rReqExecPl, req.reqPipeline);
    }

    // Handle actions

    if (actions & TaskAction::Cancel)
    {
        pipeline_cancel(tasks, graph, rExec, rExecPl, pipeline);
    }

}

} // namespace osp
