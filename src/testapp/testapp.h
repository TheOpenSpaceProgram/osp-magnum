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
#pragma once

#include <osp/core/keyed_vector.h>
#include <osp/core/resourcetypes.h>
#include <osp/framework/builder.h>
#include <osp/framework/framework.h>
#include <osp/util/logging.h>

#include <entt/core/any.hpp>

#include <vector>

namespace testapp
{

struct MainLoopArgs
{
    osp::fw::Framework &rFW;
    osp::fw::IExecutor &rExecutor;
    osp::fw::ContextId mainCtx;
};

using MainLoopFunc_t = std::function<bool(MainLoopArgs)>;

inline std::vector<MainLoopFunc_t>& main_loop_stack()
{
    static std::vector<MainLoopFunc_t> instance;
    return instance;
}

void run_cleanup(osp::fw::ContextId ctx, osp::fw::Framework &rFW, osp::fw::IExecutor &rExec);

//struct TestApp
//{
//    struct UpdateParams {
//        float deltaTimeIn;
//        bool update;
//        bool sceneUpdate;
//        bool resync;
//        bool sync;
//        bool render;
//    };

//    /**
//     * @brief Deal with resource reference counts for a clean termination
//     */
//    void clear_resource_owners();

//    void drive_default_main_loop();

//    void drive_scene_cycle(UpdateParams p);

//    void run_context_cleanup(osp::fw::ContextId);

//    void init();



//private:


//};

} // namespace testapp
