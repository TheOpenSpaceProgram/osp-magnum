/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

#include <osp/core/math_types.h>

#include <osp/link/machines.h>
#include <osp/link/signal.h>

namespace adera
{

using osp::link::PortEntry;
using osp::link::sigfloat_input;
using osp::link::sigfloat_output;

inline osp::link::MachTypeId const gc_mtUserCtrl    = osp::link::MachTypeReg_t::create();
inline osp::link::MachTypeId const gc_mtMagicRocket = osp::link::MachTypeReg_t::create();
inline osp::link::MachTypeId const gc_mtRcsDriver   = osp::link::MachTypeReg_t::create();

constexpr osp::Vector3 gc_rocketForward{0.0f, 0.0f, 1.0f};

namespace ports_userctrl
{
PortEntry const gc_throttleOut      = sigfloat_output(0);
PortEntry const gc_pitchOut         = sigfloat_output(1);
PortEntry const gc_yawOut           = sigfloat_output(2);
PortEntry const gc_rollOut          = sigfloat_output(3);
}

namespace ports_magicrocket
{
PortEntry const gc_throttleIn       = sigfloat_input(0);
PortEntry const gc_multiplierIn     = sigfloat_input(1);
}

namespace ports_rcsdriver
{
PortEntry const gc_posXIn           = sigfloat_input(0);
PortEntry const gc_posYIn           = sigfloat_input(1);
PortEntry const gc_posZIn           = sigfloat_input(2);
PortEntry const gc_dirXIn           = sigfloat_input(3);
PortEntry const gc_dirYIn           = sigfloat_input(4);
PortEntry const gc_dirZIn           = sigfloat_input(5);
PortEntry const gc_cmdLinXIn        = sigfloat_input(6);
PortEntry const gc_cmdLinYIn        = sigfloat_input(7);
PortEntry const gc_cmdLinZIn        = sigfloat_input(8);
PortEntry const gc_cmdAngXIn        = sigfloat_input(9);
PortEntry const gc_cmdAngYIn        = sigfloat_input(10);
PortEntry const gc_cmdAngZIn        = sigfloat_input(11);
PortEntry const gc_throttleOut      = sigfloat_output(12);
}

float thruster_influence(osp::Vector3 pos, osp::Vector3 dir, osp::Vector3 cmdLin, osp::Vector3 cmdAng) noexcept;

} // namespace adera
