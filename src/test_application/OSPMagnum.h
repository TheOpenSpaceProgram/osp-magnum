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
