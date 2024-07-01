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
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StridedArrayViewStl.h>
// IWYU pragma: end_exports

#include <iterator>

namespace osp
{

using Corrade::Containers::ArrayView;
using Corrade::Containers::arrayView;

using Corrade::Containers::arrayCast;

/**
 * @brief Wraps a Corrade ArrayView or StridedArrayView to use as a 2D array of equally sized rows
 */
template<typename T>
struct ArrayView2DWrapper
{
    // yes, I know that StridedArrayView2D exists, but ".row(...)" is more explicit

    constexpr T row(std::size_t const columnIndex) const noexcept
    {
        return view.sliceSize(columnIndex * rowSize, rowSize);
    }

    T view;
    std::size_t rowSize; ///< Size of each row, same as the number of columns
};

/**
 * @brief Returns an interface that treats an ArrayView as a 2D array of equally sized rows.
 *
 * This overload auto-converts ArrayView-compatible types.
 */
template<typename T>
    requires requires (T &view) { osp::arrayView(view); }
constexpr decltype(auto) as_2d(T &view, std::size_t rowSize) noexcept
{
    return ArrayView2DWrapper<decltype(osp::arrayView(view))>{ .view = osp::arrayView(view), .rowSize = rowSize };
}

/**
 * @brief Returns an interface that treats an ArrayView as a 2D array of equally sized rows.
 */
template<typename T>
constexpr ArrayView2DWrapper< ArrayView<T> > as_2d(ArrayView<T> view, std::size_t rowSize) noexcept
{
    return { .view = view, .rowSize = rowSize };
}

/**
 * @brief Returns an interface that treats an ArrayView as a 2D array of equally sized rows.
 */
template<typename T>
constexpr ArrayView2DWrapper< Corrade::Containers::StridedArrayView1D<T> >
        as_2d(Corrade::Containers::StridedArrayView1D<T> view, std::size_t rowSize) noexcept
{
    return { .view = view, .rowSize = rowSize };
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
