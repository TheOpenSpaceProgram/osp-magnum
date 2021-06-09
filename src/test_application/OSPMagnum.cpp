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
#include "osp/Active/SysHierarchy.h"

#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>

#include <iostream>

#include <toml.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

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

OSPMagnum::~OSPMagnum()
{
    // Clear scene data before GL resources are freed
    m_scenes.clear();
}

void OSPMagnum::drawEvent()
{

    Magnum::GL::defaultFramebuffer.clear(Magnum::GL::FramebufferClear::Color
                                         | Magnum::GL::FramebufferClear::Depth);

    m_userInput.update_controls();

    for (auto &[name, scene] : m_scenes)
    {
        scene.update();
    }

    m_userInput.clear_events();

    for (auto &[name, scene] : m_scenes)
    {
        scene.draw();
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
    auto const& [it, success] =
        m_scenes.try_emplace(name, m_ospApp, m_glResources);
    return it->second;
}

osp::active::ActiveScene& OSPMagnum::scene_create(std::string && name)
{
    auto const& [it, success] =
        m_scenes.try_emplace(std::move(name), m_ospApp, m_glResources);
    return it->second;
}

void testapp::config_controls(OSPMagnum& rOspApp)
{
    // Configure Controls
    //Load toml
    auto data = toml::parse("settings.toml");
    osp::UserInputHandler& rUserInput = rOspApp.get_input_handler();
    for (const auto& [k, v] : data.as_table())
    {
        std::string const& primary = toml::find(v, "primary").as_string();
        std::vector<osp::ButtonVarConfig> controls = parse_control(primary);

        std::string const& secondary = toml::find(v, "secondary").as_string();
        std::vector<osp::ButtonVarConfig> secondaryKeys = parse_control(secondary);

        controls.insert(controls.end(), std::make_move_iterator(secondaryKeys.begin()), std::make_move_iterator(secondaryKeys.end()));

        bool holdable = toml::find(v, "holdable").as_boolean();

        //Add user input
        rUserInput.config_register_control(k, holdable, std::move(controls));
    }
}


//Map for all the keys
//The tuple is in this order: device, number, and hold/pressed
using Key_t = OSPMagnum::KeyEvent::Key;
using Mouse_t = OSPMagnum::MouseEvent::Button;
using VarOp_t = osp::ButtonVarConfig::VarOperator;
using VarTrig_t = osp::ButtonVarConfig::VarTrigger;

typedef std::tuple<int, int> button_tuple;
const std::map<std::string_view, button_tuple, std::less<>> buttonMap = {
    //Keyboard
    {"LCtrl", {osp::sc_keyboard, (int)Key_t::LeftCtrl}},
    {"RCtrl", {osp::sc_keyboard, (int)Key_t::RightCtrl }},
    {"LShift", {osp::sc_keyboard, (int)Key_t::LeftShift }},
    {"RShift", {osp::sc_keyboard, (int)Key_t::RightShift }},
    {"LAlt", {osp::sc_keyboard, (int)Key_t::LeftAlt }},
    {"RAlt", {osp::sc_keyboard, (int)Key_t::RightAlt }},
    {"Up", {osp::sc_keyboard, (int)Key_t::Up }},
    {"Down", {osp::sc_keyboard, (int)Key_t::Down }},
    {"Left", {osp::sc_keyboard, (int)Key_t::Left }},
    {"Right", {osp::sc_keyboard, (int)Key_t::Right }},
    {"Esc", {osp::sc_keyboard, (int)Key_t::Esc  }},
    {"Tab", {osp::sc_keyboard, (int)Key_t::Tab  }},
    {"Space", {osp::sc_keyboard, (int)Key_t::Space }},
    {"Backspace", {osp::sc_keyboard, (int)Key_t::Backspace }},
    {"Backslash", {osp::sc_keyboard, (int)Key_t::Backslash  }},
    {"Comma", {osp::sc_keyboard, (int)Key_t::Comma  }},
    {"Delete", {osp::sc_keyboard, (int)Key_t::Delete }},
    {"Enter", {osp::sc_keyboard, (int)Key_t::Enter }},
    {"Equal", {osp::sc_keyboard, (int)Key_t::Equal }},
    {"Insert", {osp::sc_keyboard, (int)Key_t::Insert }},
    {"Slash", {osp::sc_keyboard, (int)Key_t::Slash }},

    //Alphabet keys
    {"A", {osp::sc_keyboard, (int)Key_t::A }},
    {"B", {osp::sc_keyboard, (int)Key_t::B }},
    {"C", {osp::sc_keyboard, (int)Key_t::C }},
    {"D", {osp::sc_keyboard, (int)Key_t::D  }},
    {"E", {osp::sc_keyboard, (int)Key_t::E  }},
    {"F", {osp::sc_keyboard, (int)Key_t::F  }},
    {"G", {osp::sc_keyboard, (int)Key_t::G  }},
    {"H", {osp::sc_keyboard, (int)Key_t::H  }},
    {"I", {osp::sc_keyboard, (int)Key_t::I  }},
    {"J", {osp::sc_keyboard, (int)Key_t::J  }},
    {"K", {osp::sc_keyboard, (int)Key_t::K  }},
    {"L", {osp::sc_keyboard, (int)Key_t::L  }},
    {"M", {osp::sc_keyboard, (int)Key_t::M  }},
    {"N", {osp::sc_keyboard, (int)Key_t::N  }},
    {"O", {osp::sc_keyboard, (int)Key_t::O  }},
    {"P", {osp::sc_keyboard, (int)Key_t::P  }},
    {"Q", {osp::sc_keyboard, (int)Key_t::Q  }},
    {"R", {osp::sc_keyboard, (int)Key_t::R  }},
    {"S", {osp::sc_keyboard, (int)Key_t::S  }},
    {"T", {osp::sc_keyboard, (int)Key_t::T  }},
    {"U", {osp::sc_keyboard, (int)Key_t::U  }},
    {"V", {osp::sc_keyboard, (int)Key_t::V  }},
    {"W", {osp::sc_keyboard, (int)Key_t::W  }},
    {"X", {osp::sc_keyboard, (int)Key_t::X  }},
    {"Y", {osp::sc_keyboard, (int)Key_t::Y  }},
    {"Z", {osp::sc_keyboard, (int)Key_t::Z  }},

    //Number keys
    {"0", {osp::sc_keyboard, (int)Key_t::NumZero  }},
    {"1", {osp::sc_keyboard, (int)Key_t::NumOne  }},
    {"2", {osp::sc_keyboard, (int)Key_t::NumTwo  }},
    {"3", {osp::sc_keyboard, (int)Key_t::NumThree  }},
    {"4", {osp::sc_keyboard, (int)Key_t::NumFour  }},
    {"5", {osp::sc_keyboard, (int)Key_t::NumFive  }},
    {"6", {osp::sc_keyboard, (int)Key_t::NumSix  }},
    {"7", {osp::sc_keyboard, (int)Key_t::NumSeven  }},
    {"8", {osp::sc_keyboard, (int)Key_t::NumEight  }},
    {"9", {osp::sc_keyboard, (int)Key_t::NumNine  }},

    //Function keys
    {"F1", {osp::sc_keyboard, (int)Key_t::F1  }},
    {"F2", {osp::sc_keyboard, (int)Key_t::F2  }},
    {"F3", {osp::sc_keyboard, (int)Key_t::F3  }},
    {"F4", {osp::sc_keyboard, (int)Key_t::F4  }},
    {"F5", {osp::sc_keyboard, (int)Key_t::F5  }},
    {"F6", {osp::sc_keyboard, (int)Key_t::F6  }},
    {"F7", {osp::sc_keyboard, (int)Key_t::F7  }},
    {"F8", {osp::sc_keyboard, (int)Key_t::F8  }},
    {"F9", {osp::sc_keyboard, (int)Key_t::F9  }},
    {"F10", {osp::sc_keyboard, (int)Key_t::F10  }},
    {"F11", {osp::sc_keyboard, (int)Key_t::F11  }},
    {"F12", {osp::sc_keyboard, (int)Key_t::F12  }},

    //Mouse
    {"RMouse", {osp::sc_mouse, (int)OSPMagnum::MouseEvent::Button::Right }},
    {"LMouse", {osp::sc_mouse, (int)OSPMagnum::MouseEvent::Button::Left }},
    {"MMouse", {osp::sc_mouse, (int)OSPMagnum::MouseEvent::Button::Middle }}
};

std::vector<osp::ButtonVarConfig> parse_control(std::string_view str) noexcept 
{
    std::vector<osp::ButtonVarConfig> handlers;

    //If none, then no actions
    if (str == "None") {
        return handlers;
    }

    static constexpr std::string_view delim = "+";

    auto start = 0U;
    auto end = str.find(delim);
    while (end != std::string::npos)
    {
        std::string_view sub = str.substr(start, end - start);
        auto const& it = buttonMap.find(sub);
        if (it != buttonMap.end()) 
        {
            auto const& [device, button] = it->second;
            handlers.emplace_back(osp::ButtonVarConfig(device, button, VarTrig_t::HOLD, false, VarOp_t::AND));
        }
        start = end + delim.length();
        end = str.find(delim, start);
    }

    std::string_view sub = str.substr(start, end);
    auto const& it = buttonMap.find(sub);
    if (it != buttonMap.end()) 
    {
        auto const& [device, button] = it->second;
        handlers.emplace_back(osp::ButtonVarConfig(device, button, VarTrig_t::PRESSED, false, VarOp_t::OR));
    }
    return handlers;
}
