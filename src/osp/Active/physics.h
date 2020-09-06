#pragma once

#include "SysNewton.h"

namespace osp::active
{

// in case Newton Dynamics gets swapped out, one can implement a system class
// with all the same methods
using SysPhysics = SysNewton;

}
