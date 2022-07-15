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
#pragma once

#include "tasks.h"
#include "worker.h"

#include <vector>

namespace osp
{

struct MainTask
{
    std::vector<MainDataId> m_dataUsed;
    MainTaskFunc_t m_func;
};

inline void task_data(TaskDataVec<MainTask> &rData, TaskTags::Task const task, std::initializer_list<MainDataId> dataUsed, MainTaskFunc_t func)
{
    rData.m_taskData.resize(
            std::max(rData.m_taskData.size(), std::size_t(task) + 1));
    auto &rMainTask = rData.m_taskData[std::size_t(task)];
    rMainTask.m_dataUsed = dataUsed;
    rMainTask.m_func = func;
}

using MainData_t = std::vector<entt::any>;

using MainTaskDataVec_t = TaskDataVec<MainTask>;

} // namespace osp
