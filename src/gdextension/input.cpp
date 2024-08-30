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

#include "input.h"

#include <godot_cpp/classes/global_constants.hpp>
#include <toml.hpp>


using namespace testapp;

using Key_t = godot::Key;
using Mouse_t = godot::MouseButton;

using osp::input::sc_keyboard;
using osp::input::sc_mouse;

using osp::input::UserInputHandler;
using osp::input::ControlTermConfig;
using osp::input::ControlExprConfig_t;
using osp::input::EVarTrigger;
using osp::input::EVarOperator;



// void MagnumApplication::keyPressEvent(KeyEvent& event)
// {
//     if (event.isRepeated()) { return; }
//     m_rUserInput.event_raw(osp::input::sc_keyboard, (int) event.key(),
//                            osp::input::EButtonEvent::Pressed);
// }

// void MagnumApplication::keyReleaseEvent(KeyEvent& event)
// {
//     if (event.isRepeated()) { return; }
//     m_rUserInput.event_raw(osp::input::sc_keyboard, (int) event.key(),
//                            osp::input::EButtonEvent::Released);
// }

// void MagnumApplication::mousePressEvent(MouseEvent& event)
// {
//     m_rUserInput.event_raw(osp::input::sc_mouse, (int) event.button(),
//                            osp::input::EButtonEvent::Pressed);
// }

// void MagnumApplication::mouseReleaseEvent(MouseEvent& event)
// {
//     m_rUserInput.event_raw(osp::input::sc_mouse, (int) event.button(),
//                            osp::input::EButtonEvent::Released);
// }

// void MagnumApplication::mouseMoveEvent(MouseMoveEvent& event)
// {
//     m_rUserInput.mouse_delta(event.relativePosition());
// }

