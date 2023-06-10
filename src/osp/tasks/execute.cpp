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

static bool eval_stage_enter_req(ExecContext &rExec, std::vector<TplTaskPipelineStage> const& reqirements)
{
    bool out = true;

    for (auto const [reqTask, reqPipeline, reqStage] : reqirements)
    {
        ExecPipeline &rReqExecPl    = rExec.m_plData[reqPipeline];
        bool const pipelineDirty    = rExec.m_plDirty.test(std::size_t(reqPipeline));
        bool const stageMatches     = rReqExecPl.m_currentStage == reqStage;
        bool const taskRunning      = rExec.m_tasksQueuedRun    .contains(reqTask);
        bool const taskBlocked      = rExec.m_tasksQueuedBlocked.contains(reqTask);
        bool const allTasksDone     =    (rReqExecPl.m_tasksQueuedRun     == 0)
                                      && (rReqExecPl.m_tasksQueuedBlocked == 0);

        // All required Pipelines must be on the correct stage to allow incrementing stage
        out &= stageMatches;

        // All required Tasks must not be in-progress (completed) to allow incrementing stage
        out &= ! (taskRunning || taskBlocked);

        if (taskBlocked)
        {
            rExec.m_tasksTryRun.set(std::size_t(reqTask));
        }

        if ( ( ! stageMatches ) && ( ! pipelineDirty ) && allTasksDone )
        {
            // Pipeline is not running tasks and not on the right stage. We need it to advance even
            // though it has no tasks to run. For this, mark it as 'required' to be set dirty later
            rExec.m_plRequired.set(std::size_t(reqPipeline));
        }
    }

    return out;
}

//static void advance_pipeline(ExecContext &rExec, TaskGraphPipeline const &graphPl, ExecPipeline &rExecPl, PipelineId const pipeline, StageId& rNextStage)
//{
//    auto const stageCount = int(graphPl.m_stages.size());

//    rNextStage = rExecPl.m_currentStage;

//    while(true)
//    {
//        auto const nextStageInt  = int(rNextStage);

//        bool const triggered        = rExecPl.m_triggered .test(nextStageInt);
//        bool const waited           = rExecPl.m_stageCounts[nextStageInt].m_waitingTasks != 0;

//        StageId const nextNextStage = stage_next(rNextStage, stageCount);
//        bool const canIncrement     = eval_stage_enter_req(rExec, graphPl.m_stages[nextNextStage].m_enterReq);

//        if ( ( ! canIncrement ) || triggered || waited )
//        {
//            return;
//        }

//        rNextStage = nextNextStage;

//        // No triggered and no waiting tasks means infinite loop. Stage should still move by 1 though.
//        if (   ( ! rExecPl.m_triggered.any() )
//            && ( rExecPl.m_waitingTasksTotal == 0 ) )
//        {
//            return;
//        }
//    }
//}

static void update_pipeline(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, PipelineId const pipeline)
{
//    ExecPipeline        &rPlExec    = rExec.m_plData[pipeline];
//    StageId const       stage       = rExec.m_plNextStage[pipeline];
//    StageBits_t const   stageBit    = 1 << int(stage);

//    rPlExec.m_currentStage = stage;

//    if ( (rPlExec.m_triggered & stageBit) == 0 )
//    {
//        return; // Not triggered
//    }

//    rPlExec.m_triggered ^= stageBit;

//    // Enqueue all tasks

//    auto const& runTasks = graph.m_pipelines[pipeline].m_stages[stage].m_runTasks;

//    for (TaskId task : runTasks)
//    {
//        bool blocked = false;
//        for (auto const [waitPipeline, waitStage] : graph.m_taskSync[TaskInt(task)])
//        {
//            ExecPipeline &rWaitPlExec = rExec.m_plData[waitPipeline];
//            ++ rWaitPlExec.m_stageCounts[std::size_t(waitStage)].m_waitingTasks;
//            ++ rWaitPlExec.m_waitingTasksTotal;

//            if (rWaitPlExec.m_currentStage != waitStage)
//            {
//                blocked = true;
//            }
//        }

//        if (blocked)
//        {
//            ++rPlExec.m_tasksQueuedBlocked;
//            rExec.m_tasksQueuedBlocked.emplace(task);
//        }
//        else
//        {
//            ++rPlExec.m_tasksQueuedRun;
//            rExec.m_tasksQueuedRun.emplace(task);
//        }
//    }
}


