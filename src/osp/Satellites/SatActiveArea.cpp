
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Constants.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Primitives/Cube.h>



#include <iostream>

#include "SatActiveArea.h"
#include "../Resource/SturdyImporter.h"

// for the 0xrrggbb_rgbf literals
using namespace Magnum::Math::Literals;


namespace osp
{


SatActiveArea::SatActiveArea() : SatelliteObject()
{
    // do something here eventually
}

SatActiveArea::~SatActiveArea()
{
    // do something here eventually
}

void SatActiveArea::draw_gl()
{
    //std::cout << "draw call\n";

    using namespace Magnum;

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

    // Temporary: draw the spinning rectangular prisim

    static Deg lazySpin;
    lazySpin += 5.0_degf;

    Matrix4 ttt;
    ttt = Matrix4::translation(Vector3(0, 0, -5))
        * Matrix4::rotationY(lazySpin)
        * Matrix4::scaling({0.05f, 1.0f, 2.0f});

    (*m_shader)
        .setDiffuseColor(0x67ff00_rgbf)
        .setAmbientColor(0x111111_rgbf)
        .setSpecularColor(0x330000_rgbf)
        .setLightPosition({10.0f, 15.0f, 5.0f})
        .setTransformationMatrix(ttt)
        .setProjectionMatrix(m_camera->projectionMatrix())
        .setNormalMatrix(ttt.normalMatrix());

    m_bocks->draw(*m_shader);

}

int SatActiveArea::on_load()
{
    // when switched to. Active areas can only be loaded by the universe.
    // why, and how would an active area load another active area?
    m_loadedActive = true;



    // Temporary: draw a spinning rectangular prisim

    m_bocks = std::make_unique<Magnum::GL::Mesh>(
                Magnum::GL::Mesh(Magnum::MeshTools::compile(
                                        Magnum::Primitives::cubeSolid())));

    m_shader = std::make_unique<Magnum::Shaders::Phong>(Magnum::Shaders::Phong{});


    Object3D *cameraObj = new Object3D{&m_scene};
    (*cameraObj).translate(Magnum::Vector3::zAxis(100.0f));
    //            .rotate();
    m_camera = new Magnum::SceneGraph::Camera3D(*cameraObj);
    (*m_camera)
        .setAspectRatioPolicy(Magnum::SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Magnum::Matrix4::perspectiveProjection(45.0_degf, 1.0f, 0.001f, 100.0f))
        .setViewport(Magnum::GL::defaultFramebuffer.viewport().size());



    return 0;
}

}
