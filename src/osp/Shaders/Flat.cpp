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
#include "Flat.h"

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using namespace osp;
using namespace osp::active;
using namespace osp::shader;

void shader::draw_ent_flat(
        DrawEnt ent, ViewProjMatrix const& viewProj,
        EntityToDraw::UserData_t userData) noexcept
{
    using Flag = Flat::Flag;

    void* const pData   = std::get<0>(userData);
    void* const pShader = std::get<1>(userData);
    assert(pData   != nullptr);
    assert(pShader != nullptr);

    auto &rData   = *reinterpret_cast<ACtxDrawFlat*>(pData);
    auto &rShader = *reinterpret_cast<Flat*>(pShader);

    // Collect uniform information
    Matrix4 const &drawTf = (*rData.m_pDrawTf)[ent];

    if (rShader.flags() & Flag::Textured)
    {
        TexGlId const texGlId = (*rData.m_pDiffuseTexId)[ent].m_glId;
        rShader.bindTexture(rData.m_pTexGl->get(texGlId));
    }

    if (rData.m_pColor != nullptr)
    {
        rShader.setColor((*rData.m_pColor)[ent]);
    }

    MeshGlId const meshId = (*rData.m_pMeshId)[ent].m_glId;
    Magnum::GL::Mesh &rMesh = rData.m_pMeshGl->get(meshId);

    rShader
        .setTransformationProjectionMatrix(viewProj.m_viewProj * drawTf)
        .draw(rMesh);
}


