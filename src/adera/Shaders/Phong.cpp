#include "Phong.h"
#include <Magnum/Math/Matrix4.h>

using namespace osp::active;
using namespace adera::shader;

void Phong::draw(ActiveEnt e,
    ActiveScene& scene, 
    Magnum::GL::Mesh& mesh,
    ACompCamera const& camera,
    ACompTransform const& transform)
{
    PhongShaderInstance& shaderInstance = scene.reg_get<PhongShaderInstance>(e);
    Phong& shader = *shaderInstance.m_shaderProgram;

    Magnum::Matrix4 entRelative = camera.m_inverse * transform.m_transformWorld;

    shader
        .bindDiffuseTexture(*shaderInstance.m_textures[0])
        .setAmbientColor(shaderInstance.m_ambientColor)
        .setSpecularColor(shaderInstance.m_specularColor)
        .setLightPosition(shaderInstance.m_lightPosition)
        .setTransformationMatrix(entRelative)
        .setProjectionMatrix(camera.m_projection)
        .setNormalMatrix(entRelative.normalMatrix())
        .draw(mesh);
}
