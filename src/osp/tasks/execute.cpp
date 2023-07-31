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

static void exec_log(ExecContext &rExec, ExecContext::LogMsg_t msg) noexcept;

static void pipeline_advance_stage(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept;

static void pipeline_advance_reqs(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept;

static void pipeline_advance_run(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept;

static constexpr bool pipeline_can_advance(TaskGraph const& graph, ExecContext const &rExec, ExecPipeline &rExecPl) noexcept;

static inline void pipeline_try_advance(TaskGraph const& graph, ExecContext &rExec, ExecPipeline &rExecPl, PipelineId const pipeline) noexcept;

static void pipeline_cancel(Tasks const& tasks, TaskGraph const& graph, ExecContext& rExec, ExecPipeline& rExecPl, PipelineId pipeline) noexcept;

struct ArgsForIsPipelineInLoop
{
    PipelineId viewedFrom;
    PipelineId insideLoop;
};

static bool is_pipeline_in_loop(Tasks const& tasks, TaskGraph const& graph, ArgsForIsPipelineInLoop const args) noexcept;

//-----------------------------------------------------------------------------

// Main entry-point functions

void exec_update(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec) noexcept
{
    auto const run_pipeline = [&graph, &rExec] (ExecPipeline& rExecPl, PipelineId const pipeline, bool const loop)
    {
        if (fanout_size(graph.pipelineToFirstAnystg, pipeline) != 0)
        {
            rExecPl.running  = true;
            rExecPl.loop     = loop;

            LGRN_ASSERT(rExecPl.ownStageReqTasksLeft == 0);
            {
                rExec.plAdvance.set(std::size_t(pipeline));
                rExec.hasPlAdvanceOrLoop = true;
            }
        }
    };

    exec_log(rExec, ExecContext::EnqueueStart{});

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
                run_pipeline(rExec.plData[pipeline], pipeline, tasks.m_pipelineControl[pipeline].loops);
            }
            else
            {
                uint32_t const          descendants = graph.pltreeDescendantCounts[treePos];
                PipelineTreePos_t const lastPos     = treePos + 1 + descendants;

                uint32_t loopNextN = 0;

                for (PipelineTreePos_t runPos = treePos; runPos < lastPos; ++runPos)
                {
                    PipelineId const runPipeline = graph.pltreeToPipeline[runPos];
                    ExecPipeline     &rRunExecPl = rExec.plData[runPipeline];

                    if (tasks.m_pipelineControl[runPipeline].loops)
                    {
                        uint32_t const runDescendants = graph.pltreeDescendantCounts[runPos];
                        rRunExecPl.loopPipelinesLeft = 1 + runDescendants;

                        // Loop this pipeline and all of its descendants
                        loopNextN = std::max(loopNextN, 1 + runDescendants);
                    }

                    run_pipeline(rRunExecPl, runPipeline, loopNextN != 0);

                    loopNextN -= (loopNextN != 0);
                }
            }
        }
        rExec.plRequestRun.reset();
        rExec.hasRequestRun = false;
    }


    while (rExec.hasPlAdvanceOrLoop)
    {
        exec_log(rExec, ExecContext::EnqueueCycle{});

        rExec.hasPlAdvanceOrLoop = false;

        for (auto const [pipeline, treePos] : rExec.requestLoop)
        {
            ExecPipeline &rExecPl = rExec.plData[pipeline];
            LGRN_ASSERT(rExecPl.loopPipelinesLeft == 0);


        }
        rExec.requestLoop.clear();

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

    if (actions & TaskAction::Cancel)
    {
        LGRN_ASSERTMV(rExecPl.tasksQueuedRun == 0 && rExecPl.tasksQueuedBlocked == 0,
                      "Tasks that cancel the pipeline should be the only task running. Too lazy to enforce this elsewhere LOL",
                      rExecPl.tasksQueuedRun, rExecPl.tasksQueuedBlocked);
        pipeline_cancel(tasks, graph, rExec, rExecPl, pipeline);
    }

    // Handle stages requiring this task
    for (AnyStageId const& reqTaskAnystg : fanout_view(graph.taskToFirstRevStgreqtask, graph.revStgreqtaskToStage, task))
    {
        PipelineId const    reqPl       = graph.anystgToPipeline[reqTaskAnystg];
        StageId const       reqStg      = stage_from(graph, reqPl, reqTaskAnystg);
        ExecPipeline        &rReqExecPl = rExec.plData[reqPl];

        if (rReqExecPl.stage == reqStg)
        {
            if ( ! (rExecPl.loop && is_pipeline_in_loop(tasks, graph, {.viewedFrom = reqPl, .insideLoop = pipeline})) || rExecPl.canceled )
            {
                -- rReqExecPl.ownStageReqTasksLeft;

                pipeline_try_advance(graph, rExec, rReqExecPl, reqPl);
            }
            // else: This pipeline is enclosed in a loop relative to dependency. This task may run
            //       more times.
        }
        else
        {
            LGRN_ASSERTMV(int(rReqExecPl.stage) < int(reqStg) && rReqExecPl.stage != lgrn::id_null<StageId>(),
                          "Stage-requires-Task means that rReqExecPl.stage cannot advance any further than reqStg until task completes.",
                          int(task), int(rReqExecPl.stage), int(reqStg));
        }
    }

    // Handle this task requiring stages from other pipelines
    for (TaskRequiresStage const& req : fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task))
    {
        ExecPipeline &rReqExecPl = rExec.plData[req.reqPipeline];

        LGRN_ASSERTMV(rReqExecPl.stage == req.reqStage,
                      "Task-requires-Stage means this task should have not run unless the stage is selected",
                      int(task), int(rReqExecPl.stage), int(req.reqStage));

        if ( ! (rExecPl.loop && is_pipeline_in_loop(tasks, graph, {.viewedFrom = req.reqPipeline, .insideLoop = pipeline})) || rExecPl.canceled )
        {
            -- rReqExecPl.tasksReqOwnStageLeft;

            pipeline_try_advance(graph, rExec, rReqExecPl, req.reqPipeline);
        }
        // else: This pipeline is enclosed in a loop relative to dependency. This task may run
        //       more times.
    }
}

