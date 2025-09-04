/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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

#include "../core/strong_id.h"
#include "../core/math_types.h"

#include <longeron/id_management/refcount.hpp>

#include <Magnum/Math/Vector3.h>

#include <cstdint>

namespace osp::universe
{

// Universe consists of several types of 'global' instances:
// * Simulations
// * CoordinateSpaces
// * DataAccessors
// * Components
// * DataSources
// * SatelliteSets

using SimulationId      = osp::StrongId<std::uint32_t, struct DummyForSimulationId>;

using CoSpaceId         = osp::StrongId<std::uint32_t, struct DummyForCoSpaceId>;
using CoSpaceOwner      = lgrn::IdRefCount<CoSpaceId>::Owner_t;

using ComponentTypeId   = osp::StrongId<std::uint32_t, struct DummyForComponentTypeId>;
using DataAccessorId    = osp::StrongId<std::uint32_t, struct DummyForDataAccessorId>;
using DataAccessorOwner = lgrn::IdRefCount<DataAccessorId>::Owner_t;

using DataSourceId      = osp::StrongId<std::uint32_t, struct DummyForDataSourceId>;
using DataSourceOwner   = lgrn::IdRefCount<DataSourceId>::Owner_t;

using SatelliteId       = osp::StrongId<std::uint32_t, struct DummyForSatId>;

using IntakeId          = osp::StrongId<std::uint32_t, struct DummyForIntakeId>;


using spaceint_t = std::int64_t;

// A Vector3 for space
using Vector3g = Magnum::Math::Vector3<spaceint_t>;


}
