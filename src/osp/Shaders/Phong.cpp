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

#include <Magnum/Math/Matrix4.h>

#include "../Active/ActiveScene.h"
#include "../Active/SysRender.h"

using namespace osp::active;
using namespace osp::shader;

void Phong::draw_entity(
        ActiveEnt ent, ActiveScene& rScene, ACompCamera const& camera,
        void* pUserData) noexcept
{
    auto &rShader = *reinterpret_cast<Phong*>(pUserData);

    // Collect uniform information
    auto& drawTf = rScene.reg_get<ACompDrawTransform>(ent);
    Magnum::GL::Mesh& rMesh = *rScene.reg_get<ACompMesh>(ent).m_mesh;

    Magnum::Matrix4 entRelative = camera.m_inverse * drawTf.m_transformWorld;

    /* 4th component indicates light type. A value of 0.0f indicates that the
     * light is a direction light coming from the specified direction relative
     * to the camera.
     */
    Vector4 light = Vector4{1.0f, 0.0f, 0.0f, 0.0f};

    if (rShader.flags() & Flag::DiffuseTexture)
    {
        rShader.bindDiffuseTexture(*rScene.reg_get<ACompDiffuseTex>(ent).m_tex);
    }

    rShader
        .setAmbientColor(Magnum::Color4{0.1f})
        .setSpecularColor(Magnum::Color4{1.0f})
        .setLightPositions({light})
        .setTransformationMatrix(entRelative)
        .setProjectionMatrix(camera.m_projection)
        .setNormalMatrix(Matrix3{drawTf.m_transformWorld})
        .draw(rMesh);
}

Phong::RenderGroup::DrawAssigner_t Phong::gen_assign_phong_opaque(
        Phong* pNoTexture, Phong* pTextured)
{
    return [pNoTexture, pTextured]
            (ActiveScene& rScene, RenderGroup::Storage_t& rStorage,
             RenderGroup::ArrayView_t entities)
    {
        ActiveReg_t &rReg = rScene.get_registry();

        auto viewOpaque = rReg.view<ACompOpaque>();
        auto viewDiffuse = rReg.view<ACompDiffuseTex>();

        for (ActiveEnt ent : entities)
        {
            if (!viewOpaque.contains(ent))
            {
                continue; // This assigner is for opaque materials only
            }

            if (viewDiffuse.contains(ent))
            {
                rStorage.emplace(ent, EntityToDraw{&draw_entity, pTextured});
            }
            else
            {
                rStorage.emplace(ent, EntityToDraw{&draw_entity, pNoTexture});
            }
        }
    };
}
