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
#include "top_session.h"
#include "top_execute.h"
#include "execute_simple.h"
#include "tasks.h"

#include <Corrade/Containers/ArrayViewStl.h>

#include <longeron/containers/bit_view.hpp>

using Corrade::Containers::ArrayView;
using Corrade::Containers::StridedArrayView2D;
using Corrade::Containers::arrayView;

namespace osp
{

void top_close_session(Tags& rTags, Tasks& rTasks, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecutionContext& rExec, ArrayView<Session> sessions)
{
    // Run cleanup tasks
    {
        // Accumolate together all cleanup tags from all sessons
        std::vector<bit_int_t> tagsToRun(rTags.m_tags.vec().size());
        auto tagsToRunBits = lgrn::bit_view(tagsToRun);
        for (Session &rSession : sessions)
        {
            if (TagId const tgCleanupEvt = std::exchange(rSession.m_tgCleanupEvt, lgrn::id_null<TagId>());
                tgCleanupEvt != lgrn::id_null<TagId>())
            {
                tagsToRunBits.set(std::size_t(tgCleanupEvt));
            }
        }

        task_enqueue(rTags, rTasks, rExec, tagsToRun);
        top_run_blocking(rTags, rTasks, rTaskData, topData, rExec);
    }

    // Clear each session's TopData
    for (Session &rSession : sessions)
    {
        for (TopDataId const id : std::exchange(rSession.m_dataIds, {}))
        {
            if (id != lgrn::id_null<TopDataId>())
            {
                topData[std::size_t(id)].reset();
            }
        }
    }

    // Clear each session's tasks
    for (Session &rSession : sessions)
    {
        for (TaskId const task : rSession.m_taskIds)
        {
            rTasks.m_tasks.remove(TaskId(task));
            TopTask &rCurrTaskData = rTaskData.m_taskData[std::size_t(task)];
            rCurrTaskData.m_dataUsed.clear();
            rCurrTaskData.m_func = nullptr;
        }
    }

    // Clear each session's tags
    for (Session &rSession : sessions)
    {
        for (TagId const tag : std::exchange(rSession.m_tagIds, {}))
        {
            if (tag != lgrn::id_null<TagId>())
            {
                rTags.m_tags.remove(tag);
            }
        }
    }
}

} // namespace testapp
