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

#include <entt/entity/storage.hpp>

#include <Magnum/Math/Vector3.h>

#include <cstdint>

namespace osp::universe
{

using SatId     = uint32_t;
using CoSpaceId = uint32_t;

}

// Specialize entt::storage_traits to disable signals for storage that uses
// Satellites as entities
template<typename Type>
struct entt::storage_type<Type, osp::universe::SatId>
{
    using type = basic_storage<Type, osp::universe::SatId>;
};

namespace osp::universe
{

using spaceint_t = int64_t;

// 1024 space units = 1 meter
// TODO: this should vary by trajectory, but for now it's global
constexpr float gc_units_per_meter = 1024.0f;

// A Vector3 for space
using Vector3g = Magnum::Math::Vector3<spaceint_t>;

template<typename COMP_T>
using ucomp_storage_t = typename entt::storage_type<COMP_T, SatId>::type;

}
