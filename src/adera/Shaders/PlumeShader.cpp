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
#include "PlumeShader.h"

#include <Magnum/GL/Version.h>
#include <Magnum/GL/Shader.h>
#include <Corrade/Containers/Reference.h>
#include <Magnum/GL/Texture.h>

using namespace Magnum;
using namespace osp::active;
using namespace adera::shader;

void PlumeShader::draw_plume(ActiveEnt e, ActiveScene& rScene, GL::Mesh& rMesh,
    ACompCamera const& camera, ACompTransform const& transform)
{
    auto& shaderInstance = rScene.reg_get<ACompPlumeShaderInstance>(e);
    PlumeShader& shader = *shaderInstance.m_shaderProgram;

    Magnum::Matrix4 entRelative = camera.m_inverse * transform.m_transformWorld;

    shader
        .bindNozzleNoiseTexture(*shaderInstance.m_nozzleTex)
        .bindCombustionNoiseTexture(*shaderInstance.m_combustionTex)
        .setMeshZBounds(shaderInstance.m_maxZ, shaderInstance.m_minZ)
        .setBaseColor(shaderInstance.m_color)
        .setFlowVelocity(shaderInstance.m_flowVelocity)
        .updateTime(shaderInstance.m_currentTime)
        .setPower(shaderInstance.m_powerLevel)
        .setTransformationMatrix(entRelative)
        .setProjectionMatrix(camera.m_projection)
        .setNormalMatrix(entRelative.normalMatrix())
        .draw(rMesh);
}

void PlumeShader::init()
{
    GL::Shader vert{GL::Version::GL430, GL::Shader::Type::Vertex};
    GL::Shader frag{GL::Version::GL430, GL::Shader::Type::Fragment};
    vert.addFile("OSPData/adera/Shaders/PlumeShader.vert");
    frag.addFile("OSPData/adera/Shaders/PlumeShader.frag");

    CORRADE_INTERNAL_ASSERT_OUTPUT(GL::Shader::compile({vert, frag}));
    attachShaders({vert, frag});
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    // Set TexSampler2D uniforms
    setUniform(
        static_cast<Int>(UniformPos::NozzleNoiseTex),
        static_cast<Int>(TextureSlot::NozzleNoiseTexUnit));
    setUniform(
        static_cast<Int>(UniformPos::CombustionNoiseTex),
        static_cast<Int>(TextureSlot::CombustionNoiseTexUnit));
}

PlumeShader::PlumeShader()
{
    init();
}

PlumeShader& PlumeShader::setProjectionMatrix(Matrix4 const& matrix)
{
    setUniform(static_cast<Int>(UniformPos::ProjMat), matrix);
    return *this;
}

PlumeShader& PlumeShader::setTransformationMatrix(Matrix4 const& matrix)
{
    setUniform(static_cast<Int>(UniformPos::ModelTransformMat), matrix);
    return *this;
}

PlumeShader& PlumeShader::setNormalMatrix(Matrix3x3 const& matrix)
{
    setUniform(static_cast<Int>(UniformPos::NormalMat), matrix);
    return *this;
}

PlumeShader& PlumeShader::setMeshZBounds(float topZ, float bottomZ)
{
    setUniform(static_cast<Int>(UniformPos::MeshTopZ), topZ);
    setUniform(static_cast<Int>(UniformPos::MeshBottomZ), bottomZ);
    return *this;
}

PlumeShader& PlumeShader::bindNozzleNoiseTexture(Magnum::GL::Texture2D& tex)
{
    tex.bind(static_cast<Int>(TextureSlot::NozzleNoiseTexUnit));
    return *this;
}

PlumeShader& PlumeShader::bindCombustionNoiseTexture(Magnum::GL::Texture2D& tex)
{
    tex.bind(static_cast<Int>(TextureSlot::CombustionNoiseTexUnit));
    return *this;
}

PlumeShader& PlumeShader::setBaseColor(const Magnum::Color4 color)
{
    setUniform(static_cast<Int>(UniformPos::BaseColor), color);
    return *this;
}

PlumeShader& PlumeShader::setFlowVelocity(const float vel)
{
    setUniform(static_cast<Int>(UniformPos::FlowVelocity), vel);
    return *this;
}

PlumeShader& PlumeShader::updateTime(const float currentTime)
{
    setUniform(static_cast<Int>(UniformPos::Time), currentTime);
    return *this;
}

PlumeShader& PlumeShader::setPower(const float power)
{
    setUniform(static_cast<Int>(UniformPos::Power), power);
    return *this;
}
