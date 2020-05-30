#include <iostream>

#include "OSPMagnum.h"


namespace osp
{

OSPMagnum::OSPMagnum(const Magnum::Platform::Application::Arguments& arguments):
    Magnum::Platform::Application{
        arguments,
        Configuration{}.setTitle("OSP-Magnum").setSize({1280, 720})},
    m_area(nullptr),
    m_userInput(12)
{
    //.setWindowFlags(Configuration::WindowFlag::Hidden)

    m_timeline.start();

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
            // Enable active area if not already done so
            m_area->activate();
        }

       // std::cout << "deltaTime: " << m_timeline.previousFrameDuration() << "\n";

        m_area->update_physics(1.0f / 60.0f);

        m_area->draw_gl();
    }

    // TODO: GUI and stuff

    swapBuffers();
    m_timeline.nextFrame();
    redraw();
}

void OSPMagnum::keyPressEvent(KeyEvent& event)
{
    if (m_area)
    {
        m_area->input_key_press(event);
    }
}

void OSPMagnum::keyReleaseEvent(KeyEvent& event)
{
    if (m_area)
    {
        m_area->input_key_release(event);
    }
}

void OSPMagnum::mousePressEvent(MouseEvent& event)
{
    if (m_area)
    {
        m_area->input_mouse_press(event);
    }
}
void OSPMagnum::mouseReleaseEvent(MouseEvent& event)
{
    if (m_area)
    {
        m_area->input_mouse_release(event);
    }
}
void OSPMagnum::mouseMoveEvent(MouseMoveEvent& event)
{
    if (m_area)
    {
        m_area->input_mouse_move(event);
    }
}

}
