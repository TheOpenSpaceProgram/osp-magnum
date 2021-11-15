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

#include <osp/Resource/Package.h>

#include <functional>

namespace testapp
{

struct ActiveApplication;

using on_draw_t = std::function<void(ActiveApplication&)>;



namespace flight
{

struct FlightScene;

/**
 * @brief Setup a flight scene
 *
 * @return
 */
entt::any setup_scene();

/**
 * @brief Generate ActiveApplication draw function
 *
 * OpenGL context resources as
 *
 * @param rScene [in] Flight scene data
 *
 * @return
 */
on_draw_t gen_draw(FlightScene& rScene, ActiveApplication& rApp);

} // namespace flight



namespace enginetest
{

struct EngineTestScene;


entt::any setup_scene(osp::Package &rPkg);

on_draw_t gen_draw(EngineTestScene& rScene, ActiveApplication& rApp);

void load_gl_resources(ActiveApplication& rApp);

} // namespace enginetest

} // namespace testapp
