#include <iostream>


#include "Rocket.h"

namespace osp
{

MachineRocket::MachineRocket()
{

}

MachineRocket::MachineRocket(MachineRocket&& move)
{

}

void MachineRocket::propagate_output(WireOutput* output)
{

}

WireInput* MachineRocket::request_input(WireInPort port)
{
    return existing_inputs()[port];
}

WireOutput* MachineRocket::request_output(WireOutPort port)
{
    return existing_outputs()[port];
}

std::vector<WireInput*> MachineRocket::existing_inputs()
{
    return {};
}

std::vector<WireOutput*> MachineRocket::existing_outputs()
{
    return {};
}

void SysMachineRocket::update_sensor()
{
}

void SysMachineRocket::update_physics()
{
    //std::cout << "foo\n";
}

}
