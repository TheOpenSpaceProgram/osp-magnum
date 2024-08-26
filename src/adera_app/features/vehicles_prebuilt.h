/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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

#include <osp/framework/builder.h>

#include <adera/activescene/VehicleBuilder.h>

#include <osp/core/copymove_macros.h>
#include <osp/core/keyed_vector.h>
#include <osp/core/global_id.h>
#include <osp/core/strong_id.h>

#include <memory>

namespace adera
{

using PrebuiltVhId          = osp::StrongId<uint32_t, struct DummyForPBV>;
using PrebuiltVhIdReg_t     = osp::GlobalIdReg<PrebuiltVhId>;

struct PrebuiltVehicles : osp::KeyedVec< PrebuiltVhId, std::unique_ptr<adera::VehicleData> >
{
    PrebuiltVehicles() = default;
    OSP_MOVE_ONLY_CTOR_ASSIGN(PrebuiltVehicles);
};

inline PrebuiltVhId const gc_pbvSimpleCommandServiceModule = PrebuiltVhIdReg_t::create();

extern osp::fw::FeatureDef const ftrPrebuiltVehicles;

} // namespace adera

