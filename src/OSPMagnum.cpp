#include <iostream>

#include "OSPMagnum.h"


namespace osp
{

OSPMagnum::OSPMagnum(const Magnum::Platform::Application::Arguments& arguments):
    Magnum::Platform::Application{
        arguments,
        Configuration{}.setTitle("OSP-Magnum").setSize({1280, 720})},
    m_area(nullptr){
    //.setWindowFlags(Configuration::WindowFlag::Hidden)



}

void OSPMagnum::set_active_area(SatActiveArea& area)
{
    // TODO: tell the current active area to unload

    m_area = &area;

}


void OSPMagnum::drawEvent()
{
    Magnum::GL::defaultFramebuffer.clear(Magnum::GL::FramebufferClear::Color
                                         | Magnum::GL::FramebufferClear::Depth);

    if (m_area)
    {
        //Scene3D& scene = m_area->get_scene();
        if (!m_area->is_loaded_active())
        {
            // Load active area if not already done so
            m_area->on_load();
        }

        m_area->draw_gl();
    }

    // TODO: GUI and stuff

    swapBuffers();
    redraw();
}

}
