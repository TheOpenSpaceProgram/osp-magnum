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

#include "../core/math_types.h"
#include "universetypes.h"

#include <longeron/id_management/registry_stl.hpp>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StridedArrayView.h>

#include <array>
#include <cstdint>

namespace osp::universe
{

struct StrideDesc
{
    constexpr bool not_used() const noexcept { return m_stride == 0; }

    std::size_t     m_offset{0};
    std::ptrdiff_t  m_stride{0};
};

/**
 * @brief Describes strided data within an externally stored buffer
 */
template <typename T>
struct TypedStrideDesc : StrideDesc
{
    using View_t        = Corrade::Containers::StridedArrayView1D<T>;
    using ViewConst_t   = Corrade::Containers::StridedArrayView1D<T const>;
    using Data_t        = Corrade::Containers::ArrayView<unsigned char>;
    using DataConst_t   = Corrade::Containers::ArrayView<unsigned char const>;

    constexpr View_t view(Data_t data, std::size_t count) const noexcept
    {
        return stridedArrayView<T>(data, reinterpret_cast<T*>(&data[m_offset]), count, m_stride);
    }

    constexpr ViewConst_t view(DataConst_t data, std::size_t count) const noexcept
    {
        return stridedArrayView<T>(data, reinterpret_cast<T const*>(&data[m_offset]), count, m_stride);
    }
};

template <typename T, std::size_t N>
using StrideDescArray_t = std::array<TypedStrideDesc<T>, N>;

struct CoSpaceHierarchy
{
    CoSpaceId       m_parent{lgrn::id_null<CoSpaceId>()};
    SatId           m_parentSat{lgrn::id_null<SatId>()};
};

struct CoSpaceTransform
{
    // Position and rotation is relative to m_parent
    // Ignore and use m_parentSat's position and rotation instead if non-null
    Quaterniond     m_rotation;
    Vector3g        m_position;

    int             m_precision{10}; // 1 meter = 2^m_precision
};

struct CoSpaceSatData
{
    uint32_t        m_satCount;
    uint32_t        m_satCapacity;

    Corrade::Containers::Array<unsigned char>   m_data;

    // Describes m_data
    StrideDescArray_t<spaceint_t, 3>            m_satPositions;
    StrideDescArray_t<double, 3>                m_satVelocities;
    StrideDescArray_t<double, 4>                m_satRotations;
};


struct CoSpaceCommon : CoSpaceTransform, CoSpaceHierarchy, CoSpaceSatData { };

struct Universe
{
    Universe() = default;
    Universe(Universe const&) = delete;
    Universe(Universe&& move) = default;

    lgrn::IdRegistryStl<CoSpaceId>   m_coordIds;

    std::vector<CoSpaceCommon>       m_coordCommon;
};

struct SceneFrame : CoSpaceTransform, CoSpaceHierarchy
{
    Vector3g m_scenePosition;
};

template <typename FIRST_T, typename ... T>
constexpr void aux_partition(std::size_t const pos, TypedStrideDesc<FIRST_T>& rInterleveFirst, TypedStrideDesc<T>& ... rInterleve)
{
    rInterleveFirst.m_offset = pos;

    if constexpr (sizeof...(T) != 0)
    {
        aux_partition(pos + sizeof(FIRST_T), rInterleve ...);
    }
}

template <typename ... T>
constexpr void partition(std::size_t& rPos, std::size_t count, TypedStrideDesc<T>& ... rInterleve)
{
    constexpr std::size_t stride = (sizeof(T) + ...);

    (rInterleve.m_stride = ... = stride);

    aux_partition(rPos, rInterleve ...);

    rPos += stride * count;
}

// INDEX_T is a template parameter to allow passing in "strong typedef" types,
// like enum classes and having them converted without warning to size_t.
// This is a limitation of the enum class feature in C++, in that
// it cant have an implicit conversion operator.
template <typename VEC_T, typename INDEX_T, typename ... RANGE_T>
constexpr VEC_T to_vec(INDEX_T i, RANGE_T&& ... args) noexcept
{
    return {std::forward<RANGE_T>(args)[std::size_t(i)] ...};
}


/**
 * @brief Get StridedArrayView1Ds from all components of a SatVectorDesc
 *
 * @param strideDescArray   [in] Array of stride description for data in rData
 * @param rData             [in] unsigned char* Satellite data, optionally const
 * @param satCount          [in] Range of valid satellites
 *
 * @return std::array of views, intended to work with structured bindings
 */
template <typename T, std::size_t DIMENSIONS_T, typename DATA_T, std::size_t ... COUNT_T>
constexpr auto sat_views(
        StrideDescArray_t<T, DIMENSIONS_T> const& strideDescArray,
        DATA_T &rData,
        std::size_t satCount) noexcept
{
    // Recursive call to make COUNT_T a range of numbers from 0 to DIMENSIONS_T
    // This is unpacked into strideDescArray[COUNT_T].view(...) to access components
    constexpr int countUp = sizeof...(COUNT_T);
    if constexpr (countUp != DIMENSIONS_T)
    {
        return sat_views<T, DIMENSIONS_T, DATA_T, COUNT_T ..., countUp>(strideDescArray, rData, satCount);
    }
    else
    {
        return std::array{ strideDescArray[COUNT_T].view(Corrade::Containers::arrayView(rData), satCount) ... };
    }
}

/**
 * @brief Get transform of a coordinate space, but if a parent satellite exists,
 *        use parent satellite's transform instead.
 */
template<typename POSVIEW_T, typename ROTVIEW_T>
constexpr CoSpaceTransform coord_get_transform(
        CoSpaceHierarchy coordHier, CoSpaceTransform coordOrig,
        POSVIEW_T const& x,  POSVIEW_T const& y,  POSVIEW_T const& z,
        ROTVIEW_T const& qx, ROTVIEW_T const& qy, ROTVIEW_T const& qz, ROTVIEW_T const& qw) noexcept
{
    SatId const sat = coordHier.m_parentSat;
    return (sat == lgrn::id_null<SatId>())
        ? coordOrig
        : CoSpaceTransform{
            .m_rotation = {to_vec<Vector3d>(sat, qx, qy, qz), qw[std::size_t(sat)]},
            .m_position = to_vec<Vector3g>(sat, x, y, z),
            .m_precision = coordOrig.m_precision };
}

} // namespace osp::universe
