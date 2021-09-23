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

#include <osp/Active/scene.h>             // for ACompCamera
#include <osp/Active/drawing.h>           // for ACompDrawTransform
#include <osp/Active/SysRender.h>         // for ACompMesh
#include <osp/Active/ActiveScene.h>       // for ActiveScene
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
        ActiveEnt ent, ActiveScene& rScene,
        ACompCamera const& camera, void* pUserData) noexcept
{
    using Magnum::GL::Renderer;
    using Magnum::GL::Texture2D;

    auto& rResources = rScene.get_context_resources();
    PlumeShader &rShader = *reinterpret_cast<PlumeShader*>(pUserData);

    // Collect uniform data
    auto const& drawTf = rScene.reg_get<ACompDrawTransform>(ent);
    auto const& rPlumeComp = rScene.reg_get<ACompExhaustPlume>(ent);
    auto const& plumedata = *rPlumeComp.m_effect;
    auto& rMesh = *rScene.reg_get<ACompMesh>(ent).m_mesh;

    constexpr std::string_view texName = "OSPData/adera/noise1024.png";
    DependRes<Texture2D> tmpTex = rResources.get<Texture2D>(texName);
    if (tmpTex.empty())
    {
        tmpTex = AssetImporter::compile_tex(texName,
            rScene.get_packages().find("lzdb"), rResources);
    }

    Magnum::Matrix4 entRelative = camera.m_inverse * drawTf.m_transformWorld;

    rShader
        .bindNozzleNoiseTexture(*tmpTex)
        .bindCombustionNoiseTexture(*tmpTex)
        .setMeshZBounds(plumedata.m_zMax, plumedata.m_zMin)
        .setBaseColor(plumedata.m_color)
        .setFlowVelocity(plumedata.m_flowVelocity)
        .updateTime(rPlumeComp.m_time)
        .setPower(rPlumeComp.m_powerLevel)
        .setTransformationMatrix(entRelative)
        .setProjectionMatrix(camera.m_projection)
        .setNormalMatrix(entRelative.normalMatrix());

    // Draw back face
    Renderer::setFaceCullingMode(Renderer::PolygonFacing::Front);
    rShader.draw(rMesh);

    // Draw front face
    Renderer::setFaceCullingMode(Renderer::PolygonFacing::Back);
    rShader.draw(rMesh);
}

PlumeShader::RenderGroup::DrawAssigner_t PlumeShader::gen_assign_plume(
        PlumeShader *pShader)
{
    return [pShader]
            (ActiveScene& rScene, RenderGroup::Storage_t& rStorage,
             RenderGroup::ArrayView_t entities)
    {
        for (ActiveEnt ent : entities)
        {
            rStorage.emplace(ent, EntityToDraw{&draw_plume, pShader});
        }
    };
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
