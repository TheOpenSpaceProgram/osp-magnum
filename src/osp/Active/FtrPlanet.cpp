#include <iostream>

#include <Magnum/GL/GL.h>
#include <Magnum/Math/Angle.h>


// for the 0xrrggbb_rgbf literals
using namespace Magnum::Math::Literals;

#include "../../types.h"
#include "../Satellites/SatActiveArea.h"
#include "FtrPlanet.h"

namespace osp
{

FtrPlanet::FtrPlanet(Object3D& object, PlanetData& data,
                     Magnum::SceneGraph::DrawableGroup3D& group)
          : Magnum::SceneGraph::Drawable3D{object, &group}
{
    std::cout << "Planet feature created\n";
    m_planet.initialize(1.0f);


    m_vrtxBufGL.setData(m_planet.get_vertex_buffer());
    m_indxBufGL.setData(m_planet.get_index_buffer());

    using namespace Magnum;
    //using Magnum::GL::MeshIndexType::UnsignedInt;
    using Magnum::Shaders::Phong;

    m_shader = std::make_unique<Phong>(Phong{});

    m_mesh
        .setPrimitive(MeshPrimitive::Triangles)
        .addVertexBuffer(m_vrtxBufGL, 0, Phong::Position{}, Phong::Normal{})
        .setIndexBuffer(m_indxBufGL, 0, GL::MeshIndexType::UnsignedInt)
        .setCount(m_planet.calc_index_count());


}

FtrPlanet::~FtrPlanet()
{
}

void FtrPlanet::draw(const Magnum::Matrix4 &transformationMatrix,
                     Magnum::SceneGraph::Camera3D &camera)
{

    using Magnum::Deg;
    static Deg lazySpin;
    lazySpin += 1.0_degf;

    Matrix4 ttt;
    ttt = Matrix4::translation(Vector3(0, 0, -5))
        * Matrix4::rotationX(lazySpin)
        * Matrix4::scaling({1.0f, 1.0f, 1.0f});
    static_cast<Object3D&>(this->object()).setTransformation(ttt);

    std::cout << "planetrender\n";
    m_shader
        ->setDiffuseColor(Magnum::Color4{0.2f, 1.0f, 0.2f, 1.0f})
        .setLightPosition(camera.cameraMatrix().transformPoint({-3.0f, 10.0f, 10.0f}))
        .setTransformationMatrix(transformationMatrix)
        .setNormalMatrix(transformationMatrix.normalMatrix())
        .setProjectionMatrix(camera.projectionMatrix());



    m_mesh.draw(*m_shader);
}

}
