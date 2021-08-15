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

#include <osp/Active/activetypes.h>
#include <osp/Active/drawing.h>

#include <Magnum/Shaders/MeshVisualizerGL.h>

namespace osp::shader
{

class MeshVisualizer : protected Magnum::Shaders::MeshVisualizerGL3D
{
    using RenderGroup = osp::active::RenderGroup;

public:

    using Magnum::Shaders::MeshVisualizerGL3D::MeshVisualizerGL3D;
    using Magnum::Shaders::MeshVisualizerGL3D::Flag;

    static void draw_entity(
            osp::active::ActiveEnt ent, osp::active::ActiveScene& rScene,
            osp::active::ACompCamera const& camera, void* pUserData) noexcept;

    static RenderGroup::DrawAssigner_t gen_assign_phong_opaque(
            MeshVisualizer* pShader);
};

} // namespace osp::shader
