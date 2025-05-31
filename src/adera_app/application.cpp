/**
 * Open Space Program
 * Copyright © 2019-2025 Open Space Program Project
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
#include "application.h"
#include "feature_interfaces.h"

#include <osp/util/logging.h>

namespace adera
{

void run_cleanup(osp::fw::ContextId ctx, osp::fw::Framework &rFW, osp::fw::IExecutor &rExec)
{
    auto const cleanup = rFW.get_interface<ftr_inter::FICleanupContext>(ctx);
    if ( cleanup.id.has_value() )
    {
        // Run cleanup pipeline for the window context
        rExec.task_finish(rFW, cleanup.tasks.blockSchedule);
        rExec.wait(rFW);

        if (rExec.is_running(rFW, cleanup.loopblks.cleanup))
        {
            OSP_LOG_CRITICAL("Deadlock in cleanup pipeline");
            std::abort();
        }
    }
}


} // namespace adera

