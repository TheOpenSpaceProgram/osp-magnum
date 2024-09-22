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
#pragma once

#include "framework.h"

#include "../tasks/execute.h"

#include <spdlog/logger.h>

namespace osp::fw
{


class SingleThreadedExecutor final : public IExecutor
{
public:

    struct WriteState
    {
        Tasks                         const &tasks;
        KeyedVec<TaskId, TaskImpl>          &rTaskImpl;
        TaskGraph                     const &graph;
        ExecContext                   const &exec;

        friend std::ostream& operator<<(std::ostream& rStream, WriteState const& write);
    };

    struct WriteLog
    {
        Tasks                         const &tasks;
        KeyedVec<TaskId, TaskImpl>    const &rTaskImpl;
        TaskGraph                     const &graph;
        ExecContext                   const &exec;

        friend std::ostream& operator<<(std::ostream& rStream, WriteLog const& write);
    };

    void load(Framework& rFW) override;

    void run(Framework& rFW, PipelineId pipeline) override;

    void signal(Framework& rFW, PipelineId pipeline) override;

    void wait(Framework& rFW) override;

    bool is_running(Framework const& rFW) override;

    std::shared_ptr<spdlog::logger> m_log;

private:

    static void run_blocking(
        Tasks                     const &tasks,
        TaskGraph                 const &graph,
        KeyedVec<TaskId, TaskImpl>      &rTaskImpl,
        KeyedVec<DataId, entt::any>     &fwData,
        ExecContext                     &rExec,
        WorkerContext                   worker = {} );


    ExecContext                     m_execContext;
    TaskGraph                       m_graph;


};




} // namespace adera
