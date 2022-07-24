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
#include "Phong.h"

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using namespace osp;
using namespace osp::active;
using namespace osp::shader;

void shader::draw_ent_phong(
        ActiveEnt ent, ViewProjMatrix const& viewProj,
        EntityToDraw::UserData_t userData) noexcept
{
    using Flag = Phong::Flag;

    void* const pData   = std::get<0>(userData);
    void* const pShader = std::get<1>(userData);
    assert(pData   != nullptr);
    assert(pShader != nullptr);

    auto &rData   = *reinterpret_cast<ACtxDrawPhong*>(pData);
    auto &rShader = *reinterpret_cast<Phong*>(pShader);

    // Collect uniform information
    ACompDrawTransform const &drawTf = rData.m_pDrawTf->get(ent);

    Magnum::Matrix4 entRelative = viewProj.m_view * drawTf.m_transformWorld;

    /* 4th component indicates light type. A value of 0.0f indicates that the
     * light is a direction light coming from the specified direction relative
     * to the camera.
     */
    //Vector4 light = ;

    if (rShader.flags() & Flag::DiffuseTexture)
    {
        TexGlId const texGlId = rData.m_pDiffuseTexId->get(ent).m_glId;
        Magnum::GL::Texture2D &rTexture = rData.m_pTexGl->get(texGlId);
        rShader.bindDiffuseTexture(rTexture);

        if (rShader.flags() & (Flag::AmbientTexture | Flag::AlphaMask))
        {
            rShader.bindAmbientTexture(rTexture);
        }
    }

    if (rData.m_pColor != nullptr)
    {
        rShader.setDiffuseColor(rData.m_pColor->contains(ent)
                                ? rData.m_pColor->get(ent)
                                : 0xffffffff_rgbaf);
    }

    MeshGlId const meshId = rData.m_pMeshId->get(ent).m_glId;
    Magnum::GL::Mesh &rMesh = rData.m_pMeshGl->get(meshId);

    rShader
        .setAmbientColor(0x000000ff_rgbaf)
        .setSpecularColor(0xffffff00_rgbaf)
        .setLightColors({0xfff5ec_rgbf, 0xe4e8ff_rgbf})
        .setLightPositions({ Vector4{ Vector3{0.2f, 0.6f, 0.5f}.normalized(), 0.0f},
                             Vector4{-Vector3{0.2f, 0.6f, 0.5f}.normalized(), 0.0f} })
        .setTransformationMatrix(entRelative)
        .setProjectionMatrix(viewProj.m_proj)
        .setNormalMatrix(Matrix3{drawTf.m_transformWorld})
        .draw(rMesh);
}


