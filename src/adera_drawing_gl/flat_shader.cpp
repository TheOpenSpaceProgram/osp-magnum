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
#include "flat_shader.h"

using namespace osp;
using namespace osp::draw;

void adera::shader::draw_ent_flat(
        DrawEnt                     ent,
        ViewProjMatrix const&       viewProj,
        EntityToDraw::UserData_t    userData) noexcept
{
    void* const pData   = std::get<0>(userData);
    void* const pShader = std::get<1>(userData);
    assert(pData   != nullptr);
    assert(pShader != nullptr);

    auto &rData   = *reinterpret_cast<ACtxDrawFlat*>(pData);
    auto &rShader = *reinterpret_cast<FlatGL3D*>(pShader);

    // Collect uniform information
    Matrix4 const &drawTf = (*rData.pDrawTf)[ent];

    if (rShader.flags() & FlatGL3D::Flag::Textured)
    {
        TexGlId const texGlId = (*rData.pDiffuseTexId)[ent].m_glId;
        rShader.bindTexture(rData.pTexGl->get(texGlId));
    }

    if (rData.pColor != nullptr)
    {
        rShader.setColor((*rData.pColor)[ent]);
    }

    MeshGlId const      meshId = (*rData.pMeshId)[ent].m_glId;
    Magnum::GL::Mesh    &rMesh = rData.pMeshGl->get(meshId);

    rShader.setTransformationProjectionMatrix(viewProj.m_viewProj * drawTf)
           .draw(rMesh);
}
