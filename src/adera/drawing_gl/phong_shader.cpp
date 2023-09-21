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
#include "phong_shader.h"

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using namespace osp;
using namespace osp::draw;

void adera::shader::draw_ent_phong(
        DrawEnt                     ent,
        ViewProjMatrix const&       viewProj,
        EntityToDraw::UserData_t    userData) noexcept
{
    using Flag = PhongGL::Flag;

    void* const pData   = std::get<0>(userData);
    void* const pShader = std::get<1>(userData);
    assert(pData   != nullptr);
    assert(pShader != nullptr);

    auto &rData   = *reinterpret_cast<ACtxDrawPhong*>(pData);
    auto &rShader = *reinterpret_cast<PhongGL*>(pShader);

    // Collect uniform information
    Matrix4 const &drawTf = (*rData.pDrawTf)[ent];

    Magnum::Matrix4 entRelative = viewProj.m_view * drawTf;

    /* 4th component indicates light type. A value of 0.0f indicates that the
     * light is a direction light coming from the specified direction relative
     * to the camera.
     */
    //Vector4 light = ;

    if (rShader.flags() & Flag::DiffuseTexture)
    {
        TexGlId const texGlId = (*rData.pDiffuseTexId)[ent].m_glId;
        Magnum::GL::Texture2D &rTexture = rData.pTexGl->get(texGlId);
        rShader.bindDiffuseTexture(rTexture);

        if (rShader.flags() & (Flag::AmbientTexture | Flag::AlphaMask))
        {
            rShader.bindAmbientTexture(rTexture);
        }
    }

    if (rData.pColor != nullptr)
    {
        rShader.setDiffuseColor((*rData.pColor)[ent]);
    }

    MeshGlId const      meshId = (*rData.pMeshId)[ent].m_glId;
    Magnum::GL::Mesh    &rMesh = rData.pMeshGl->get(meshId);

    Matrix3 a{viewProj.m_view};


    // Lights with w=0.0f are directional lights
    // Directonal lights are camera-relative, so we need 'viewProj.m_view *'
    auto const lightPositions =
    {
        viewProj.m_view * Vector4{ Vector3{0.2f, 0.6f, 0.5f}.normalized(), 0.0f},
        viewProj.m_view * Vector4{-Vector3{0.0f, 0.0f, 1.0f}, 0.0f}
    };

    auto const lightColors =
    {
        0xddd4Cd_rgbf,
        0x32354e_rgbf
    };

    auto const lightSpecColors =
    {
        0xfff5ed_rgbf,
        0x000000_rgbf
    };

    // TODO: find a better way to deal with lights instead of hard-coding it
    rShader
        .setAmbientColor(0x1a1e29ff_rgbaf)
        .setSpecularColor(0xffffff00_rgbaf)
        .setLightColors(lightColors)
        .setLightSpecularColors(lightSpecColors)
        .setLightPositions(lightPositions)
        .setTransformationMatrix(entRelative)
        .setProjectionMatrix(viewProj.m_proj)
        .setNormalMatrix(entRelative.normalMatrix())
        .draw(rMesh);
}

