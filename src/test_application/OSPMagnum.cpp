/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "OSPMagnum.h"
#include "osp/types.h"

#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>

#include <iostream>

using namespace testapp;

OSPMagnum::OSPMagnum(const Magnum::Platform::Application::Arguments& arguments,
                     osp::OSPApplication &rOspApp) :
        Magnum::Platform::Application{
            arguments,
            Configuration{}.setTitle("OSP-Magnum").setSize({1280, 720})},
        m_userInput(12),
        m_ospApp(rOspApp)
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
        scene.draw(scene.get_registry().view<osp::active::ACompCamera>().front());
    }


    // TODO: GUI and stuff

    swapBuffers();
    m_timeline.nextFrame();
    redraw();
}



void OSPMagnum::keyPressEvent(KeyEvent& event)
{
    if (event.isRepeated()) { return; }
    m_userInput.event_raw(osp::sc_keyboard, (int) event.key(),
                          osp::UserInputHandler::ButtonRawEvent::PRESSED);
}

void OSPMagnum::keyReleaseEvent(KeyEvent& event)
{
    if (event.isRepeated()) { return; }
    m_userInput.event_raw(osp::sc_keyboard, (int) event.key(),
                          osp::UserInputHandler::ButtonRawEvent::RELEASED);
}

void OSPMagnum::mousePressEvent(MouseEvent& event)
{
    m_userInput.event_raw(osp::sc_mouse, (int) event.button(),
                          osp::UserInputHandler::ButtonRawEvent::PRESSED);
}

void OSPMagnum::mouseReleaseEvent(MouseEvent& event)
{
    m_userInput.event_raw(osp::sc_mouse, (int) event.button(),
                          osp::UserInputHandler::ButtonRawEvent::RELEASED);
}

void OSPMagnum::mouseMoveEvent(MouseMoveEvent& event)
{
    m_userInput.mouse_delta(event.relativePosition());
}

void OSPMagnum::mouseScrollEvent(MouseScrollEvent & event)
{
    m_userInput.scroll_delta(static_cast<osp::Vector2i>(event.offset()));
}

osp::active::ActiveScene& OSPMagnum::scene_create(std::string const& name)
{
    auto const& [it, success] = m_scenes.try_emplace(name, m_userInput, m_ospApp);
    return it->second;
}

osp::active::ActiveScene& OSPMagnum::scene_create(std::string && name)
{
    auto const& [it, success] = m_scenes.try_emplace(std::move(name), m_userInput, m_ospApp);
    return it->second;
}

void testapp::config_controls(OSPMagnum& rOspApp)
{
    // Configure Controls

    // It should be pretty easy to write a config file parser that calls these
    // functions.

    using namespace osp;

    using Key_t = OSPMagnum::KeyEvent::Key;
    using Mouse_t = OSPMagnum::MouseEvent::Button;
    using VarOp_t = ButtonVarConfig::VarOperator;
    using VarTrig_t = ButtonVarConfig::VarTrigger;

    UserInputHandler& rUserInput = rOspApp.get_input_handler();

    // vehicle control, used by MachineUserControl

    // would help to get an axis for yaw, pitch, and roll, but use individual
    // axis buttons for now
    rUserInput.config_register_control("vehicle_pitch_up", true,
            {{0, (int) Key_t::S, VarTrig_t::PRESSED, false, VarOp_t::AND}});
    rUserInput.config_register_control("vehicle_pitch_dn", true,
            {{0, (int) Key_t::W, VarTrig_t::PRESSED, false, VarOp_t::AND}});
    rUserInput.config_register_control("vehicle_yaw_lf", true,
            {{0, (int) Key_t::A, VarTrig_t::PRESSED, false, VarOp_t::AND}});
    rUserInput.config_register_control("vehicle_yaw_rt", true,
            {{0, (int) Key_t::D, VarTrig_t::PRESSED, false, VarOp_t::AND}});
    rUserInput.config_register_control("vehicle_roll_lf", true,
            {{0, (int) Key_t::Q, VarTrig_t::PRESSED, false, VarOp_t::AND}});
    rUserInput.config_register_control("vehicle_roll_rt", true,
            {{0, (int) Key_t::E, VarTrig_t::PRESSED, false, VarOp_t::AND}});

    // Set throttle max to Z
    rUserInput.config_register_control("vehicle_thr_max", false,
            {{0, (int) Key_t::Z, VarTrig_t::PRESSED, false, VarOp_t::OR}});
    // Set throttle min to X
    rUserInput.config_register_control("vehicle_thr_min", false,
            {{0, (int) Key_t::X, VarTrig_t::PRESSED, false, VarOp_t::OR}});
    // Set self destruct to LeftCtrl+C or LeftShift+A
    rUserInput.config_register_control("vehicle_self_destruct", false,
            {{0, (int) Key_t::LeftCtrl, VarTrig_t::HOLD, false, VarOp_t::AND},
             {0, (int) Key_t::C, VarTrig_t::PRESSED, false, VarOp_t::OR},
             {0, (int) Key_t::LeftShift, VarTrig_t::HOLD, false, VarOp_t::AND},
             {0, (int) Key_t::A, VarTrig_t::PRESSED, false, VarOp_t::OR}});

    // Camera and Game controls, handled in DebugCameraController

    // Switch to next vehicle
    rUserInput.config_register_control("game_switch", false,
            {{0, (int) Key_t::V, VarTrig_t::PRESSED, false, VarOp_t::OR}});

    // Set UI Up/down/left/right to arrow keys. this is used to rotate the view
    // for now
    rUserInput.config_register_control("ui_up", true,
            {{osp::sc_keyboard, (int) Key_t::Up, VarTrig_t::PRESSED, false, VarOp_t::AND}});
    rUserInput.config_register_control("ui_dn", true,
            {{osp::sc_keyboard, (int) Key_t::Down, VarTrig_t::PRESSED, false, VarOp_t::AND}});
    rUserInput.config_register_control("ui_lf", true,
            {{osp::sc_keyboard, (int) Key_t::Left, VarTrig_t::PRESSED, false, VarOp_t::AND}});
    rUserInput.config_register_control("ui_rt", true,
            {{osp::sc_keyboard, (int) Key_t::Right, VarTrig_t::PRESSED, false, VarOp_t::AND}});

    rUserInput.config_register_control("ui_rmb", true,
            {{osp::sc_mouse, (int) Mouse_t::Right, VarTrig_t::PRESSED, false, VarOp_t::AND}});

    rUserInput.config_register_control("debug_planet_update", false,
            {{0, (int) Key_t::LeftCtrl, VarTrig_t::HOLD, false, VarOp_t::AND},
             {0, (int) Key_t::One, VarTrig_t::PRESSED, false, VarOp_t::OR}});
}
