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
#pragma once

#include "activescenes/scenarios.h"

#include <osp/types.h>
#include <osp/UserInputHandler.h>
#include <osp/Resource/Package.h>

#include <Magnum/Timeline.h>

#include <cstring> // workaround: memcpy needed by SDL2
#include <Magnum/Platform/Sdl2Application.h>

#include <functional>
#include <memory>

namespace testapp
{

/**
 * @brief An interactive Magnum application
 *
 * This is intended to run a flight scene, map view, vehicle editor, or menu.
 */
class ActiveApplication : public Magnum::Platform::Application
{

public:

    explicit ActiveApplication(
            const Magnum::Platform::Application::Arguments& arguments);

    ~ActiveApplication();

    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;

    void mousePressEvent(MouseEvent& event) override;
    void mouseReleaseEvent(MouseEvent& event) override;
    void mouseMoveEvent(MouseMoveEvent& event) override;
    void mouseScrollEvent(MouseScrollEvent& event) override;

    void set_on_draw(on_draw_t onDraw)
    {
        m_onDraw = std::move(onDraw);
    }

    constexpr osp::input::UserInputHandler& get_input_handler() noexcept
    {
        return m_userInput;
    }

    constexpr osp::Package& get_gl_resources() noexcept
    {
        return m_glResources;
    }

private:

    void drawEvent() override;

    on_draw_t m_onDraw;

    osp::input::UserInputHandler m_userInput;

    Magnum::Timeline m_timeline;

    osp::Package m_glResources;
};

void config_controls(ActiveApplication& rApp);

}

/**
* Parses the control string from the config file. 
* 
* A "None" input returns a empty vector.
* 
* @param Control string
* @returns vector of the control created from the string. 
*/
osp::input::ControlExprConfig_t parse_control(std::string_view str) noexcept;
