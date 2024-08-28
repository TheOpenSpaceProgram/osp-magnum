/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "MagnumWindowApp.h"

#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>

#include <toml.hpp>


using namespace testapp;

using Key_t = MagnumWindowApp::KeyEvent::Key;
using Mouse_t = MagnumWindowApp::MouseEvent::Button;

using osp::input::sc_keyboard;
using osp::input::sc_mouse;

using osp::input::UserInputHandler;
using osp::input::ControlTermConfig;
using osp::input::ControlExprConfig_t;
using osp::input::EVarTrigger;
using osp::input::EVarOperator;

using Magnum::Platform::Application;

MagnumWindowApp::MagnumWindowApp(const Application::Arguments& arguments,
                                     UserInputHandler& rUserInput)
 : Application{arguments, Configuration{}.setTitle("OSP-Magnum").setSize({1280, 720})}
 , m_rUserInput(rUserInput)
{
    // temporary fixed 60fps. No physics interpolation or anything is implemented yet
    setSwapInterval(1);
    setMinimalLoopPeriod(16);
    m_timeline.start();
}

void MagnumWindowApp::drawEvent()
{
    m_rUserInput.update_controls();

    if (m_events != nullptr)
    {
        m_events->draw(*this, m_timeline.previousFrameDuration());
    }

    m_rUserInput.clear_events();

    swapBuffers();
    m_timeline.nextFrame();
    redraw();
}

void MagnumWindowApp::keyPressEvent(KeyEvent& event)
{
    if (event.isRepeated()) { return; }
    m_rUserInput.event_raw(osp::input::sc_keyboard, (int) event.key(),
                           osp::input::EButtonEvent::Pressed);
}

void MagnumWindowApp::keyReleaseEvent(KeyEvent& event)
{
    if (event.isRepeated()) { return; }
    m_rUserInput.event_raw(osp::input::sc_keyboard, (int) event.key(),
                           osp::input::EButtonEvent::Released);
}

void MagnumWindowApp::mousePressEvent(MouseEvent& event)
{
    m_rUserInput.event_raw(osp::input::sc_mouse, (int) event.button(),
                           osp::input::EButtonEvent::Pressed);
}

void MagnumWindowApp::mouseReleaseEvent(MouseEvent& event)
{
    m_rUserInput.event_raw(osp::input::sc_mouse, (int) event.button(),
                           osp::input::EButtonEvent::Released);
}

void MagnumWindowApp::mouseMoveEvent(MouseMoveEvent& event)
{
    m_rUserInput.mouse_delta(event.relativePosition());
}

void MagnumWindowApp::mouseScrollEvent(MouseScrollEvent & event)
{
    m_rUserInput.scroll_delta(static_cast<osp::Vector2i>(event.offset()));
}

void testapp::config_controls(UserInputHandler& rUserInput)
{
    // Configure Controls
    //Load toml
    auto data = toml::parse("settings.toml");
    for (const auto& [k, v] : data.as_table())
    {
        std::string const& primary = toml::find(v, "primary").as_string();
        ControlExprConfig_t controls = parse_control(primary);

        std::string const& secondary = toml::find(v, "secondary").as_string();
        ControlExprConfig_t secondaryKeys = parse_control(secondary);

        controls.insert(controls.end(), std::make_move_iterator(secondaryKeys.begin()), std::make_move_iterator(secondaryKeys.end()));

        bool holdable = toml::find(v, "holdable").as_boolean();

        //Add user input
        rUserInput.config_register_control(k, holdable, std::move(controls));
    }
}

// Map for all the keys

