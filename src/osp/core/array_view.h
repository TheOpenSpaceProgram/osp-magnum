/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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

// IWYU pragma: begin_exports
#include <Corrade/Containers/ArrayView.h>
// IWYU pragma: end_exports

#include <iterator>

namespace osp
{

using Corrade::Containers::ArrayView;
using Corrade::Containers::arrayView;

template<typename T>
struct ArrayView2DWrapper
{
    constexpr ArrayView<T> row(std::size_t const index) const noexcept
    {
        return view.sliceSize(index * columns, columns);
    }

    ArrayView<T> view;
    std::size_t columns;
};

template<typename T>
constexpr ArrayView2DWrapper<T> as_2d(ArrayView<T> view, std::size_t columns) noexcept
{
    return { .view = view, .columns = columns };
}

/**
 * @brief slice_2d_row
 * @param view
 * @param index
 * @param size
 */
template<typename T>
ArrayView<T> slice_2d_row(ArrayView<T> const& view, std::size_t index, std::size_t size) noexcept
{
    return view.sliceSize(index, size);
}

/**
 * @brief Anger the address sanitizer for invalid array views / spans / similar containers
 */
template<typename T>
void debug_touch_container(T const& container)
{
    // This feels kinda stupid, but if you're reading this and have a better idea then go ahead
    // and change it. (asan annotations?)
    auto const* ptrA = reinterpret_cast<unsigned char const*>(std::addressof(*container.begin()));
    auto const* ptrB = reinterpret_cast<unsigned char const*>(std::addressof((std::size(container) == 0) ? *container.begin() : *std::prev(container.end())));

    [[maybe_unused]] char dummy = *ptrA + *ptrB;
}

} // namespace osp
