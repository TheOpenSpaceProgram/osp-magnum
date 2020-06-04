#include <iostream>

#include "OSPMagnum.h"


namespace osp
{

// temporary-ish
const UserInputHandler::DeviceId sc_keyboard = 0;
const UserInputHandler::DeviceId sc_mouse = 1;



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
    m_userInput.update_controls();


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

    m_userInput.event_clear();
}



void OSPMagnum::keyPressEvent(KeyEvent& event)
{
    if (event.isRepeated())
    {
        return;
    }
    m_userInput.event_raw(sc_keyboard, (int) event.key(),
                          UserInputHandler::ButtonRawEvent::PRESSED);
}

void OSPMagnum::keyReleaseEvent(KeyEvent& event)
{
    if (event.isRepeated())
    {
        return;
    }

    m_userInput.event_raw(sc_keyboard, (int) event.key(),
                          UserInputHandler::ButtonRawEvent::RELEASED);

}

void OSPMagnum::mousePressEvent(MouseEvent& event)
{
    m_userInput.event_raw(sc_mouse, (int) event.button(),
                          UserInputHandler::ButtonRawEvent::PRESSED);

}
void OSPMagnum::mouseReleaseEvent(MouseEvent& event)
{
    m_userInput.event_raw(sc_mouse, (int) event.button(),
                          UserInputHandler::ButtonRawEvent::RELEASED);

}
void OSPMagnum::mouseMoveEvent(MouseMoveEvent& event)
{

}

}
