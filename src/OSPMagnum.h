#pragma once


#include <Magnum/GL/Buffer.h>

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Platform/ScreenedApplication.h>
#include <Magnum/Platform/Screen.h>
#include <Magnum/Shaders/VertexColor.h>

#include <memory>

#include "osp/Universe.h"
#include "osp/Satellites/SatActiveArea.h"


namespace osp
{

class OSPMagnum: public Magnum::Platform::Application
{

public:
    explicit OSPMagnum(const Magnum::Platform::Application::Arguments& arguments);

    void set_active_area(SatActiveArea& area);


private:

    // TODO: not sure how to safely access m_area.
    //       SatelliteObjects are stored with unique_ptr in Satellite
    //       might have to used shared_ptr unless there's a better way
    SatActiveArea* m_area;
    //std::weak_ptr<SatActiveArea> m_area;

    void drawEvent() override;

};



}
