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

#include "../logging.h"

#include <Corrade/Containers/ArrayViewStl.h>

#include <entt/core/any.hpp>

#include <algorithm>
#include <sstream>
#include <vector>

namespace osp
{

void top_run_blocking(Tasks const& tasks, ExecGraph const& graph, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecContext& rExec, WorkerContext worker)
{
    std::vector<entt::any> topDataRefs;

    // Run until there's no tasks left to run
    while (true)
    {
        if (rExec.m_tasksQueued.empty())
        {
            break;
        }

        TaskId const task = rExec.m_tasksQueued.at(0);

        TopTask &rTopTask = rTaskData[task];

        std::cout << "running: " << rTopTask.m_debugName << "\n";

        topDataRefs.clear();
        topDataRefs.reserve(rTopTask.m_dataUsed.size());
        for (TopDataId const dataId : rTopTask.m_dataUsed)
        {
            topDataRefs.push_back((dataId != lgrn::id_null<TopDataId>())
                                   ? topData[dataId].as_ref()
                                   : entt::any{});
        }

//        if (rTopTask.m_awareOfDirtyDeps)
//        {
//            worker.m_dependOnDirty.reset();

//            std::size_t index = 0;
//            for (TargetId const dependOn : graph.m_taskDependOn[std::size_t(task)])
//            {
//                if (rExec.m_targetDirty.test(std::size_t(dependOn)))
//                {
//                    worker.m_dependOnDirty.set(index);
//                }
//                ++index;
//            }
//        }

        // Task actually runs here. Results are not yet used for anything.
        FulfillDirty_t const status = rTopTask.m_func(worker, topDataRefs);

        mark_completed_task(tasks, graph, rExec, task, status);

        enqueue_dirty(tasks, graph, rExec);
        for (TaskId const task : rExec.m_tasksQueued)
        {
            std::cout << "enq: " << rTaskData[task].m_debugName << "\n";
        }
    }
}

void top_enqueue_quick(Tasks const& tasks, ExecGraph const& graph, ExecContext& rExec, ArrayView<TargetId const> enqueue)
{
    for (TargetId const target : enqueue)
    {
        rExec.m_targetDirty.set(std::size_t(target));
    }

    enqueue_dirty(tasks, graph, rExec);
}



void write_dot_graph(std::ostream& rStream, Tasks const& tasks, ExecGraph const& graph, TopTaskDataVec_t& rTaskData)
{
    rStream << "graph {\n"
            << "rankdir=LR;\n"
            << "nodesep=0.4;\n"
            << "splines=line;\n";

    // Define targets
    rStream << "node [shape=circle,fixedsize=true]; ";
    for (std::size_t const target : tasks.m_targetIds.bitview().zeros())
    {
        rStream << target << ";";
    }

    rStream << "node [shape=rectangle,fixedsize=false];\n";

    for (std::size_t const task : tasks.m_taskIds.bitview().zeros())
    {
        rStream << "task_" << task << " [label=\"" << rTaskData.at(TaskId(task)).m_debugName << "\"];\n";

        for (DependOn const dependOn : graph.m_taskDependOn[task])
        {
            rStream << std::size_t(dependOn.m_target) << " -- task_" << task << " "
                    << (dependOn.m_trigger ? "[color=darkblue,penwidth=\"2px\", weight=3,]" : "[color=darkblue,style=dashed]")
                    << ";\n";
        }

        for (TargetId const fulfill : graph.m_taskFulfill[task])
        {
            rStream << "task_" << task << " -- " << std::size_t(fulfill) << " [color=darkred];\n";
        }
    }

    rStream << "}";
}

} // namespace testapp
