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
#include "MapTrailShader.h"

#include <Magnum/GL/Version.h>
#include <Magnum/GL/Shader.h>
#include <Corrade/Containers/Reference.h>

#include <osp/Active/ActiveScene.h>

using namespace adera::shader;
using namespace osp::active;
using namespace osp;
using Magnum::Int;

void MapTrailShader::draw_trails(ActiveEnt e,
    ActiveScene& rScene, Magnum::GL::Mesh& rMesh,
    ACompCamera const & camera, ACompTransform const & transform)
{
    auto& shaderInstance = rScene.reg_get<ACompMapTrailShaderInstance>(e);
    MapTrailShader& shader = *shaderInstance.m_shaderProgram;

    Matrix4 transformation = camera.m_projection * camera.m_inverse;

    shader
        .set_transform_matrix(transformation)
        .draw(rMesh);
}

void MapTrailShader::init()
{
    using namespace Magnum;
    GL::Shader vert{GL::Version::GL430, GL::Shader::Type::Vertex};
    GL::Shader frag{GL::Version::GL430, GL::Shader::Type::Fragment};
    vert.addFile("OSPData/adera/Shaders/MapTrail.vert");
    frag.addFile("OSPData/adera/Shaders/MapTrail.frag");

    CORRADE_INTERNAL_ASSERT_OUTPUT(GL::Shader::compile({vert, frag}));
    attachShaders({vert, frag});
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());
}

MapTrailShader& MapTrailShader::set_transform_matrix(Matrix4 const& transformation)
{
    setUniform(static_cast<Int>(EUniformPos::TransformationMatrix), transformation);
    return *this;
}
