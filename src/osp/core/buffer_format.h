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

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StridedArrayView.h>

namespace osp
{

/**
 * @brief Buffer Attribute Format. Describes how to access element attribute data within a buffer.
 *
 * This is useful for SIMD, GPU, and serialization. A SIMD n-body simulation may prefer [XXXYYYZZZ]
 * to store positions, but GPU mesh vertex positions prefer [XYZXYZXYZ...].
 */
template <typename T>
struct BufAttribFormat
{
    using View_t        = Corrade::Containers::StridedArrayView1D<T>;
    using ViewConst_t   = Corrade::Containers::StridedArrayView1D<T const>;
    using Data_t        = Corrade::Containers::ArrayView<std::byte>;
    using DataConst_t   = Corrade::Containers::ArrayView<std::byte const>;

    constexpr View_t view(Data_t data, std::size_t count) const noexcept
    {
        return stridedArrayView<T>(data, reinterpret_cast<T*>(&data[offset]), count, stride);
    }

    constexpr ViewConst_t view_const(DataConst_t data, std::size_t count) const noexcept
    {
        return stridedArrayView<T const>(data, reinterpret_cast<T const*>(&data[offset]), count, stride);
    }

    constexpr bool is_not_used() const noexcept { return stride == 0; }

    std::size_t     offset{};
    std::ptrdiff_t  stride{};
};

/**
 * @brief Builder to more easily create BufAttribFormats
 */
class BufferFormatBuilder
{
public:

    /**
     * @brief Insert a single contiguous block of attribute data
     *
     * To make the buffer format [XXXX... YYYYY... ZZZZZ...] for example, use:
     *
     * @code{.cpp}
     * builder.insert_block<float>(count); // X
     * builder.insert_block<float>(count); // Y
     * builder.insert_block<float>(count); // Z
     * @endcode
     *
     * @param count [in] Number of elements
     */
    template <typename T>
    constexpr BufAttribFormat<T> insert_block(std::size_t const count)
    {
        auto const prevbytesUsed = m_totalSize;
        m_totalSize += sizeof(T) * count;

        return { .offset = prevbytesUsed, .stride = sizeof(T) };
    }

    /**
     * @brief Insert interleaved attribute data
     *
     * To make the buffer format [XYZXYZXYZXYZ...] for example, use:
     *
     * @code{.cpp}
     * BufAttribFormat<float> attribX;
     * BufAttribFormat<float> attribY;
     * BufAttribFormat<float> attribZ;
     * builder.insert_interleave(count, attribX, attribY, attribZ);
     * @endcode
     *
     * @param count [in] Number of elements
     */
    template <typename ... T>
    constexpr void insert_interleave(std::size_t count, BufAttribFormat<T>& ... rInterleave)
    {
        constexpr std::size_t stride = (sizeof(T) + ...);

        (rInterleave.m_stride = ... = stride);

        interleave_aux(m_totalSize, rInterleave ...);

        m_totalSize += stride * count;
    }

    constexpr std::size_t total_size() const noexcept
    {
        return m_totalSize;
    }

private:

    template <typename FIRST_T, typename ... T>
    constexpr void interleave_aux(std::size_t const pos, BufAttribFormat<FIRST_T>& rInterleaveFirst, BufAttribFormat<T>& ... rInterleave)
    {
        rInterleaveFirst.m_offset = pos;

        if constexpr (sizeof...(T) != 0)
        {
            interleave_aux(pos + sizeof(FIRST_T), rInterleave ...);
        }
    }

    std::size_t m_totalSize{0};
};

} // namespace osp
