/**
 * Open Space Program
 * Copyright © 2019-2025 Open Space Program Project
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

/**
 * @file
 * @brief Data required to glue/interface simulations.h into OSP universe
 */
#include "simulations.h"

#include <osp/universe/universetypes.h>

namespace adera {

using CirclePathSimId   = osp::StrongId< std::uint32_t, struct DummyForCirclePathSimId >;
using ConstantSpinSimId = osp::StrongId< std::uint32_t, struct DummyForConstantSpinSimId >;
using SimpleGravitySimId = osp::StrongId< std::uint32_t, struct DummyForSimpleGravitySimId >;

// These simulators only have one buffer, and only need one DataAccessorId.
// More complicated simulators can use multiple buffers (buckets to store data) and accessors.

struct UCtxCirclePathSims
{
    struct Instance
    {
        osp::universe::SimulationId     simId;
        CirclePathSim                   sim;
        std::int64_t                    updateInterval{};
        osp::universe::DataAccessorId   accessorId;
        osp::universe::CoSpaceId        cospaceId;
        osp::universe::IntakeId         intakeId;
    };

    lgrn::IdRegistryStl<CirclePathSimId>        ids;
    osp::KeyedVec<CirclePathSimId, Instance>    instOf;
};

struct UCtxConstantSpinSims
{
    struct Instance
    {
        ConstantSpinSim                 sim;
        osp::universe::SimulationId     simId;
        std::int64_t                    updateInterval{};
        osp::universe::DataAccessorId   accessorId;
        osp::universe::CoSpaceId        cospaceId;
        osp::universe::IntakeId         intakeId;
    };

    lgrn::IdRegistryStl<ConstantSpinSimId>      ids;
    osp::KeyedVec<ConstantSpinSimId, Instance>  instOf;
};

struct UCtxSimpleGravitySims
{
    struct Instance
    {
        SimpleGravitySim                sim;
        osp::universe::SimulationId     simId;
        std::int64_t                    updateInterval{};
        osp::universe::DataAccessorId   accessorId;
        osp::universe::CoSpaceId        cospaceId;
        osp::universe::IntakeId         intakeId;
    };

    lgrn::IdRegistryStl<SimpleGravitySimId>         ids;
    osp::KeyedVec<SimpleGravitySimId, Instance>     instOf;
};



}