//-----------------------------------------------------------------------------

// Major steps

static void pipeline_advance_stage(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept
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
    else if ( rExecPl.loop )
    {
        // Loop done
        rExecPl.stage = lgrn::id_null<StageId>();

        PipelineTreePos_t const scopeTreePos = graph.pipelineToLoopScope[pipeline];
        PipelineId const        scopePl      = graph.pltreeToPipeline[scopeTreePos];

        ExecPipeline &rScopeExecPl = rExec.plData[scopePl];
        LGRN_ASSERT(rScopeExecPl.loopPipelinesLeft != 0);
        -- rScopeExecPl.loopPipelinesLeft;

        if (rScopeExecPl.loopPipelinesLeft == 0)
        {
            // all loop pipelines parented to this are done
            if ( ! rScopeExecPl.canceled )
            {
                // Loop more

                uint32_t const          descendants = graph.pltreeDescendantCounts[scopeTreePos];
                PipelineTreePos_t const lastPos     = scopeTreePos + 1 + descendants;

                for (PipelineTreePos_t loopPos = scopeTreePos; loopPos < lastPos; ++loopPos)
                {
                    PipelineId const loopPipeline = graph.pltreeToPipeline[loopPos];
                    ExecPipeline     &rLoopExecPl = rExec.plData[loopPipeline];

                    rExec.plAdvanceNext.set(std::size_t(loopPipeline));
                    rExec.hasPlAdvanceOrLoop = true;
                }

                rScopeExecPl.loopPipelinesLeft = 1 + descendants;
            }
            else
            {
                // Loop finished

                uint32_t const          descendants = graph.pltreeDescendantCounts[scopeTreePos];
                PipelineTreePos_t const lastPos     = scopeTreePos + 1 + descendants;

                for (PipelineTreePos_t loopPos = scopeTreePos; loopPos < lastPos; ++loopPos)
                {
                    PipelineId const loopPipeline = graph.pltreeToPipeline[loopPos];
                    ExecPipeline     &rLoopExecPl = rExec.plData[loopPipeline];

                    rLoopExecPl.running  = false;
                    rLoopExecPl.canceled = false;
                    rLoopExecPl.loop     = false;
                }
            }
        }
    }
    else
    {
        // Finished running
        rExecPl.stage    = lgrn::id_null<StageId>();
        rExecPl.running  = false;
        rExecPl.canceled = false;
    }
}

static void pipeline_advance_reqs(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    if (rExecPl.stage == lgrn::id_null<StageId>())
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

static void pipeline_advance_run(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline) noexcept
{
    ExecPipeline &rExecPl = rExec.plData[pipeline];

    if (rExecPl.stage == lgrn::id_null<StageId>())
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
        rExec.hasPlAdvanceOrLoop = true;
    }
}

//-----------------------------------------------------------------------------

// Pipeline utility

