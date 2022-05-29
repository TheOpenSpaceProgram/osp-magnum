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
#include "universetypes.h"

#include <longeron/id_management/registry_stl.hpp>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StridedArrayView.h>

#include <array>
#include <cstdint>

namespace osp::universe
{

/**
 * @brief Describes strided vector data within an externally stored buffer
 *
 * Intended to make different data layouts easier to access, and allows
 * interleving different datatypes:
 * * { XXXXX ... YYYYY ... ZZZZZ ... }
 * * { XYZ XYZ XYZ XYZ XYZ XYZ ... }
 * * { XYZ ??? XYZ ??? XYZ ??? ... }
 */
template <typename T, int DIMENSIONS_T>
struct SatVectorDesc
{
    using View_t        = Corrade::Containers::StridedArrayView1D<T>;
    using ViewConst_t   = Corrade::Containers::StridedArrayView1D<T const>;
    using Data_t        = Corrade::Containers::ArrayView<unsigned char>;
    using DataConst_t   = Corrade::Containers::ArrayView<unsigned char const>;

    static constexpr int smc_dimensions = DIMENSIONS_T;

    // Offsets of xyz components in bytes
    std::array<std::size_t, DIMENSIONS_T> m_offsets;

    // Assuming xyz all have the same stride
    std::ptrdiff_t m_stride;

    constexpr View_t view(Data_t data, std::size_t count, int dimension) const
    {
        using namespace Corrade::Containers;
        auto first = reinterpret_cast<T*>((data.exceptPrefix(m_offsets[dimension])).data());
        return {data, first, count, m_stride};
    }

    constexpr ViewConst_t view(DataConst_t data, std::size_t count, int dimension) const
    {
        using namespace Corrade::Containers;
        auto first = reinterpret_cast<T const*>((data.exceptPrefix(m_offsets[dimension])).data());
        return {data, first, count, m_stride};
    }
};

template <typename VEC_T, typename INDEX_T, typename ... RANGE_T>
constexpr VEC_T to_vec(INDEX_T i, RANGE_T&& ... args) noexcept
{
    return {args[std::size_t(i)] ...};
}

/**
 * @brief Get StridedArrayView1Ds from all components of a SatVectorDesc
 *
 * @param satVec   [in] SatVectorDesc describing layout in rData
 * @param rData    [in] unsigned char* Satellite data, optionally const
 * @param satCount [in] Range of valid satellites
 *
 * @return std::array of views, intended to work with structured bindings
 */
template <typename T, int DIMENSIONS_T, typename DATA_T, int ... COUNT_T>
constexpr auto sat_views(
        SatVectorDesc<T, DIMENSIONS_T> const& satVec,
        DATA_T &rData,
        std::size_t satCount) noexcept
{
    // Recursive call to make COUNT_T a range of numbers from 0 to DIMENSIONS_T
    // This is unpacked into satVec.view(...) to access components
    constexpr int countUp = sizeof...(COUNT_T);
    if constexpr (countUp != DIMENSIONS_T)
    {
        return sat_views<T, DIMENSIONS_T, DATA_T, COUNT_T ..., countUp>(satVec, rData, satCount);
    }
    else
    {
        return std::array{ satVec.view(Corrade::Containers::arrayView(rData), satCount, COUNT_T) ... };
    }
}

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

    Corrade::Containers::Array<unsigned char> m_data;
    SatVectorDesc<spaceint_t, 3>    m_satPositions;
    SatVectorDesc<float, 3>         m_satVelocities;
};

struct CoSpaceCommon : CoSpaceTransform, CoSpaceHierarchy, CoSpaceSatData { };

struct RedesignateSat
{
    SatId m_old;
    SatId m_new;
};

struct TransferSat
{
    SatId m_satOld;
    SatId m_satNew;

    CoSpaceId m_coordOld;
    CoSpaceId m_coordNew;
};

struct Universe
{
    lgrn::IdRegistryStl<CoSpaceId>   m_coordIds;

    std::vector<CoSpaceCommon>     m_coordCommon;
};

} // namespace osp::universe