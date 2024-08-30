/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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
#include "executor.h"

#include <spdlog/fmt/ostr.h>

#include <algorithm>
#include <iomanip>
#include <vector>

namespace osp::fw
{


void SingleThreadedExecutor::load(Framework& rFW)
{
    m_graph = osp::make_exec_graph(rFW.m_tasks);
    m_execContext = {};
    exec_conform(rFW.m_tasks, m_execContext);
    m_execContext.doLogging = m_log != nullptr;
}

void SingleThreadedExecutor::run(Framework& rFW, PipelineId pipeline)
{
    exec_request_run(m_execContext, pipeline);
}

void SingleThreadedExecutor::signal(Framework& rFW, PipelineId pipeline)
{
    exec_signal(m_execContext, pipeline);
}

void SingleThreadedExecutor::wait(Framework& rFW)
{
    if (m_log != nullptr)
    {
        m_log->info("\n>>>>>>>>>> Previous State Changes\n{}\n>>>>>>>>>> Current State\n{}\n",
                    WriteLog  {rFW.m_tasks, rFW.m_taskImpl, m_graph, m_execContext},
                    WriteState{rFW.m_tasks, rFW.m_taskImpl, m_graph, m_execContext} );
        m_execContext.logMsg.clear();
    }

    exec_update(rFW.m_tasks, m_graph, m_execContext);
    run_blocking(rFW.m_tasks, m_graph, rFW.m_taskImpl, rFW.m_data, m_execContext);

    if (m_log != nullptr)
    {
        m_log->info("\n>>>>>>>>>> New State Changes\n{}",
                    WriteLog{rFW.m_tasks, rFW.m_taskImpl, m_graph, m_execContext} );
        m_execContext.logMsg.clear();
    }
}

bool SingleThreadedExecutor::is_running(Framework const& rFW)
{
    return m_execContext.hasRequestRun || (m_execContext.pipelinesRunning != 0);
}


void SingleThreadedExecutor::run_blocking(
        Tasks                     const &tasks,
        TaskGraph                 const &graph,
        KeyedVec<TaskId, TaskImpl>      &rTaskImpl,
        KeyedVec<DataId, entt::any>     &fwData,
        ExecContext                     &rExec,
        WorkerContext                   worker)
{
    std::vector<entt::any> argumentRefs;

    while ( ! rExec.tasksQueuedRun.empty() )
    {
        TaskId   const willRunId     = rExec.tasksQueuedRun[0];
        TaskImpl       &rWillRunImpl = rTaskImpl[willRunId];

        if (rWillRunImpl.func != nullptr)
        {
            argumentRefs.clear();
            argumentRefs.reserve(rWillRunImpl.args.size());
            for (DataId const dataId : rWillRunImpl.args)
            {
                argumentRefs.push_back(dataId.has_value() ? fwData[dataId].as_ref() : entt::any{});
            }

            TaskActions const status = rWillRunImpl.func(worker, argumentRefs);
            complete_task(tasks, graph, rExec, willRunId, status);
        }
        else
        {
            // Allow tasks to not have a function.
            complete_task(tasks, graph, rExec, willRunId, {});
        }

        exec_update(tasks, graph, rExec);
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

        if (reqPlData.stage != req.reqStage)
        {
            rStream << "* Requires PL" << std::setw(3) << PipelineInt(req.reqPipeline) << " stage " << stageNames[std::size_t(req.reqStage)] << "\n";
        }
    }
}


std::ostream& operator<<(std::ostream& rStream, SingleThreadedExecutor::WriteState const& write)
{
    auto const& [tasks, taskData, graph, exec] = write;

    static constexpr std::size_t nameMinColumns = 50;
    static constexpr std::size_t maxDepth = 4;

    rStream << "Pipeline/Tree  | Status  |  Stages                                     |  Pipeline Names\n"
            << "_________________________________________________________________________________________\n";

    auto const write_pipeline = [&rStream, &tasks=tasks, &exec=exec, &graph=graph] (PipelineId const pipeline, std::size_t const depth)
    {
        ExecPipeline const  &plExec = exec.plData[pipeline];

        for (std::size_t i = 0; i < depth; ++i)
        {
            rStream << "- ";
        }

        rStream << "PL" << std::setw(3) << std::left << PipelineInt(pipeline) << " ";

        for (int i = 0; i < (maxDepth - depth); ++i)
        {
            rStream << "  ";
        }

        rStream << " | ";

        ExecPipeline const &execPl = exec.plData[pipeline];

        bool const signalBlocked =    (execPl.waitStage != lgrn::id_null<StageId>())
                                   && (execPl.waitStage == execPl.stage)
                                   && ( ! execPl.waitSignaled );

        rStream << (execPl.running                 ? 'R' : '-')
                << (execPl.loop                    ? 'L' : '-')
                << (execPl.loopChildrenLeft   != 0 ? 'O' : '-')
                << (execPl.canceled                ? 'C' : '-')
                << (signalBlocked                  ? 'S' : '-')
                << (execPl.tasksQueuedRun     != 0 ? 'Q' : '-')
                << (execPl.tasksQueuedBlocked != 0 ? 'B' : '-');

        rStream << " | ";

        uint32_t const stageCount = fanout_size(graph.pipelineToFirstAnystg, pipeline);

        PipelineInfo const& info = tasks.m_pipelineInfo[pipeline];

        std::size_t charsUsed = 7; // "PL###" + ": "

        if (info.stageType != lgrn::id_null<PipelineInfo::StageTypeId>())
        {
            auto const stageNames = ArrayView<std::string_view const>{PipelineInfo::sm_stageNames[info.stageType]};

            for (uint32_t stage = 0; stage < std::min(uint32_t(stageNames.size()), stageCount); ++stage)
            {
                bool const sel = uint32_t(plExec.stage) == stage;
                rStream << (sel ? '[' : ' ')
                        << stageNames[stage]
                        << (sel ? ']' : ' ');

                charsUsed += 2 + stageNames[stage].size();
            }
        }

        for (; charsUsed < nameMinColumns; ++charsUsed)
        {
            rStream << ' ';
        }

        rStream << " | " << (info.name.empty() ? "untitled or deleted" : info.name);

        rStream << "\n";
    };

    auto const traverse = [&write_pipeline, &rStream, &graph=graph] (auto const& self, PipelineTreePos_t first, PipelineTreePos_t last, int depth) -> void
    {
        uint32_t descendants = 0;
        for (PipelineTreePos_t pos = first; pos != last; pos += 1 + descendants)
        {
            descendants = graph.pltreeDescendantCounts[pos];

            write_pipeline(graph.pltreeToPipeline[pos], depth);

            self(self, pos + 1, pos + 1 + descendants, depth + 1);
        }
    };

    traverse(traverse, 0, graph.pltreeToPipeline.size(), 0);

    // Write pipelines that are not in the tree
    for (PipelineId const pipeline : tasks.m_pipelineIds)
    {
        if (graph.pipelineToPltree[pipeline] == lgrn::id_null<PipelineTreePos_t>())
        {
            write_pipeline(pipeline, 0);
        }
    }

    rStream << "*Status: [R: Running]  [L: Looping] [O: Looping Children] [C: Canceled] [S: Signal Blocked] [Q: Has Queued Tasks To Run] [B: Queued Tasks Blocked]\n";


    for (auto const [task, block] : exec.tasksQueuedBlocked.each())
    {
        rStream << "Task Blocked: " << "TASK" << TaskInt(task) << " - " << taskData[task].debugName << "\n";

        write_task_requirements(rStream, tasks, graph, exec, task);
    }

    return rStream;
}

std::ostream& operator<<(std::ostream& rStream, SingleThreadedExecutor::WriteLog const& write)
{
    auto const& [tasks, rTaskImpl, graph, exec] = write;

    auto const stage_name = [&tasks=tasks] (PipelineId pl, StageId stg) -> std::string_view
    {
        if (stg != lgrn::id_null<StageId>())
        {
            PipelineInfo const& info        = tasks.m_pipelineInfo[pl];
            auto const          stageNames  = ArrayView<std::string_view const>{PipelineInfo::sm_stageNames[info.stageType]};
            return stageNames[std::size_t(stg)];
        }
        else
        {
            return "NULL";
        }
    };

    auto const visitMsg = [&rStream, &tasks=tasks, &rTaskImpl=rTaskImpl, &graph=graph, &stage_name] (auto&& msg)
    {
        using MSG_T = std::decay_t<decltype(msg)>;
        if constexpr (std::is_same_v<MSG_T, ExecContext::UpdateStart>)
        {
            rStream << "UpdateStart\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::UpdateCycle>)
        {
            rStream << "UpdateCycle\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::UpdateEnd>)
        {
            rStream << "UpdateEnd\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::PipelineRun>)
        {
            rStream << "    PipelineRun PL" << std::setw(3) << std::left << PipelineInt(msg.pipeline) << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::PipelineFinish>)
        {
            rStream << "    PipelineFinish PL" << std::setw(3) << std::left << PipelineInt(msg.pipeline) << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::PipelineCancel>)
        {
            rStream << "    PipelineCancel PL" << std::setw(3) << std::left << PipelineInt(msg.pipeline) << "("
                    << stage_name(msg.pipeline, msg.stage) << ")\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::PipelineLoop>)
        {
            rStream << "    PipelineLoop PL" << std::setw(3) << std::left << PipelineInt(msg.pipeline) << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::PipelineLoopFinish>)
        {
            rStream << "    PipelineLoopFinish PL" << std::setw(3) << std::left << PipelineInt(msg.pipeline) << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::StageChange>)
        {
            rStream << "    StageChange PL" << std::setw(3) << std::left << PipelineInt(msg.pipeline)
                    << "(" << stage_name(msg.pipeline, msg.stageOld) << " -> " << stage_name(msg.pipeline, msg.stageNew) << ")\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::EnqueueTask>)
        {
            rStream << "    Enqueue " << (msg.blocked ? "Blocked" : "Run")
                    << " on PL" << std::setw(3) << std::left << PipelineInt(msg.pipeline)
                    << "(" << stage_name(msg.pipeline, msg.stage) << ")"
                    << " TASK" << TaskInt(msg.task) << " - " << rTaskImpl[msg.task].debugName << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::EnqueueTaskReq>)
        {
            rStream << "    * " << (msg.satisfied ? "[DONE]" : "[wait]") << "Require PL"
                    << std::setw(3) << std::left << PipelineInt(msg.pipeline)
                    << "(" << stage_name(msg.pipeline, msg.stage) << ")\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::UnblockTask>)
        {
            rStream << "    * Unblock TASK" << TaskInt(msg.task) << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::CompleteTask>)
        {
            rStream << "Complete TASK" << TaskInt(msg.task) << " - " << rTaskImpl[msg.task].debugName << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::ExternalRunRequest>)
        {
            rStream << "ExternalRunRequest PL" << std::setw(3) << std::left << PipelineInt(msg.pipeline) << "\n";
        }
        else if constexpr (std::is_same_v<MSG_T, ExecContext::ExternalRunRequest>)
        {
            rStream << "ExternalSignal PL" << std::setw(3) << std::left << PipelineInt(msg.pipeline) << (msg.ignored ? " IGNORED!" : " ") << "\n";
        }
    };

    for (ExecContext::LogMsg_t const& msg : exec.logMsg)
    {
        std::visit(visitMsg, msg);
    }

    return rStream;
}


} // namespace adera
