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

#include <osp/Active/SysRender.h>

#include <osp/Resource/Package.h>

#include <Magnum/Math/Matrix4.h>

using namespace osp::active;
using namespace osp::shader;

void Phong::draw_entity(
        ActiveEnt ent, ACompCamera const& camera,
        osp::active::EntityToDraw::UserData_t userData) noexcept
{
    auto &rData = *reinterpret_cast<ACtxPhongData*>(userData[0]);
    auto &rShader = *reinterpret_cast<Phong*>(userData[1]);

    // Collect uniform information
    auto const& drawTf = rData.m_views->m_drawTf.get<ACompDrawTransform>(ent);

    Magnum::Matrix4 entRelative = camera.m_inverse * drawTf.m_transformWorld;

    /* 4th component indicates light type. A value of 0.0f indicates that the
     * light is a direction light coming from the specified direction relative
     * to the camera.
     */
    Vector4 light = Vector4{Vector3{0.2f, -1.0f, 0.5f}.normalized(), 0.0f};

    if (rShader.flags() & Flag::DiffuseTexture)
    {
        rShader.bindDiffuseTexture(*rData.m_views->m_diffuseTexGl.get<ACompTextureGL>(ent).m_tex);
    }

    rShader
        .setAmbientColor(Magnum::Color4{0.1f})
        .setSpecularColor(Magnum::Color4{1.0f})
        .setLightPositions({light})
        .setTransformationMatrix(entRelative)
        .setProjectionMatrix(camera.m_projection)
        .setNormalMatrix(Matrix3{drawTf.m_transformWorld})
        .draw(*rData.m_views->m_meshGl.get<ACompMeshGL>(ent).m_mesh);
}


void Phong::assign_phong_opaque(
        RenderGroup::ArrayView_t entities,
        RenderGroup::Storage_t& rStorage,
        acomp_view_t<ACompOpaque const> viewOpaque,
        acomp_view_t<ACompTextureGL const> viewDiffuse,
        ACtxPhongData &rData)
{

    for (ActiveEnt ent : entities)
    {
        if (!viewOpaque.contains(ent))
        {
            continue; // This assigner is for opaque materials only
        }

        if (viewDiffuse.contains(ent))
        {
            rStorage.emplace(
                    ent, EntityToDraw{&draw_entity, {&rData, &(*rData.m_shaderDiffuse)} });
        }
        else
        {
            rStorage.emplace(
                    ent, EntityToDraw{&draw_entity, {&rData, &(*rData.m_shaderUntextured)} });
        }
    }
}
