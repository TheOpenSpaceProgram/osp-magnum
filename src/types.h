#pragma once

#include <map>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Math/Matrix4.h>


#include <Magnum/Platform/Sdl2Application.h>

// An int for space
typedef int64_t SpaceInt;

// A Vector3 for space
typedef Magnum::Math::Vector3<SpaceInt> Vector3s;

//typedef KeyMap std::map<Magnum::Platform::Application::KeyEvent>

using Magnum::Vector2;
using Magnum::Vector2i;

using Magnum::Vector3;
using Magnum::Quaternion;
using Magnum::Matrix4;

class TranslateRotateScale
{

};
