/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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

#include "activetypes.h"
#include "osp/Active/ActiveScene.h"
#include <Magnum/GL/Mesh.h>

namespace osp::active
{
/**
 * A function pointer to a Shader's draw() function
 * @param ActiveEnt - The entity being drawn; used to fetch component data
 * @param ActiveScene - The scene containing the entity's component data
 * @param Mesh - Mesh data to be drawn with the shader
 * @param ACompCamera - Camera used to draw the scene
 * @param ACompTransform - Object transformation data
 */
using ShaderDrawFnc_t = void (*)(
    ActiveEnt,
    ActiveScene&,
    Magnum::GL::Mesh&,
    ACompCamera const&,
    ACompTransform const&);
}