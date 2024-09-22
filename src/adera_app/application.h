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

#include <osp/framework/framework.h>

#include <entt/core/any.hpp>

#include <vector>

namespace adera
{

struct MainLoopControl
{
    bool doUpdate;
};

struct SceneLoopControl
{
    bool doSceneUpdate;
};

struct WindowAppLoopControl
{
    bool doResync;
    bool doSync;
    bool doRender;
};

struct AppContexts
{
    osp::fw::ContextId main;
    osp::fw::ContextId window;
    osp::fw::ContextId sceneRender;
    osp::fw::ContextId universe;
    osp::fw::ContextId scene;
};

struct FrameworkModify
{
    struct Command
    {
        using Func_t = void(*)(osp::fw::Framework &rFW, osp::fw::ContextId ctx, entt::any userData);
        entt::any           userData;
        osp::fw::ContextId  ctx;
        Func_t              func;
    };

    std::vector<Command> commands;
};



} // namespace adera
