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

template <typename INT_T, std::size_t DIMENSIONS_T>
struct SatVectorDesc
{
    using View_t = Corrade::Containers::StridedArrayView1D<INT_T>;

    // Offsets of xyz components in bytes
    std::array<typename View_t::Size, DIMENSIONS_T> m_offsets;

    // Assuming xyz all have the same stride
    typename View_t::Stride m_stride;
};

struct CoSpaceTransform
{
    CoSpaceId       m_parent{lgrn::id_null<CoSpaceId>()};
    Vector3g        m_position;
    Quaterniond     m_rotation;
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

struct CoSpaceCommon : CoSpaceTransform, CoSpaceSatData { };

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
