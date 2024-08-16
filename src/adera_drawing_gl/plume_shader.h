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

#if 0

#include "../SysExhaustPlume.h"

#include <osp/Active/opengl/SysRenderGL.h>
#include <osp/Active/activetypes.h>

#include <Magnum/GL/Texture.h>
#include <Magnum/GL/AbstractShaderProgram.h>

#include <Magnum/Shaders/GenericGL.h>

#include <Magnum/Magnum.h> // for Magnum::Int

namespace osp::active { class ActiveScene; }
namespace osp::active { struct ACompCamera; }

namespace adera::shader
{

struct ACtxPlumeData;

class PlumeShader : public Magnum::GL::AbstractShaderProgram
{

    using RenderGroup = osp::active::RenderGroup;

public:
    // Vertex attribs
    Magnum::Shaders::GenericGL3D::Position Position;
    Magnum::Shaders::GenericGL3D::Normal Normal;
    Magnum::Shaders::GenericGL3D::TextureCoordinates TextureCoordinates;

    // Outputs
    enum : Magnum::UnsignedInt
    {
        ColorOutput = 0
    };

    PlumeShader();

    static void draw_plume(
            osp::active::ActiveEnt ent,
            osp::active::ViewProjMatrix const& viewProj,
            osp::active::EntityToDraw::UserData_t userData) noexcept;

    static void assign_plumes(
            RenderGroup::ArrayView_t entities,
            RenderGroup::Storage_t& rStorage,
            ACtxPlumeData& rData);

private:

    // GL init
    void init();

    // Uniforms
    enum class UniformPos : Magnum::Int
    {
        ProjMat = 0,
        ModelTransformMat = 1,
        NormalMat = 2,
        MeshTopZ = 3,
        MeshBottomZ = 4,
        NozzleNoiseTex = 5,
        CombustionNoiseTex = 6,
        BaseColor = 7,
        FlowVelocity = 8,
        Time = 9,
        Power = 10
    };

    // Texture2D slots
    enum class TextureSlot : Magnum::Int
    {
        NozzleNoiseTexUnit = 0,
        CombustionNoiseTexUnit = 1
    };

    // Hide irrelevant calls
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    // Private uniform setters
    PlumeShader& setProjectionMatrix(const Magnum::Matrix4& matrix);
    PlumeShader& setTransformationMatrix(const Magnum::Matrix4& matrix);
    PlumeShader& setNormalMatrix(const Magnum::Matrix3x3& matrix);

    PlumeShader& setMeshZBounds(float topZ, float bottomZ);
    PlumeShader& bindNozzleNoiseTexture(Magnum::GL::Texture2D& rTex);
    PlumeShader& bindCombustionNoiseTexture(Magnum::GL::Texture2D& rTex);
    PlumeShader& setBaseColor(const Magnum::Color4 color);
    PlumeShader& setFlowVelocity(const float vel);
    PlumeShader& updateTime(const float currentTime);
    PlumeShader& setPower(const float power);
};

/**
 * @brief Required data for drawing exaust plumes in the scene
 */
struct ACtxPlumeData
{
    template<typename COMP_T>
    using acomp_storage_t       = typename osp::active::acomp_storage_t<COMP_T>;
    using ACompExhaustPlume     = adera::active::ACompExhaustPlume;
    using MeshGlId              = osp::active::MeshGlId;
    using TexGlId               = osp::active::TexGlId;

    PlumeShader m_shader;

    TexGlId m_tmpTex;

    acomp_storage_t<osp::Matrix4>       *m_pDrawTf{nullptr};
    acomp_storage_t<ACompExhaustPlume>  *m_pExaustPlumes{nullptr};
    acomp_storage_t<MeshGlId>           *m_pMeshId{nullptr};
    osp::active::MeshGlStorage_t        *m_pMeshGl{nullptr};
    osp::active::TexGlStorage_t         *m_pTexGl{nullptr};
};

} // namespace adera::shader

#endif