// void MagnumApplication::mouseScrollEvent(MouseScrollEvent & event)
// {
//     m_rUserInput.scroll_delta(static_cast<osp::Vector2i>(event.offset()));
// }

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
    {"LCtrl", {sc_keyboard, (int)Key_t::KEY_CTRL}},
    {"RCtrl", {sc_keyboard, (int)Key_t::KEY_CTRL }},
    {"LShift", {sc_keyboard, (int)Key_t::KEY_SHIFT }},
    {"RShift", {sc_keyboard, (int)Key_t::KEY_SHIFT }},
    {"LAlt", {sc_keyboard, (int)Key_t::KEY_ALT }},
    {"RAlt", {sc_keyboard, (int)Key_t::KEY_ALT }},
    {"Up", {sc_keyboard, (int)Key_t::KEY_UP }},
    {"Down", {sc_keyboard, (int)Key_t::KEY_DOWN }},
    {"Left", {sc_keyboard, (int)Key_t::KEY_LEFT }},
    {"Right", {sc_keyboard, (int)Key_t::KEY_RIGHT }},
    {"Esc", {sc_keyboard, (int)Key_t::KEY_ESCAPE  }},
    {"Tab", {sc_keyboard, (int)Key_t::KEY_TAB  }},
    {"Space", {sc_keyboard, (int)Key_t::KEY_SPACE }},
    {"Backspace", {sc_keyboard, (int)Key_t::KEY_BACKSPACE }},
    {"Backslash", {sc_keyboard, (int)Key_t::KEY_BACKSLASH  }},
    {"Comma", {sc_keyboard, (int)Key_t::KEY_COMMA  }},
    {"Delete", {sc_keyboard, (int)Key_t::KEY_DELETE }},
    {"Enter", {sc_keyboard, (int)Key_t::KEY_ENTER }},
    {"Equal", {sc_keyboard, (int)Key_t::KEY_EQUAL }},
    {"Insert", {sc_keyboard, (int)Key_t::KEY_INSERT }},
    {"Slash", {sc_keyboard, (int)Key_t::KEY_SLASH }},

    //Alphabet keys
    {"A", {sc_keyboard, (int)Key_t::KEY_A }},
    {"B", {sc_keyboard, (int)Key_t::KEY_B }},
    {"C", {sc_keyboard, (int)Key_t::KEY_C }},
    {"D", {sc_keyboard, (int)Key_t::KEY_D  }},
    {"E", {sc_keyboard, (int)Key_t::KEY_E  }},
    {"F", {sc_keyboard, (int)Key_t::KEY_F  }},
    {"G", {sc_keyboard, (int)Key_t::KEY_G  }},
    {"H", {sc_keyboard, (int)Key_t::KEY_H  }},
    {"I", {sc_keyboard, (int)Key_t::KEY_I  }},
    {"J", {sc_keyboard, (int)Key_t::KEY_J  }},
    {"K", {sc_keyboard, (int)Key_t::KEY_K  }},
    {"L", {sc_keyboard, (int)Key_t::KEY_L  }},
    {"M", {sc_keyboard, (int)Key_t::KEY_M  }},
    {"N", {sc_keyboard, (int)Key_t::KEY_N  }},
    {"O", {sc_keyboard, (int)Key_t::KEY_O  }},
    {"P", {sc_keyboard, (int)Key_t::KEY_P  }},
    {"Q", {sc_keyboard, (int)Key_t::KEY_Q  }},
    {"R", {sc_keyboard, (int)Key_t::KEY_R  }},
    {"S", {sc_keyboard, (int)Key_t::KEY_S  }},
    {"T", {sc_keyboard, (int)Key_t::KEY_T  }},
    {"U", {sc_keyboard, (int)Key_t::KEY_U  }},
    {"V", {sc_keyboard, (int)Key_t::KEY_V  }},
    {"W", {sc_keyboard, (int)Key_t::KEY_W  }},
    {"X", {sc_keyboard, (int)Key_t::KEY_X  }},
    {"Y", {sc_keyboard, (int)Key_t::KEY_Y  }},
    {"Z", {sc_keyboard, (int)Key_t::KEY_Z  }},

    //Number keys
    {"0", {sc_keyboard, (int)Key_t::KEY_0  }},
    {"1", {sc_keyboard, (int)Key_t::KEY_1  }},
    {"2", {sc_keyboard, (int)Key_t::KEY_2  }},
    {"3", {sc_keyboard, (int)Key_t::KEY_3  }},
    {"4", {sc_keyboard, (int)Key_t::KEY_4  }},
    {"5", {sc_keyboard, (int)Key_t::KEY_5  }},
    {"6", {sc_keyboard, (int)Key_t::KEY_6  }},
    {"7", {sc_keyboard, (int)Key_t::KEY_7  }},
    {"8", {sc_keyboard, (int)Key_t::KEY_8  }},
    {"9", {sc_keyboard, (int)Key_t::KEY_9  }},

    //Function keys
    {"F1", {sc_keyboard, (int)Key_t::KEY_F1  }},
    {"F2", {sc_keyboard, (int)Key_t::KEY_F2  }},
    {"F3", {sc_keyboard, (int)Key_t::KEY_F3  }},
    {"F4", {sc_keyboard, (int)Key_t::KEY_F4  }},
    {"F5", {sc_keyboard, (int)Key_t::KEY_F5  }},
    {"F6", {sc_keyboard, (int)Key_t::KEY_F6  }},
    {"F7", {sc_keyboard, (int)Key_t::KEY_F7  }},
    {"F8", {sc_keyboard, (int)Key_t::KEY_F8  }},
    {"F9", {sc_keyboard, (int)Key_t::KEY_F9  }},
    {"F10", {sc_keyboard, (int)Key_t::KEY_F10  }},
    {"F11", {sc_keyboard, (int)Key_t::KEY_F11  }},
    {"F12", {sc_keyboard, (int)Key_t::KEY_F12  }},

    //Mouse
    {"RMouse", {sc_mouse, (int)Mouse_t::MOUSE_BUTTON_RIGHT }},
    {"LMouse", {sc_mouse, (int)Mouse_t::MOUSE_BUTTON_LEFT }},
    {"MMouse", {sc_mouse, (int)Mouse_t::MOUSE_BUTTON_MIDDLE }}
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
