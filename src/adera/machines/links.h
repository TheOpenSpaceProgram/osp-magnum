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
using osp::link::gc_ntSigFloat;
using osp::link::gc_sigIn;
using osp::link::gc_sigOut;

inline osp::link::MachTypeId const gc_mtUserCtrl    = osp::link::MachTypeReg_t::create();
inline osp::link::MachTypeId const gc_mtMagicRocket = osp::link::MachTypeReg_t::create();
inline osp::link::MachTypeId const gc_mtRcsDriver   = osp::link::MachTypeReg_t::create();

constexpr osp::Vector3 gc_rocketForward{0.0f, 0.0f, 1.0f};

namespace ports_userctrl
{
PortEntry const gc_throttleOut      { gc_ntSigFloat, 0, gc_sigOut };
PortEntry const gc_pitchOut         { gc_ntSigFloat, 1, gc_sigOut };
PortEntry const gc_yawOut           { gc_ntSigFloat, 2, gc_sigOut };
PortEntry const gc_rollOut          { gc_ntSigFloat, 3, gc_sigOut };
}

namespace ports_magicrocket
{
PortEntry const gc_throttleIn       { gc_ntSigFloat, 0, gc_sigIn };
PortEntry const gc_multiplierIn     { gc_ntSigFloat, 1, gc_sigIn };
}

namespace ports_rcsdriver
{
PortEntry const gc_posXIn           { gc_ntSigFloat, 0, gc_sigIn };
PortEntry const gc_posYIn           { gc_ntSigFloat, 1, gc_sigIn };
PortEntry const gc_posZIn           { gc_ntSigFloat, 2, gc_sigIn };
PortEntry const gc_dirXIn           { gc_ntSigFloat, 3, gc_sigIn };
PortEntry const gc_dirYIn           { gc_ntSigFloat, 4, gc_sigIn };
PortEntry const gc_dirZIn           { gc_ntSigFloat, 5, gc_sigIn };
PortEntry const gc_cmdLinXIn        { gc_ntSigFloat, 6, gc_sigIn };
PortEntry const gc_cmdLinYIn        { gc_ntSigFloat, 7, gc_sigIn };
PortEntry const gc_cmdLinZIn        { gc_ntSigFloat, 8, gc_sigIn };
PortEntry const gc_cmdAngXIn        { gc_ntSigFloat, 9, gc_sigIn };
PortEntry const gc_cmdAngYIn        { gc_ntSigFloat, 10, gc_sigIn };
PortEntry const gc_cmdAngZIn        { gc_ntSigFloat, 11, gc_sigIn };
PortEntry const gc_throttleOut      { gc_ntSigFloat, 12, gc_sigOut };
}

float thruster_influence(osp::Vector3 pos, osp::Vector3 dir, osp::Vector3 cmdLin, osp::Vector3 cmdAng) noexcept;

} // namespace adera
