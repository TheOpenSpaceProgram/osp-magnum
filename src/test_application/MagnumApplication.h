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

#include <Magnum/Timeline.h>

#include <cstring> // workaround: memcpy needed by SDL2
#include <Magnum/Platform/Sdl2Application.h>

#include <memory>

namespace testapp
{

class MagnumApplication;

class IOspApplication
{
public:
    virtual ~IOspApplication() = default;

    virtual void run(MagnumApplication& rApp) = 0;
    virtual void draw(MagnumApplication& rApp, float delta) = 0;
    virtual void exit(MagnumApplication& rApp) = 0;
};

/**
 * @brief An interactive Magnum application
 *
 * This is intended to run a flight scene, map view, vehicle editor, or menu.
 */
class MagnumApplication : Magnum::Platform::Application
{

public:

    using Magnum::Platform::Application::KeyEvent;
    using Magnum::Platform::Application::MouseEvent;
    using Magnum::Platform::Application::Arguments;

    using AppPtr_t = std::unique_ptr<IOspApplication>;

    explicit MagnumApplication(
            const Magnum::Platform::Application::Arguments& arguments,
            osp::input::UserInputHandler& rUserInput);

    ~MagnumApplication();

    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;

    void mousePressEvent(MouseEvent& event) override;
    void mouseReleaseEvent(MouseEvent& event) override;
    void mouseMoveEvent(MouseMoveEvent& event) override;
    void mouseScrollEvent(MouseScrollEvent& event) override;

    void exec()
    {
        m_ospApp->run(*this);
        Magnum::Platform::Application::exec();
        m_ospApp->exit(*this);
    }

    void exit()
    {
        Magnum::Platform::Application::exit();
    }

    void set_osp_app(AppPtr_t ospApp)
    {
        m_ospApp = std::move(ospApp);
    }

private:

    void drawEvent() override;

    AppPtr_t m_ospApp;

    osp::input::UserInputHandler &m_rUserInput;

    Magnum::Timeline m_timeline;

};

void config_controls(osp::input::UserInputHandler& rUserInput);

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
