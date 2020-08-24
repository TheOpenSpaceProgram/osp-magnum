#include <iostream>

#include <Magnum/GL/DefaultFramebuffer.h>

#include "SatActiveArea.h"

#include "../Active/ActiveScene.h"
#include "../Active/SysNewton.h"
#include "../Active/SysMachine.h"

#include "../Universe.h"


// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using Magnum::Matrix4;

namespace osp::universe
{

SatActiveArea::SatActiveArea(Universe& universe) :
        CommonTypeSat<SatActiveArea, ucomp::ActiveArea>(universe)
{

}


}
