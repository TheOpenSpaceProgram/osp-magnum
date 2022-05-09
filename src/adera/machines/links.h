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

#include <osp/link/machines.h>
#include <osp/link/signal.h>

namespace adera
{

extern osp::link::MachTypeId const gc_mtUserCtrl;
extern osp::link::MachTypeId const gc_mtMagicRocket;

namespace ports_userctrl
{
using osp::link::PortEntry;
using osp::link::gc_ntNumber;
using osp::link::gc_sigOut;

PortEntry const gc_throttleOut      { gc_ntNumber, 0, gc_sigOut };
PortEntry const gc_pitchOut         { gc_ntNumber, 1, gc_sigOut };
PortEntry const gc_yawOut           { gc_ntNumber, 2, gc_sigOut };
PortEntry const gc_rollOut          { gc_ntNumber, 3, gc_sigOut };
}

namespace ports_magicrocket
{
using osp::link::PortEntry;
using osp::link::gc_ntNumber;
using osp::link::gc_sigIn;

PortEntry const gc_throttleIn       { gc_ntNumber, 0, gc_sigIn };
}

} // namespace adera
