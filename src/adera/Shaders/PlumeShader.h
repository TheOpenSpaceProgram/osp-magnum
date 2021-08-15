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
#include <Magnum/GL/Texture.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Shaders/GenericGL.h>

#include "adera/Plume.h"
#include <osp/Resource/Resource.h>
#include <osp/Active/activetypes.h>
#include <osp/Active/ActiveScene.h>

namespace adera::shader
{

class PlumeShader : public Magnum::GL::AbstractShaderProgram
{
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

    static void draw_plume(osp::active::ActiveEnt e,
        osp::active::ActiveScene& rScene,
        osp::active::ACompCamera const& camera, void*) noexcept;

private:
    constexpr static std::string_view smc_resourceName = "plume_shader";

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

} // namespace adera::shader
