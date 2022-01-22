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
#include "PlumeShader.h"                  // IWYU pragma: associated

#include <adera/Plume.h>                  // for PlumeEffectData
#include <adera/SysExhaustPlume.h>        // for ACompExhaustPlume

#include <osp/Resource/Package.h>         // for Package
#include <osp/Resource/Resource.h>        // for DependRes
#include <osp/Resource/AssetImporter.h>   // for AssetImporter

#include <osp/Active/basic.h>             // for ACompCamera
#include <osp/Active/drawing.h>           // for ACompDrawTransform
#include <osp/Active/SysRender.h>         // for ACompMesh
#include <osp/Active/activetypes.h>       // for basic_sparse_set, ActiveEnt

#include <osp/Resource/PackageRegistry.h> // for PackageRegistry

#include <Magnum/GL/Shader.h>             // for Shader, Shader::Type
#include <Magnum/GL/Texture.h>            // for Texture2D
#include <Magnum/GL/Version.h>            // for Version, Version::GL430
#include <Magnum/GL/Renderer.h>

#include <Magnum/Magnum.h>                // for Int, Color4, Matrix3x3, Matrix4

// Needed to make the following line of code compile, for unknown reasons.
// CORRADE_INTERNAL_ASSERT_OUTPUT(GL::Shader::compile({vert, frag}));
#include <Corrade/Containers/Reference.h> // IWYU pragma: keep
#include "Corrade/Containers/ArrayView.h" // for ArrayView

#include <Corrade/Utility/Assert.h>       // for CORRADE_INTERNAL_ASSERT_OUTPUT

#include <string_view>                    // for std::string_view

// IWYU pragma: no_include "osp/types.h"

// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <cstddef>
// IWYU pragma: no_include <stdint.h>
// IWYU pragma: no_include <algorithm>

using namespace osp;
using namespace osp::active;
using namespace adera::active;
using namespace adera::shader;

void PlumeShader::draw_plume(
        ActiveEnt ent,
        ViewProjMatrix const& viewProj,
        EntityToDraw::UserData_t userData) noexcept
{
    using Magnum::GL::Renderer;
    using Magnum::GL::Texture2D;

    ACtxPlumeData &rData = *reinterpret_cast<ACtxPlumeData*>(userData[0]);

    // Collect uniform data
    ACompDrawTransform const &drawTf    = rData.m_pDrawTf->get(ent);
    ACompExhaustPlume const &comp       = rData.m_pExaustPlumes->get(ent);
    PlumeEffectData const &effect       = *comp.m_effect;
    Magnum::GL::Mesh &rMesh             = *rData.m_pMesh->get(ent).m_mesh;

    Magnum::Matrix4 entRelative = viewProj.m_view * drawTf.m_transformWorld;

    rData.m_shader
        .bindNozzleNoiseTexture     (*rData.m_tmpTex)
        .bindCombustionNoiseTexture (*rData.m_tmpTex)
        .setMeshZBounds             (effect.m_zMax, effect.m_zMin)
        .setBaseColor               (effect.m_color)
        .setFlowVelocity            (effect.m_flowVelocity)
        .updateTime                 (comp.m_time)
        .setPower                   (comp.m_powerLevel)
        .setTransformationMatrix    (entRelative)
        .setProjectionMatrix        (viewProj.m_proj)
        .setNormalMatrix            (entRelative.normalMatrix());

    // Draw back face
    Renderer::setFaceCullingMode(Renderer::PolygonFacing::Front);
    rData.m_shader.draw(rMesh);

    // Draw front face
    Renderer::setFaceCullingMode(Renderer::PolygonFacing::Back);
    rData.m_shader.draw(rMesh);
}

void PlumeShader::assign_plumes(
        RenderGroup::ArrayView_t entities,
        RenderGroup::Storage_t& rStorage,
        ACtxPlumeData& rData)
{
    for (ActiveEnt ent : entities)
    {
        rStorage.emplace(ent, EntityToDraw{&draw_plume, {&rData} });
    }
}

void PlumeShader::init()
{
    using namespace Magnum;

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

PlumeShader& PlumeShader::setProjectionMatrix(Magnum::Matrix4 const& matrix)
{
    setUniform(static_cast<Magnum::Int>(UniformPos::ProjMat), matrix);
    return *this;
}

PlumeShader& PlumeShader::setTransformationMatrix(Magnum::Matrix4 const& matrix)
{
    setUniform(static_cast<Magnum::Int>(UniformPos::ModelTransformMat), matrix);
    return *this;
}

PlumeShader& PlumeShader::setNormalMatrix(Magnum::Matrix3x3 const& matrix)
{
    setUniform(static_cast<Magnum::Int>(UniformPos::NormalMat), matrix);
    return *this;
}

PlumeShader& PlumeShader::setMeshZBounds(float topZ, float bottomZ)
{
    setUniform(static_cast<Magnum::Int>(UniformPos::MeshTopZ), topZ);
    setUniform(static_cast<Magnum::Int>(UniformPos::MeshBottomZ), bottomZ);
    return *this;
}

PlumeShader& PlumeShader::bindNozzleNoiseTexture(Magnum::GL::Texture2D& tex)
{
    tex.bind(static_cast<Magnum::Int>(TextureSlot::NozzleNoiseTexUnit));
    return *this;
}

PlumeShader& PlumeShader::bindCombustionNoiseTexture(Magnum::GL::Texture2D& tex)
{
    tex.bind(static_cast<Magnum::Int>(TextureSlot::CombustionNoiseTexUnit));
    return *this;
}

PlumeShader& PlumeShader::setBaseColor(const Magnum::Color4 color)
{
    setUniform(static_cast<Magnum::Int>(UniformPos::BaseColor), color);
    return *this;
}

PlumeShader& PlumeShader::setFlowVelocity(const float vel)
{
    setUniform(static_cast<Magnum::Int>(UniformPos::FlowVelocity), vel);
    return *this;
}

PlumeShader& PlumeShader::updateTime(const float currentTime)
{
    setUniform(static_cast<Magnum::Int>(UniformPos::Time), currentTime);
    return *this;
}

PlumeShader& PlumeShader::setPower(const float power)
{
    setUniform(static_cast<Magnum::Int>(UniformPos::Power), power);
    return *this;
}
