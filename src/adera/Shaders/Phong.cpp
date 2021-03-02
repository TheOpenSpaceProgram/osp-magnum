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

using namespace osp::active;
using namespace adera::shader;

void Phong::draw_entity(ActiveEnt e,
    ActiveScene& rScene, 
    Magnum::GL::Mesh& rMesh,
    ACompCamera const& camera,
    ACompTransform const& transform)
{
    auto& shaderInstance = rScene.reg_get<ACompPhongInstance>(e);
    Phong& shader = *shaderInstance.m_shaderProgram;

    Magnum::Matrix4 entRelative = camera.m_inverse * transform.m_transformWorld;

    /* 4th component indicates light type. A value of 0.0f indicates that the
     * light is a direction light coming from the specified direction relative
     * to the camera.
     */
    Magnum::Vector4 light = {shaderInstance.m_lightPosition, 0.0f};

    shader
        .bindDiffuseTexture(*shaderInstance.m_textures[0])
        .setAmbientColor(shaderInstance.m_ambientColor)
        .setSpecularColor(shaderInstance.m_specularColor)
        .setLightPositions({light})
        .setTransformationMatrix(entRelative)
        .setProjectionMatrix(camera.m_projection)
        .setNormalMatrix(entRelative.normalMatrix())
        .draw(rMesh);
}
