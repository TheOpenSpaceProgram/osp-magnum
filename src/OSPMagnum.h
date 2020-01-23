#pragma once

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Platform/ScreenedApplication.h>
#include <Magnum/Platform/Screen.h>
#include <Magnum/Shaders/VertexColor.h>

#include "osp/Universe.h"


namespace osp
{

class OSPMagnum: public Magnum::Platform::Application
{

public:
    explicit OSPMagnum(const Magnum::Platform::Application::Arguments& arguments);

private:

    void drawEvent() override;

};



}
