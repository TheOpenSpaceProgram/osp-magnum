#include "OSPMagnum.h"
#include "osp/types.h"

#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>

#include <iostream>

namespace osp
{

OSPMagnum::OSPMagnum(const Magnum::Platform::Application::Arguments& arguments,
                     OSPApplication &ospApp) :
        Magnum::Platform::Application{
            arguments,
            Configuration{}.setTitle("OSP-Magnum").setSize({1280, 720})},
        m_userInput(12),
        m_ospApp(ospApp)
{
    //.setWindowFlags(Configuration::WindowFlag::Hidden)

    m_timeline.start();

}

void OSPMagnum::drawEvent()
{

    Magnum::GL::defaultFramebuffer.clear(Magnum::GL::FramebufferClear::Color
                                         | Magnum::GL::FramebufferClear::Depth);

//    if (m_area)
//    {
//        //Scene3D& scene = m_area->get_scene();
//        //if (!m_area->is_loaded_active())
//        //{
//        //    // Enable active area if not already done so
//        //    m_area->activate();
//        //}

//       // std::cout << "deltaTime: " << m_timeline.previousFrameDuration() << "\n";

//        // TODO: physics update
//        m_userInput.update_controls();
//        m_area->update_physics(1.0f / 60.0f);
//        m_userInput.clear_events();
//        // end of physics update

//        m_area->draw_gl();
//    }

    m_userInput.update_controls();

    for (auto &[name, scene] : m_scenes)
    {
        scene.update();
    }

    m_userInput.clear_events();

    for (auto &[name, scene] : m_scenes)
    {
        scene.update_hierarchy_transforms();


        // temporary: draw using first camera component found
        scene.draw(scene.get_registry().view<active::ACompCamera>().front());
    }


    // TODO: GUI and stuff

    swapBuffers();
    m_timeline.nextFrame();
    redraw();
}



void OSPMagnum::keyPressEvent(KeyEvent& event)
{
    if (event.isRepeated()) { return; }
    m_userInput.event_raw(sc_keyboard, (int) event.key(),
                          UserInputHandler::ButtonRawEvent::PRESSED);
}

void OSPMagnum::keyReleaseEvent(KeyEvent& event)
{
    if (event.isRepeated()) { return; }
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
    m_userInput.mouse_delta(event.relativePosition());
}

void OSPMagnum::mouseScrollEvent(MouseScrollEvent & event)
{
    m_userInput.scroll_delta(static_cast<Vector2i>(event.offset()));
}

active::ActiveScene& OSPMagnum::scene_add(const std::string &name)
{
    auto pair = m_scenes.try_emplace(name, m_userInput, m_ospApp);
    return pair.first->second;
}

}
