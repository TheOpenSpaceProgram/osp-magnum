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

#include "../types.h"

#include <entt/core/fwd.hpp>

#include <longeron/containers/bit_view.hpp>

#include <array>
#include <cstdint>

namespace osp
{

/**
 * @brief Return value from a task to tell the executor which fulfilled targets are dirty
 *
 * Bits here limits the max number of fulfill targets per-task to 1*64
 */
using FulfillDirty_t = lgrn::BitView<std::array<uint64_t, 1>>;

/**
 * @brief Passed to tasks to signify which targets the task depends on are dirty
 *
 * Bits here limits the max dependencies per-task to 4*64
 */
//using DependOnDirty_t = lgrn::BitView<std::array<uint64_t, 4>>;

constexpr FulfillDirty_t gc_fulfillAll{{0xFFFFFFFFFFFFFFFFul}};
constexpr FulfillDirty_t gc_fulfillNone{{0}};

using TopDataId     = uint32_t;
using TopDataIds_t  = std::initializer_list<TopDataId>;

struct Reserved {};

struct WorkerContext
{
    //DependOnDirty_t m_dependOnDirty;
};

using TopTaskFunc_t = FulfillDirty_t(*)(WorkerContext, ArrayView<entt::any>) noexcept;

} // namespace osp
