#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Color.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Shaders/Phong.h>


typedef Magnum::SceneGraph::Object<Magnum::SceneGraph::MatrixTransformation3D> Object3D;
typedef Magnum::SceneGraph::Scene<Magnum::SceneGraph::MatrixTransformation3D> Scene3D;


class DrawablePhongColored: public Magnum::SceneGraph::Drawable3D
{

    public:
        explicit DrawablePhongColored(
            Object3D& object,
            Magnum::Shaders::Phong& shader,
            Magnum::GL::Mesh& mesh,
            const Magnum::Color4& color,
            Magnum::SceneGraph::DrawableGroup3D& group):
            Magnum::SceneGraph::Drawable3D{object, &group},
            m_shader(shader), m_mesh(mesh), m_color{color} {}

    private:
        void draw(const Magnum::Matrix4& transformationMatrix,
                  Magnum::SceneGraph::Camera3D& camera) override;

        Magnum::Shaders::Phong& m_shader;
        Magnum::GL::Mesh& m_mesh;
        Magnum::Color4 m_color;

};
