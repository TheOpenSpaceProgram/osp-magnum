#pragma once

#include <Magnum/Timeline.h>

#include <Magnum/GL/Buffer.h>

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Shaders/VertexColor.h>

#include <memory>

#include "osp/Satellites/SatActiveArea.h"

#include "types.h"

#include "osp/Universe.h"
#include "osp/UserInputHandler.h"

namespace osp
{

class OSPMagnum: public Magnum::Platform::Application
{

public:
    explicit OSPMagnum(const Magnum::Platform::Application::Arguments& arguments);

    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;

    void mousePressEvent(MouseEvent& event) override;
    void mouseReleaseEvent(MouseEvent& event) override;
    void mouseMoveEvent(MouseMoveEvent& event) override;

    void set_active_area(SatActiveArea& area);

    UserInputHandler& get_input_handler() { return m_userInput; }


private:

    // TODO: not sure how to safely access m_area.
    //       SatelliteObjects are stored with unique_ptr in Satellite
    //       might have to used shared_ptr unless there's a better way
    SatActiveArea* m_area;
    UserInputHandler m_userInput;
    //std::weak_ptr<SatActiveArea> m_area;


    Magnum::Timeline m_timeline;

    void drawEvent() override;

};



}
