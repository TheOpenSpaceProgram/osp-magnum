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

#include <osp/framework/builder.h>

namespace testapp
{

extern osp::fw::FeatureDef const ftrMagnum;

/**
 * @brief stuff needed to render a scene using Magnum
 */
extern osp::fw::FeatureDef const ftrMagnumScene;

/**
 * @brief Create CameraController connected to an app's UserInputHandler
 */
extern osp::fw::FeatureDef const ftrCameraControl;

/**
 * @brief Magnum MeshVisualizer shader and optional material for drawing ActiveEnts with it
 */
extern osp::fw::FeatureDef const ftrShaderVisualizer;

/**
 * @brief Magnum Flat shader and optional material for drawing ActiveEnts with it
 */
extern osp::fw::FeatureDef const ftrShaderFlat;

/**
 * @brief Magnum Phong shader and optional material for drawing ActiveEnts with it
 */
extern osp::fw::FeatureDef const ftrShaderPhong;

extern osp::fw::FeatureDef const ftrTerrainDrawMagnum;

} // namespace testapp
