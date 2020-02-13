#include "scene.h"

void DrawablePhongColored::draw(const Magnum::Matrix4& transformationMatrix,
                                Magnum::SceneGraph::Camera3D& camera)
{
    m_shader
        .setDiffuseColor(m_color)
        .setLightPosition(camera.cameraMatrix().transformPoint({-3.0f, 10.0f, 10.0f}))
        .setTransformationMatrix(transformationMatrix)
        .setNormalMatrix(transformationMatrix.normalMatrix())
        .setProjectionMatrix(camera.projectionMatrix());

    m_mesh.draw(m_shader);
}
