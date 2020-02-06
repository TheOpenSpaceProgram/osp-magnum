#pragma once

#include <Magnum/Math/Vector3.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/SceneGraph/Scene.h>

// An int for space
typedef int64_t SpaceInt;

// A Vector3 for space
typedef Magnum::Math::Vector3<SpaceInt> Vector3s;


typedef Magnum::SceneGraph::Object<Magnum::SceneGraph::MatrixTransformation3D> Object3D;
typedef Magnum::SceneGraph::Scene<Magnum::SceneGraph::MatrixTransformation3D> Scene3D;
