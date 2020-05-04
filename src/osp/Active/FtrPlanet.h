#pragma once

#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/AbstractFeature.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/GL/Mesh.h>


#include "../scene.h"
#include "../Resource/PlanetData.h"
#include "Planet/PlanetGeometryA.h"


#include <Newton.h>

using Magnum::SceneGraph::AbstractFeature3D;

namespace osp
{

//template <class PlanetMeshGen, class PlanetColliderGen>
class FtrPlanet: public Magnum::SceneGraph::Drawable3D
{
public:
    explicit FtrPlanet(Object3D& object, PlanetData& data,
                       Magnum::SceneGraph::DrawableGroup3D& group);

    ~FtrPlanet();

private:

    void draw(const Magnum::Matrix4& transformationMatrix,
              Magnum::SceneGraph::Camera3D& camera) override;


    PlanetGeometryA m_planet;
    Magnum::GL::Mesh m_mesh;
    std::unique_ptr<Magnum::Shaders::Phong> m_shader;
    Magnum::GL::Buffer m_vrtxBufGL;
    Magnum::GL::Buffer m_indxBufGL;
};

}