void enqueue_dirty(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec) noexcept
{
//    auto const task_can_run = [&graph, &rExec] (TaskId const task)
//    {
//        for (auto const [waitPipeline, waitStage] : graph.m_taskSync[TaskInt(task)])
//        {
//            if (rExec.m_plData[waitPipeline].m_currentStage != waitStage)
//            {
//                return false;
//            }
//        }

//        return true;
//    };

//    auto const plDirtyOnes = rExec.m_plDirty.ones();

//    while (plDirtyOnes.begin() != plDirtyOnes.end())
//    {
//        // 1. Advance pipelines and determine next stages. Writes to...
//        //   * rExec.m_plNextStage  - Next stages for each dirty pipeline
//        //   * rExec.m_plRequired   - Pipelines that need to be dirty next
//        //   * rExec.m_tasksTryRun  - Blocked tasks that might be able to start
//        for (std::size_t const pipelineInt : plDirtyOnes)
//        {
//            auto const pipeline = PipelineId(pipelineInt);
//            ExecPipeline &rPlExec = rExec.m_plData[pipeline];

//            if ( rPlExec.m_tasksQueuedBlocked == 0 && rPlExec.m_tasksQueuedRun == 0 )
//            {
//                advance_pipeline(rExec,
//                                 graph.m_pipelines[pipeline],
//                                 rPlExec,
//                                 pipeline,
//                                 rExec.m_plNextStage[pipeline]);
//            }
//        }

//        // 2. Apply new stages and enqueue tasks. Writes to...
//        //   * rExec.m_plData[n]            - Pipeline trigger flags and counts for queued tasks
//        //   * rExec.m_tasksQueuedBlocked   - Queued task that are blocked
//        //   * rExec.m_tasksQueuedRun       - Queued task that can run right away
//        for (std::size_t const pipelineInt : plDirtyOnes)
//        {
//            update_pipeline(tasks, graph, rExec, PipelineId(pipelineInt));
//        }

//        // 3. Try running some blocked tasks that might be able to run after pipelines advanced
//        for (std::size_t const taskInt : rExec.m_tasksTryRun.ones())
//        {
//            auto const task = TaskId(taskInt);
//            LGRN_ASSERT(rExec.m_tasksQueuedBlocked.contains(task));

//            if (task_can_run(task))
//            {
//                rExec.m_tasksQueuedBlocked.remove(task);
//                rExec.m_tasksQueuedRun.emplace(task);

//                for (auto const [pipeline, stage] : graph.m_taskRunOn[TaskInt(task)])
//                {
//                    ExecPipeline &plExec = rExec.m_plData[pipeline];

//                    --plExec.m_tasksQueuedBlocked;
//                    ++plExec.m_tasksQueuedRun;
//                }
//            }
//        }
//        rExec.m_tasksTryRun.reset();

//        // 3. 'Required' pipeline is set as dirty for next iteration.
//        std::copy(rExec.m_plRequired.ints().begin(),
//                  rExec.m_plRequired.ints().end(),
//                  rExec.m_plDirty   .ints().begin());
//        rExec.m_plRequired.reset();
//    }

//    rExec.m_plDirty.reset();
}

void mark_completed_task(Tasks const& tasks, TaskGraph const& graph, ExecContext &rExec, TaskId const task, FulfillDirty_t dirty) noexcept
{
//    LGRN_ASSERT(rExec.m_tasksQueuedRun.contains(task));
//    rExec.m_tasksQueuedRun.erase(task);

//    bool allPipelinesComplete = true;

//    for (auto const [pipeline, stage] : graph.m_taskRunOn[TaskInt(task)])
//    {
//        ExecPipeline &plExec = rExec.m_plData[pipeline];

//        plExec.m_tasksQueuedRun --;
//        if (plExec.m_tasksQueuedRun == 0)
//        {
//            rExec.m_plDirty.set(std::size_t(pipeline));
//        }
//        else
//        {
//            allPipelinesComplete = false;
//        }
//    }

//    for (auto const [waitPipeline, waitStage] : graph.m_taskSync[TaskInt(task)])
//    {
//        ExecPipeline &rWaitPlExec = rExec.m_plData[waitPipeline];
//        auto &rStgWaitingTasks = rWaitPlExec.m_stageCounts[std::size_t(waitStage)].m_waitingTasks;
//        -- rStgWaitingTasks;
//        -- rWaitPlExec.m_waitingTasksTotal;

//        if (rStgWaitingTasks == 0 && rWaitPlExec.m_tasksQueuedRun == 0 && rWaitPlExec.m_tasksQueuedBlocked == 0)
//        {
//            rExec.m_plDirty.set(std::size_t(waitPipeline));
//        }
//    }

//    if ( ! allPipelinesComplete )
//    {
//        return;
//    }

//    auto const triggers = lgrn::Span<TplPipelineStage const>(graph.m_taskTriggers[TaskInt(task)]);

//    for (int i = 0; i < triggers.size(); ++i)
//    {
//        if (dirty.test(i))
//        {
//            auto const [pipeline, stage] = triggers[i];

//            rExec.m_plData[pipeline].m_triggered |= 1 << int(stage);
//            rExec.m_plDirty.set(std::size_t(pipeline));
//        }
//    }
}

} // namespace osp


