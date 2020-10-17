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
#pragma once

#include "osp/types.h"
#include "osp/OSPApplication.h"
#include "osp/Universe.h"
#include "osp/UserInputHandler.h"
#include "osp/Satellites/SatActiveArea.h"
#include "osp/Active/ActiveScene.h"

#include <Magnum/Timeline.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Shaders/VertexColor.h>

#include <memory>

namespace osp
{

class OSPMagnum : public Magnum::Platform::Application
{

public:
    explicit OSPMagnum(
            const Magnum::Platform::Application::Arguments& arguments,
            OSPApplication &ospApp);

    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;

    void mousePressEvent(MouseEvent& event) override;
    void mouseReleaseEvent(MouseEvent& event) override;
    void mouseMoveEvent(MouseMoveEvent& event) override;
    void mouseScrollEvent(MouseScrollEvent& event) override;

    active::ActiveScene& scene_add(std::string const &name);

    constexpr UserInputHandler& get_input_handler() { return m_userInput; }
    constexpr std::map<std::string, active::ActiveScene>& get_scenes()
    { return m_scenes; }

private:

    void drawEvent() override;

    UserInputHandler m_userInput;

    std::map<std::string, active::ActiveScene> m_scenes;

    Magnum::Timeline m_timeline;

    OSPApplication& m_ospApp;

};



}
