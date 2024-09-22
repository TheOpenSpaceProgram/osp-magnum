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

using MainLoopFunc_t = std::function<bool()>;

inline std::vector<MainLoopFunc_t>& main_loop_stack()
{
    static std::vector<MainLoopFunc_t> instance;
    return instance;
}

struct TestApp
{
    struct UpdateParams {
        float deltaTimeIn;
        bool update;
        bool sceneUpdate;
        bool resync;
        bool sync;
        bool render;
    };

    /**
     * @brief Deal with resource reference counts for a clean termination
     */
    void clear_resource_owners();

    void drive_default_main_loop();

    void drive_scene_cycle(UpdateParams p);

    void run_context_cleanup(osp::fw::ContextId);

    void init();

    osp::fw::Framework m_framework;
    osp::fw::ContextId m_mainContext;
    osp::fw::IExecutor *m_pExecutor  { nullptr };
    osp::PkgId         m_defaultPkg  { lgrn::id_null<osp::PkgId>() };

    int m_argc;
    char** m_argv;

private:

    /**
     * @brief As the name implies
     *
     * prefer not to use names like this outside of testapp
     */
    void load_a_bunch_of_stuff();
};

} // namespace testapp
