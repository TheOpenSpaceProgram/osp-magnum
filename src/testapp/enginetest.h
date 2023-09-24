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

#include "../MagnumApplication.h"

#include <osp/drawing_gl/rendergl.h>

namespace testapp::enginetest
{

struct EngineTestScene;

/**
 * @brief Setup Engine Test Scene
 *
 * @param rResources    [ref] Application Resources containing cube mesh
 * @param pkg           [in] Package Id the cube mesh is under
 *
 * @return entt::any containing scene data
 */
entt::any setup_scene(osp::Resources& rResources, osp::PkgId pkg);

/**
 * @brief Generate MagnumApplication draw function
 *
 * This draw function stores renderer data, and is responsible for updating
 * and drawing the engine test scene.
 *
 * @param rScene [ref] Engine test scene. Must be in stable memory.
 * @param rApp   [ref] Existing MagnumApplication to use GL resources of
 *
 * @return MagnumApplication draw function
 */
MagnumApplication::AppPtr_t generate_draw_func(EngineTestScene& rScene, MagnumApplication& rApp, osp::draw::RenderGL& rRenderGl, osp::input::UserInputHandler& rUserInput);


} // namespace testapp::enginetest
