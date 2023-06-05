/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "tasks.h"

namespace osp
{

struct TaskCounts
{
    std::size_t runOn           {0};
    std::size_t syncs           {0};
    std::size_t triggers        {0};
};

struct PipelineCounts
{
    std::size_t runTasks        {0};
    std::size_t syncedBy        {0};
    std::size_t triggeredBy     {0};
    uint8_t     stages          {0};
};

struct TargetCounts
{

};


TaskGraph make_exec_graph(Tasks const& tasks, ArrayView<TaskEdges const* const> const data)
{
    TaskGraph out;

    std::size_t const maxPipelines  = tasks.m_pipelineIds.capacity();
    std::size_t const maxTasks      = tasks.m_taskIds.capacity();

    KeyedVec<PipelineId, PipelineCounts>    plCounts;
    KeyedVec<TaskId, TaskCounts>            taskCounts;

    out.m_pipelines .resize(maxPipelines);
    plCounts        .resize(maxPipelines);
    taskCounts      .resize(maxTasks);

    std::size_t runTotal     = 0;
    std::size_t syncTotal    = 0;
    std::size_t triggerTotal = 0;

    // Count

    for (TaskEdges const* pEdges : data)
    {
        runTotal     += pEdges->m_runOn   .size();
        syncTotal    += pEdges->m_syncWith.size();
        triggerTotal += pEdges->m_triggers.size();

        auto const count_stage = [&plCounts] (PipelineId const pipeline, StageId const stage)
        {
            uint8_t &rStageCount = plCounts[pipeline].stages;
            rStageCount = std::max(rStageCount, uint8_t(uint8_t(stage) + 1));
        };

        for (auto const [task, pipeline, stage] : pEdges->m_runOn)
        {
            plCounts[pipeline]  .runTasks ++;
            taskCounts[task]    .runOn ++;
            count_stage(pipeline, stage);
        }

        for (auto const [task, pipeline, stage] : pEdges->m_syncWith)
        {
            plCounts[pipeline]  .syncedBy ++;
            taskCounts[task]    .syncs ++;
            count_stage(pipeline, stage);
        }

        for (auto const [task, pipeline, stage] : pEdges->m_triggers)
        {
            plCounts[pipeline]  .triggeredBy ++;
            taskCounts[task]    .triggers ++;
            count_stage(pipeline, stage);
        }
    }

    // Allocate / reserve

    out.m_taskRunOn     .ids_reserve(maxTasks);
    out.m_taskSync      .ids_reserve(maxTasks);
    out.m_taskTriggers  .ids_reserve(maxTasks);
    out.m_taskRunOn     .data_reserve(runTotal);
    out.m_taskSync      .data_reserve(syncTotal);
    out.m_taskTriggers  .data_reserve(triggerTotal);

    for (std::size_t const pipelineInt : tasks.m_pipelineIds.bitview().zeros())
    {
        auto const pipeline = PipelineId(pipelineInt);
        out.m_pipelines[pipeline].m_stages.resize(plCounts[pipeline].stages);
    }

    for (TaskInt const taskInt : tasks.m_taskIds.bitview().zeros())
    {
        TaskCounts &rCounts = taskCounts[TaskId(taskInt)];
        if (rCounts.runOn != 0)    { out.m_taskRunOn   .emplace(taskInt, rCounts.runOn); }
        if (rCounts.syncs != 0)    { out.m_taskSync    .emplace(taskInt, rCounts.syncs); }
        if (rCounts.triggers != 0) { out.m_taskTriggers.emplace(taskInt, rCounts.triggers); }
    }

    // Push

    for (TaskEdges const* pEdges : data)
    {
        // taskCounts repurposed as items remaining

        for (auto const [task, pipeline, stage] : pEdges->m_runOn)
        {
            out.m_pipelines[pipeline].m_stages[stage].m_runTasks.push_back(task);

            auto const span     = lgrn::Span<TplPipelineStage>(out.m_taskRunOn[TaskInt(task)]);
            TaskCounts &rCounts = taskCounts[task];
            span[span.size()-rCounts.runOn] = { pipeline, stage };
            rCounts.runOn --;
        }
    }

    for (TaskEdges const* pEdges : data)
    {
        for (auto const [task, pipeline, stage] : pEdges->m_syncWith)
        {
            auto const span     = lgrn::Span<TplPipelineStage>(out.m_taskSync[TaskInt(task)]);
            TaskCounts &rCounts = taskCounts[task];

            span[span.size()-rCounts.syncs] = { pipeline, stage };
            rCounts.syncs --;

            for (auto const [runPipeline, runStage] : out.m_taskRunOn[TaskInt(task)])
            {
                out.m_pipelines[pipeline].m_stages[stage_next(stage, plCounts[pipeline].stages)].m_enterReq.push_back({task, runPipeline, runStage});
            }
        }

        for (auto const [task, pipeline, stage] : pEdges->m_triggers)
        {
            auto const span     = lgrn::Span<TplPipelineStage>(out.m_taskTriggers[TaskInt(task)]);
            TaskCounts &rCounts = taskCounts[task];

            span[span.size()-rCounts.triggers] = { pipeline, stage };
            rCounts.triggers --;

            out.m_pipelines[pipeline].m_stages[stage].m_enterReq.push_back({task, pipeline, stage});

            for (auto const [runPipeline, runStage] : out.m_taskRunOn[TaskInt(task)])
            {
                out.m_pipelines[pipeline].m_stages[stage].m_enterReq.push_back({task, runPipeline, runStage});
            }
        }
    }

    [[maybe_unused]] auto const all_counts_zero = [] (TaskCounts const counts)
    {
        return counts.syncs == 0 && counts.triggers == 0;
    };

    LGRN_ASSERTM(std::all_of(taskCounts.begin(), taskCounts.end(), all_counts_zero),
                 "Counts repurposed as items remaining, and must all be zero by the end here");

    return out;
}

} // namespace osp

