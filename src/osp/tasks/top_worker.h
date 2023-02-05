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

#include <Corrade/Containers/ArrayView.h>

#include <entt/core/fwd.hpp>

#include <cstdint>

namespace osp
{

using bit_int_t = uint64_t;

using Corrade::Containers::ArrayView;

using TopDataId     = uint32_t;
using TopDataIds_t  = std::initializer_list<TopDataId>;

struct Reserved {};

struct WorkerContext
{
    struct LimitSlot
    {
        uint32_t m_tag;
        int m_slot;
    };

    Corrade::Containers::ArrayView<LimitSlot> m_limitSlots;
    Corrade::Containers::ArrayView<bit_int_t> m_enqueue;

    bool & m_rEnqueueHappened;
};

enum class TopTaskStatus : uint8_t
{
    Success     = 0,
    Failed      = 1,
    DidNothing  = 2
};

using TopTaskFunc_t = TopTaskStatus(*)(WorkerContext, ArrayView<entt::any>) noexcept;

} // namespace osp
