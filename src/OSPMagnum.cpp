#include "OSPMagnum.h"
#include "osp/types.h"

#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>

#include <iostream>

namespace osp
{

// temporary-ish
const UserInputHandler::DeviceId sc_keyboard = 0;
const UserInputHandler::DeviceId sc_mouse = 1;



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
//        m_userInput.event_clear();
//        // end of physics update

//        m_area->draw_gl();
//    }

    for (auto &[name, scene] : m_scenes)
    {
        scene.update();
    }

    for (auto &[name, scene] : m_scenes)
    {
        scene.update_hierarchy_transforms();
        //scene.draw(m_camera);
    }



    // TODO: GUI and stuff

    swapBuffers();
    m_timeline.nextFrame();
    redraw();
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

active::ActiveScene& OSPMagnum::scene_add(const std::string &name)
{
    auto pair = m_scenes.try_emplace(name, m_userInput, m_ospApp);

    return pair.first->second;
}

}
