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

#include <entt/core/any.hpp>

#include <osp/keyed_vector.h>

#include <osp/Resource/resourcetypes.h>

#include <osp/tasks/tasks.h>
#include <osp/tasks/top_session.h>

#include <optional>

namespace testapp
{

struct TestApp;

using RendererSetupFunc_t   = void(*)(TestApp&);

using SceneSetupFunc_t      = RendererSetupFunc_t(*)(TestApp&);



struct TestAppTasks
{
    std::vector<entt::any>          m_topData;
    osp::Tasks                      m_tasks;
    osp::TopTaskDataVec_t           m_taskData;
    osp::TaskGraph                  m_graph;
};

class IExecutor
{
    virtual void load(TestAppTasks const* pTasks) = 0;
    virtual void run(osp::PipelineId pipeline) = 0;
    virtual void signal(osp::PipelineId pipeline) = 0;
    virtual void wait() = 0;
    virtual bool is_done() = 0;
};

struct TestApp : TestAppTasks
{
    osp::Session                    m_application;

    osp::SessionGroup               m_scene;

    osp::Session                    m_windowApp;
    osp::Session                    m_magnum;
    osp::SessionGroup               m_renderer;

    IExecutor*                      m_executor{ nullptr };

    RendererSetupFunc_t             m_rendererSetup { nullptr };

    //osp::TopDataId                  m_idResources   { lgrn::id_null<osp::TopDataId>() };
    osp::PkgId                      m_defaultPkg    { lgrn::id_null<osp::PkgId>() };
};

void close_sessions(TestAppTasks &rTestApp, osp::SessionGroup &rSessions);

void close_session(TestAppTasks &rTestApp, osp::Session &rSession);

/**
 * @brief Deal with resource reference counts for a clean termination
 */
void clear_resource_owners(TestApp& rTestApp);

} // namespace testapp
