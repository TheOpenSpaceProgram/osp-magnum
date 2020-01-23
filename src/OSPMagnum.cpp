#include <iostream>

#include "OSPMagnum.h"


namespace osp
{

OSPMagnum::OSPMagnum(const Magnum::Platform::Application::Arguments& arguments):
    Magnum::Platform::Application{
        arguments,
        Configuration{}.setTitle("OSP-Magnum").setSize({1280, 720})}
{
    //.setWindowFlags(Configuration::WindowFlag::Hidden)
}


void OSPMagnum::drawEvent()
{
    Magnum::GL::defaultFramebuffer.clear(Magnum::GL::FramebufferClear::Color);

    swapBuffers();
}

}
