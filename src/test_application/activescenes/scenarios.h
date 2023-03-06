/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include <osp/Resource/resourcetypes.h>

#include <osp/tasks/tasks.h>
#include <osp/tasks/top_tasks.h>
#include <osp/tasks/top_execute.h>
#include <osp/tasks/top_session.h>
#include <osp/tasks/builder.h>

#include <functional>
#include <string_view>
#include <unordered_map>

namespace osp::active { struct RenderGL; }
namespace osp::input { class UserInputHandler; }

namespace testapp
{

struct MainView
{
    osp::ArrayView<entt::any>   m_topData;
    osp::Tags                   & m_rTags;
    osp::Tasks                  & m_rTasks;
    osp::ExecutionContext       & m_rExec;
    osp::TopTaskDataVec_t       & m_rTaskData;
    osp::TopDataId              m_idResources;
    osp::PkgId                  m_defaultPkg;
};

using Builder_t = osp::TaskBuilder<osp::TopTaskDataVec_t>;

using RendererSetup_t   = void(*)(MainView, osp::Session const&, osp::Sessions_t const&, osp::Sessions_t&);
using SceneSetup_t      = RendererSetup_t(*)(MainView, osp::Sessions_t&);

struct ScenarioOption
{
    std::string_view m_desc;
    SceneSetup_t m_setup;
};

using ScenarioMap_t = std::unordered_map<std::string_view, ScenarioOption>;

ScenarioMap_t const& scenarios();


} // namespace testapp
