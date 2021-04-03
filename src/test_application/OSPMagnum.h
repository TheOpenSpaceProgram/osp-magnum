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

#include <osp/types.h>
#include <osp/OSPApplication.h>
#include <osp/Universe.h>
#include <osp/UserInputHandler.h>
#include <osp/Satellites/SatActiveArea.h>
#include <osp/Active/ActiveScene.h>

#include <Magnum/Timeline.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/GlfwApplication.h>
#include <Magnum/Shaders/VertexColor.h>

#include <memory>

#include "osp/UserInputHandler.h"

namespace testapp
{

using MapActiveScene_t = std::map<std::string, osp::active::ActiveScene,
                                  std::less<> >;

class OSPMagnum : public Magnum::Platform::Application
{

public:
    explicit OSPMagnum(
            const Magnum::Platform::Application::Arguments& arguments,
            osp::OSPApplication &ospApp);

    ~OSPMagnum();

    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;

    void mousePressEvent(MouseEvent& event) override;
    void mouseReleaseEvent(MouseEvent& event) override;
    void mouseMoveEvent(MouseMoveEvent& event) override;
    void mouseScrollEvent(MouseScrollEvent& event) override;

    osp::active::ActiveScene& scene_create(std::string const& name);
    osp::active::ActiveScene& scene_create(std::string&& name);

    constexpr osp::UserInputHandler& get_input_handler() { return m_userInput; }
    constexpr MapActiveScene_t& get_scenes() { return m_scenes; }

private:

    void drawEvent() override;

    osp::UserInputHandler m_userInput;

    MapActiveScene_t m_scenes;

    osp::Package m_glResources{"gl", "gl-resources"};

    Magnum::Timeline m_timeline;

    osp::OSPApplication& m_ospApp;

};

void config_controls(OSPMagnum& rOspApp);

}

/**
* Parses the control string from the config file. 
* 
* A "None" input returns a empty vector.
* 
* @param Control string
* @returns vector of the control created from the string. 
*/
std::vector<osp::ButtonVarConfig> parse_control(std::string_view str) noexcept;
