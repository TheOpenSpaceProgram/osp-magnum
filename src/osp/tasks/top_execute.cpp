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
#include "top_execute.h"
#include "top_worker.h"
#include "execute.h"

#include <Corrade/Containers/ArrayViewStl.h>

#include <entt/core/any.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

namespace osp
{

void top_run_blocking(Tasks const& tasks, TaskGraph const& graph, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecContext& rExec, WorkerContext worker)
{
    std::vector<entt::any> topDataRefs;

    std::cout << "-- Top Run Blocking called\n";

    top_write_log(std::cout, tasks, rTaskData, graph, rExec);
    rExec.logMsg.clear();

    top_write_pipeline_states(std::cout, tasks, rTaskData, graph, rExec);

    // Run until there's no tasks left to run
    while (true)
    {
        auto const runTasksLeft     = rExec.tasksQueuedRun.size();
        auto const blockedTasksLeft = rExec.tasksQueuedBlocked.size();

        if (runTasksLeft+blockedTasksLeft == 0)
        {
            break;
        }

        if (runTasksLeft != 0)
        {
            TaskId const task = rExec.tasksQueuedRun.at(0);
            TopTask &rTopTask = rTaskData[task];

            topDataRefs.clear();
            topDataRefs.reserve(rTopTask.m_dataUsed.size());
            for (TopDataId const dataId : rTopTask.m_dataUsed)
            {
                topDataRefs.push_back((dataId != lgrn::id_null<TopDataId>())
                                       ? topData[dataId].as_ref()
                                       : entt::any{});
            }

            // Task actually runs here. Results are not yet used for anything.
            TriggerOut_t const status = (rTopTask.m_func != nullptr) ? rTopTask.m_func(worker, topDataRefs) : 0;

            complete_task(tasks, graph, rExec, task, status);
            top_write_log(std::cout, tasks, rTaskData, graph, rExec);
            rExec.logMsg.clear();
        }

        enqueue_dirty(tasks, graph, rExec);
        top_write_log(std::cout, tasks, rTaskData, graph, rExec);
        rExec.logMsg.clear();
    }
}

static void write_task_requirements(std::ostream &rStream, Tasks const& tasks, TaskGraph const& graph, ExecContext const& exec, TaskId const task)
{
    auto const taskreqstageView = ArrayView<const TaskRequiresStage>(fanout_view(graph.taskToFirstTaskreqstg, graph.taskreqstgData, task));
    for (TaskRequiresStage const& req : taskreqstageView)
    {
        ExecPipeline const  &reqPlData  = exec.plData[req.reqPipeline];
        PipelineInfo const& info        = tasks.m_pipelineInfo[req.reqPipeline];
        auto const          stageNames  = ArrayView<std::string_view const>{PipelineInfo::sm_stageNames[info.stageType]};

        if (reqPlData.currentStage != req.reqStage)
        {
            rStream << "* Requires PL" << std::setw(3) << PipelineInt(req.reqPipeline) << " stage " << stageNames[std::size_t(req.reqStage)] << "\n";
        }
    }
}


void top_write_pipeline_states(std::ostream &rStream, Tasks const& tasks, TopTaskDataVec_t const& taskData, TaskGraph const& graph, ExecContext const& exec)
{
    constexpr int nameMinColumns = 48;

    for (PipelineInt plInt : tasks.m_pipelineIds.bitview().zeros())
    {
        auto const          pl      = PipelineId(plInt);
        ExecPipeline const  &plExec = exec.plData[pl];

        rStream << "PL" << std::setw(3) << std::left << plInt << ": ";

        int const stageCount = fanout_size(graph.pipelineToFirstAnystg, pl);

        PipelineInfo const& info = tasks.m_pipelineInfo[pl];

        auto const stageNames = ArrayView<std::string_view const>{PipelineInfo::sm_stageNames[info.stageType]};

        int charsUsed = 7; // "PL###" + ": "

        for (int stage = 0; stage < stageCount; ++stage)
        {
            bool const trg = plExec.triggered.test(stage);
            bool const sel = int(plExec.currentStage) == stage;
            rStream << (sel ? '[' : ' ')
                    << (trg ? '*' : ' ')
                    << stageNames[stage]
                    << (trg ? '*' : ' ')
                    << (sel ? ']' : ' ');

            charsUsed += 4 + stageNames[stage].size();
        }

        for (; charsUsed < nameMinColumns; ++charsUsed)
        {
            rStream << ' ';
        }

        rStream << "- " << info.name;

        rStream << "\n";
    }

    for (auto const [task, block] : exec.tasksQueuedBlocked.each())
    {
        rStream << "Task Blocked: " << "TASK" << TaskInt(task) << " - " << taskData[task].m_debugName << "\n";

        write_task_requirements(rStream, tasks, graph, exec, task);
    }

    //   [*run*] - [ not ] - *trig*
}

void top_write_log(std::ostream &rStream, Tasks const& tasks, TopTaskDataVec_t const& taskData, TaskGraph const& graph, ExecContext const& exec)
{
    auto const stage_name = [&tasks] (PipelineId pl, StageId stg) -> std::string_view
    {
        PipelineInfo const& info        = tasks.m_pipelineInfo[pl];
        auto const          stageNames  = ArrayView<std::string_view const>{PipelineInfo::sm_stageNames[info.stageType]};
        return stageNames[std::size_t(stg)];
    };

    auto const visitMsg = [&rStream, &tasks, &taskData, &graph, &stage_name] (auto&& msg)
    {
        using MSG_T = std::decay_t<decltype(msg)>;
        if constexpr (std::is_same_v<MSG_T, ExecContext::EnqueueStart>)
        {
            rStream << "EnqueueStart\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::EnqueueCycle>)
        {
            rStream << "EnqueueCycle\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::EnqueueEnd>)
        {
            rStream << "EnqueueEnd\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::StageChange>)
        {
            rStream << "    StageChange PL" << std::setw(3) << PipelineInt(msg.pipeline) << "("
                    << stage_name(msg.pipeline, msg.stageOld)
                    << " -> "
                    << stage_name(msg.pipeline, msg.stageNew) << ")\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::EnqueueTask>)
        {
            rStream << "    Enqueue " << (msg.blocked ? "Blocked" : "Run")
                    << " on PL" << std::setw(3) << PipelineInt(msg.pipeline)
                    << "(" << stage_name(msg.pipeline, msg.stage) << ")"
                    << " TASK" << TaskInt(msg.task) << " - " << taskData[msg.task].m_debugName << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::EnqueueTaskReq>)
        {
            rStream << "    * Requires PL" << std::setw(3) << PipelineInt(msg.pipeline) << "(" << stage_name(msg.pipeline, msg.stage) << ")\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::UnblockTask>)
        {
            rStream << "    * Unblock TASK" << TaskInt(msg.task) << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::CompleteTask>)
        {
            rStream << "Complete TASK" << TaskInt(msg.task) << " - " << taskData[msg.task].m_debugName << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::CompleteTaskTrigger>)
        {
            rStream << "* Trigger PL" << std::setw(3) << PipelineInt(msg.pipeline)
                    << "(" << stage_name(msg.pipeline, msg.stage) << ")\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::ExternalTrigger>)
        {
            rStream << "ExternalTrigger PL" << std::setw(3) << PipelineInt(msg.pipeline)
                    << "(" << stage_name(msg.pipeline, msg.stage) << ")\n";
        }
    };

    for (ExecContext::LogMsg_t const& msg : exec.logMsg)
    {
        std::visit(visitMsg, msg);
    }
}


} // namespace testapp