// Pair holds device and enum
using button_pair_t = std::pair<int, int>;
const std::map<std::string_view, button_pair_t, std::less<>> gc_buttonMap = {
    //Keyboard
    {"LCtrl", {sc_keyboard, (int)Key_t::LeftCtrl}},
    {"RCtrl", {sc_keyboard, (int)Key_t::RightCtrl }},
    {"LShift", {sc_keyboard, (int)Key_t::LeftShift }},
    {"RShift", {sc_keyboard, (int)Key_t::RightShift }},
    {"LAlt", {sc_keyboard, (int)Key_t::LeftAlt }},
    {"RAlt", {sc_keyboard, (int)Key_t::RightAlt }},
    {"Up", {sc_keyboard, (int)Key_t::Up }},
    {"Down", {sc_keyboard, (int)Key_t::Down }},
    {"Left", {sc_keyboard, (int)Key_t::Left }},
    {"Right", {sc_keyboard, (int)Key_t::Right }},
    {"Esc", {sc_keyboard, (int)Key_t::Esc  }},
    {"Tab", {sc_keyboard, (int)Key_t::Tab  }},
    {"Space", {sc_keyboard, (int)Key_t::Space }},
    {"Backspace", {sc_keyboard, (int)Key_t::Backspace }},
    {"Backslash", {sc_keyboard, (int)Key_t::Backslash  }},
    {"Comma", {sc_keyboard, (int)Key_t::Comma  }},
    {"Delete", {sc_keyboard, (int)Key_t::Delete }},
    {"Enter", {sc_keyboard, (int)Key_t::Enter }},
    {"Equal", {sc_keyboard, (int)Key_t::Equal }},
    {"Insert", {sc_keyboard, (int)Key_t::Insert }},
    {"Slash", {sc_keyboard, (int)Key_t::Slash }},

    //Alphabet keys
    {"A", {sc_keyboard, (int)Key_t::A }},
    {"B", {sc_keyboard, (int)Key_t::B }},
    {"C", {sc_keyboard, (int)Key_t::C }},
    {"D", {sc_keyboard, (int)Key_t::D  }},
    {"E", {sc_keyboard, (int)Key_t::E  }},
    {"F", {sc_keyboard, (int)Key_t::F  }},
    {"G", {sc_keyboard, (int)Key_t::G  }},
    {"H", {sc_keyboard, (int)Key_t::H  }},
    {"I", {sc_keyboard, (int)Key_t::I  }},
    {"J", {sc_keyboard, (int)Key_t::J  }},
    {"K", {sc_keyboard, (int)Key_t::K  }},
    {"L", {sc_keyboard, (int)Key_t::L  }},
    {"M", {sc_keyboard, (int)Key_t::M  }},
    {"N", {sc_keyboard, (int)Key_t::N  }},
    {"O", {sc_keyboard, (int)Key_t::O  }},
    {"P", {sc_keyboard, (int)Key_t::P  }},
    {"Q", {sc_keyboard, (int)Key_t::Q  }},
    {"R", {sc_keyboard, (int)Key_t::R  }},
    {"S", {sc_keyboard, (int)Key_t::S  }},
    {"T", {sc_keyboard, (int)Key_t::T  }},
    {"U", {sc_keyboard, (int)Key_t::U  }},
    {"V", {sc_keyboard, (int)Key_t::V  }},
    {"W", {sc_keyboard, (int)Key_t::W  }},
    {"X", {sc_keyboard, (int)Key_t::X  }},
    {"Y", {sc_keyboard, (int)Key_t::Y  }},
    {"Z", {sc_keyboard, (int)Key_t::Z  }},

    //Number keys
    {"0", {sc_keyboard, (int)Key_t::NumZero  }},
    {"1", {sc_keyboard, (int)Key_t::NumOne  }},
    {"2", {sc_keyboard, (int)Key_t::NumTwo  }},
    {"3", {sc_keyboard, (int)Key_t::NumThree  }},
    {"4", {sc_keyboard, (int)Key_t::NumFour  }},
    {"5", {sc_keyboard, (int)Key_t::NumFive  }},
    {"6", {sc_keyboard, (int)Key_t::NumSix  }},
    {"7", {sc_keyboard, (int)Key_t::NumSeven  }},
    {"8", {sc_keyboard, (int)Key_t::NumEight  }},
    {"9", {sc_keyboard, (int)Key_t::NumNine  }},

    //Function keys
    {"F1", {sc_keyboard, (int)Key_t::F1  }},
    {"F2", {sc_keyboard, (int)Key_t::F2  }},
    {"F3", {sc_keyboard, (int)Key_t::F3  }},
    {"F4", {sc_keyboard, (int)Key_t::F4  }},
    {"F5", {sc_keyboard, (int)Key_t::F5  }},
    {"F6", {sc_keyboard, (int)Key_t::F6  }},
    {"F7", {sc_keyboard, (int)Key_t::F7  }},
    {"F8", {sc_keyboard, (int)Key_t::F8  }},
    {"F9", {sc_keyboard, (int)Key_t::F9  }},
    {"F10", {sc_keyboard, (int)Key_t::F10  }},
    {"F11", {sc_keyboard, (int)Key_t::F11  }},
    {"F12", {sc_keyboard, (int)Key_t::F12  }},

    //Mouse
    {"RMouse", {sc_mouse, (int)MagnumWindowApp::MouseEvent::Button::Right }},
    {"LMouse", {sc_mouse, (int)MagnumWindowApp::MouseEvent::Button::Left }},
    {"MMouse", {sc_mouse, (int)MagnumWindowApp::MouseEvent::Button::Middle }}
};

ControlExprConfig_t parse_control(std::string_view str) noexcept
{
    ControlExprConfig_t handlers;

    //If none, then no actions
    if (str == "None") {
        return handlers;
    }

    static constexpr std::string_view delim = "+";

    auto start = 0U;
    auto end = str.find(delim);
    while (end != std::string::npos)
    {
        std::string_view const sub = str.substr(start, end - start);

        if (auto const& it = gc_buttonMap.find(sub);
            it != gc_buttonMap.end())
        {
            auto const& [device, button] = it->second;
            handlers.emplace_back(ControlTermConfig{
                device, button, EVarTrigger::Hold, EVarOperator::And, false});
        }
        start = end + delim.length();
        end = str.find(delim, start);
    }

    std::string_view const sub = str.substr(start, end);

    if (auto const& it = gc_buttonMap.find(sub);
        it != gc_buttonMap.end())
    {
        auto const& [device, button] = it->second;
        handlers.emplace_back(ControlTermConfig{
            device, button, EVarTrigger::Pressed, EVarOperator::Or, false});
    }
    return handlers;
}
