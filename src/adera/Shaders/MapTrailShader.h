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

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Attribute.h>

#include <osp/Active/Shader.h>
#include <osp/Resource/Resource.h>
#include <osp/Active/activetypes.h>

namespace adera::shader
{

class MapTrailShader : Magnum::GL::AbstractShaderProgram
{
public:
    typedef Magnum::GL::Attribute<0, Magnum::Vector4> Position;
    typedef Magnum::GL::Attribute<1, Magnum::Color4> Color;

    MapTrailShader() { init(); }

    struct ACompMapTrailShaderInstance
    {
        // Parent shader
        osp::DependRes<MapTrailShader> m_shaderProgram;

        ACompMapTrailShaderInstance(
            osp::DependRes<MapTrailShader> parent) : m_shaderProgram(parent)
        {}
    };

    static void draw_trails(osp::active::ActiveEnt e,
        osp::active::ActiveScene& rScene,
        Magnum::GL::Mesh& rMesh,
        osp::active::ACompCamera const& camera,
        osp::active::ACompTransform const& transform);

public:
    void init();

    enum class EUniformPos : Magnum::Int
    {
        TransformationMatrix = 0
    };

    // Hide calls
    using Magnum::GL::AbstractShaderProgram::draw;
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    MapTrailShader& set_transform_matrix(Magnum::Matrix4 const& transformation);
};

} // namespace adera::shader