static void pipeline_cancel(Tasks const& tasks, TaskGraph const& graph, ExecContext& rExec, ExecPipeline& rExecPl, PipelineId pipeline) noexcept
{
    auto const decrement_stage_dependencies = [&graph, &rExec] (AnyStageId const anystg)
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
    };

    PipelineTreePos_t const treePos = graph.pipelineToPltree[pipeline];

    bool const skipDependencies = [&graph, &rExecPl, pipeline, treePos] ()
    {
        if (rExecPl.loop)
        {
            PipelineTreePos_t const loopScope = graph.pipelineToLoopScope[pipeline];

            bool const inNestedLoop =    loopScope != lgrn::id_null<PipelineTreePos_t>()
                                      && loopScope != treePos;

            if (inNestedLoop)
            {
                // Even if canceled, this pipeline may run again
                return true;
            }
        }
        return false;
    }();

    uint32_t const          descendants = graph.pltreeDescendantCounts[treePos];
    PipelineTreePos_t const lastPos     = treePos + 1 + descendants;

    // Iterate descendents
    for (PipelineTreePos_t cancelPos = treePos; cancelPos < lastPos; ++cancelPos)
    {
        PipelineId const cancelPl = graph.pltreeToPipeline[cancelPos];
        ExecPipeline &rCancelExecPl = rExec.plData[cancelPl];

        if ( ! rCancelExecPl.canceled && ! skipDependencies )
        {
            int const stageCount = fanout_size(graph.pipelineToFirstAnystg, cancelPl);

            int cancelStage = int(rCancelExecPl.stage) + 1;

            auto anystgInt = uint32_t(anystg_from(graph, cancelPl, StageId(cancelStage)));
            for (auto stgInt = cancelStage; stgInt < stageCount; ++stgInt, ++anystgInt)
            {
                decrement_stage_dependencies(AnyStageId(anystgInt));
            }
        }

        rCancelExecPl.canceled = true;
    }
}

static inline void pipeline_try_advance(TaskGraph const& graph, ExecContext &rExec, ExecPipeline &rExecPl, PipelineId const pipeline) noexcept
{
    if (pipeline_can_advance(graph, rExec, rExecPl))
    {
        rExec.plAdvance.set(std::size_t(pipeline));
        rExec.hasPlAdvanceOrLoop = true;
    }
};

//-----------------------------------------------------------------------------

// Read-only checks

static constexpr bool pipeline_can_advance(TaskGraph const& graph, ExecContext const &rExec, ExecPipeline &rExecPl) noexcept
{
    return    rExecPl.ownStageReqTasksLeft == 0   // Tasks required by stage are done
           && rExecPl.tasksReqOwnStageLeft == 0   // Not required by any tasks
           && (rExecPl.tasksQueuedBlocked+rExecPl.tasksQueuedRun) == 0; // Tasks done

}

/**
 * @brief Check if a pipeline 'sees' another pipeline is enclosed within a loop
 */
static bool is_pipeline_in_loop(Tasks const& tasks, TaskGraph const& graph, ArgsForIsPipelineInLoop const args) noexcept
{
    PipelineTreePos_t const insideLoopScopePos = graph.pipelineToLoopScope[args.insideLoop];

    if (insideLoopScopePos == lgrn::id_null<PipelineTreePos_t>())
    {
        return false; // insideLoop is in the root, not inside a loop lol
    }

    PipelineTreePos_t const viewedFromScopePos = graph.pipelineToLoopScope[args.viewedFrom];

    if (insideLoopScopePos == viewedFromScopePos)
    {
        return false; // Both pipelines are in the same loop scope
    }
    else if (viewedFromScopePos == lgrn::id_null<PipelineTreePos_t>())
    {
        return true; // viewedFrom is in the root, and insideLoop is enclosed in a loop (which has to be enclosed in root of course)
    }
    else
    {
        // Check tree if insideLoopScopePos is a descendent of viewedFromScopePos
        PipelineTreePos_t const ancestor            = viewedFromScopePos;
        PipelineTreePos_t const ancestorLastChild   = viewedFromScopePos + 1 + graph.pltreeDescendantCounts[viewedFromScopePos];
        PipelineTreePos_t const descendent          = insideLoopScopePos;

        return (ancestor < descendent) && (descendent < ancestorLastChild);
    }
}

//-----------------------------------------------------------------------------

// Minor utility

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

static void exec_log(ExecContext &rExec, ExecContext::LogMsg_t msg) noexcept
{
    if (rExec.doLogging)
    {
        rExec.logMsg.push_back(msg);
    }
}


} // namespace osp
