#pragma once

#include <vector>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Math/Color.h>
#include "osp/Active/activetypes.h"
#include "osp/Active/ActiveScene.h"
#include "osp/Resource/Resource.h"

namespace adera::shader
{


class Phong : public Magnum::Shaders::Phong
{
public:
    using Magnum::Shaders::Phong::Phong;

    static void draw(osp::active::ActiveEnt e,
        osp::active::ActiveScene& scene,
        Magnum::GL::Mesh& mesh,
        osp::active::ACompCamera const& camera,
        osp::active::ACompTransform const& transform);

private:

};

struct PhongShaderInstance
{
    osp::DependRes<Phong> m_shaderProgram;
    std::vector<osp::DependRes<Magnum::GL::Texture2D>> m_textures;
    Magnum::Vector3 m_lightPosition;
    Magnum::Color3 m_ambientColor;
    Magnum::Color3 m_specularColor;
};

}