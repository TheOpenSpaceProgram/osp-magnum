
/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "icosahedron.h"

using namespace planeta;

SubdivTriangleSkeleton planeta::create_skeleton_icosahedron(
        double  const radius,
        int     const pow2scale,
        Corrade::Containers::StaticArrayView<12, SkVrtxId>      const vrtxIds,
        Corrade::Containers::StaticArrayView<5,  SkTriGroupId>  const groupIds,
        Corrade::Containers::StaticArrayView<20, SkTriId>       const triIds,
        std::vector<osp::Vector3l>  &rPositions,
        std::vector<osp::Vector3>   &rNormals)
{

    // Create the skeleton

    SubdivTriangleSkeleton skeleton;

    // Create initial 12 vertices

    for (SkVrtxId &rVrtxId : vrtxIds)
    {
        rVrtxId = skeleton.vrtx_create_root();
    }

    rPositions.resize(skeleton.vrtx_ids().capacity());
    rNormals.resize(skeleton.vrtx_ids().capacity());

    // Set positions and normals of the new vertices

    double const scale = std::pow(2.0, pow2scale);

    for (int i = 0; i < gc_icoVrtxCount; i ++)
    {
        rPositions[i] = osp::Vector3l(gc_icoVrtxPos[i] * radius * scale);
        rNormals[i] = osp::Vector3(gc_icoVrtxPos[i]);
    }

    // Add initial triangles

    // Create 20 root triangles by forming 5 groups. each group is 4 triangles

    skeleton.tri_group_reserve(skeleton.tri_group_ids().size() + 5);

    auto const vrtx_id_lut = [&vrtxIds] (int i) -> std::array<SkVrtxId, 3>
    {
        return { vrtxIds[ gc_icoIndx[i][0] ],
                 vrtxIds[ gc_icoIndx[i][1] ],
                 vrtxIds[ gc_icoIndx[i][2] ] };
    };

    for (int i = 0, j = 0; i < 5; ++i, j += 4)
    {
        std::array<std::array<SkVrtxId, 3>, 4> const triVrtx
        {
            vrtx_id_lut(j + 0),
            vrtx_id_lut(j + 1),
            vrtx_id_lut(j + 2),
            vrtx_id_lut(j + 3)
        };

        groupIds[i] = skeleton.tri_group_create_root(0, triVrtx).id;

        triIds[j + 0] = tri_id(groupIds[i], 0);
        triIds[j + 1] = tri_id(groupIds[i], 1);
        triIds[j + 2] = tri_id(groupIds[i], 2);
        triIds[j + 3] = tri_id(groupIds[i], 3);
    }

    for (int i = 0; i < triIds.size(); ++i)
    {
        auto const& neighbors = gc_icoNeighbors[i];
        SkTriId const triId = triIds[i];
        skeleton.tri_at(triId).neighbors = {
            skeleton.tri_store(triIds[neighbors[0]]),
            skeleton.tri_store(triIds[neighbors[1]]),
            skeleton.tri_store(triIds[neighbors[2]]),
        };
    }

    return skeleton;
}

/**
 * @brief Calculate middle point on a sphere's surface given two other points on the sphere
 */
void sphere_midpoint(
        double const radius, float const scale, osp::Vector3l const a, osp::Vector3l const b,
        osp::Vector3l &rPos, osp::Vector3 &rNorm) noexcept
{
    // This is simply "rPos = (a + b).normalized() * radius * scale", but avoids squaring
    // potentially massive numbers and tries to improve precision

    osp::Vector3l const mid = (a + b) / 2;
    osp::Vector3d const midD = osp::Vector3d(mid) / scale;
    float const midLen = midD.length();
    float const roundness = radius - midLen;

    rNorm = osp::Vector3(midD / midLen);
    rPos = mid + osp::Vector3l(rNorm * roundness * scale);
}

void planeta::ico_calc_middles(
        double const radius, int const pow2scale,
        std::array<SkVrtxId, 3> const vrtxCorner,
        std::array<MaybeNewId<SkVrtxId>, 3> const vrtxMid,
        std::vector<osp::Vector3l> &rPositions,
        std::vector<osp::Vector3> &rNormals)
{
    auto const pos = [&rPositions] (SkVrtxId const id) -> osp::Vector3l& { return rPositions[size_t(id)]; };
    auto const nrm = [&rNormals]   (SkVrtxId const id) -> osp::Vector3&  { return rNormals[size_t(id)];   };

    float const scale = std::pow(2.0f, pow2scale);

    if (vrtxMid[0].isNew)
    {
        sphere_midpoint(radius, scale, pos(vrtxCorner[0]), pos(vrtxCorner[1]),
                    pos(vrtxMid[0].id), nrm(vrtxMid[0].id));
    }
    if (vrtxMid[1].isNew)
    {
        sphere_midpoint(radius, scale, pos(vrtxCorner[1]), pos(vrtxCorner[2]),
                    pos(vrtxMid[1].id), nrm(vrtxMid[1].id));
    }
    if (vrtxMid[2].isNew)
    {
        sphere_midpoint(radius, scale, pos(vrtxCorner[2]), pos(vrtxCorner[0]),
                    pos(vrtxMid[2].id), nrm(vrtxMid[2].id));
    }
}


void planeta::ico_calc_chunk_edge_recurse(
        double const        radius,
        int const           pow2scale,
        unsigned int const  level,
        SkVrtxId const      a,
        SkVrtxId const      b,
        osp::ArrayView<SkVrtxId const> const    vrtxs,
        std::vector<osp::Vector3l>              &rPositions,
        std::vector<osp::Vector3>               &rNormals)
{
    if (level == 0)
    {
        return;
    }

    auto const pos = [&rPositions] (SkVrtxId const id) -> osp::Vector3l& { return rPositions[size_t(id)]; };
    auto const nrm = [&rNormals]   (SkVrtxId const id) -> osp::Vector3&  { return rNormals[size_t(id)];   };

    size_t const halfSize = vrtxs.size() / 2;
    SkVrtxId const mid = vrtxs[halfSize];

    sphere_midpoint(radius, std::pow(2.0f, pow2scale), pos(a), pos(b),
                    pos(mid), nrm(mid));

    ico_calc_chunk_edge_recurse(radius, pow2scale, level - 1, a, mid,
                                vrtxs.prefix(halfSize), rPositions, rNormals);
    ico_calc_chunk_edge_recurse(radius, pow2scale, level - 1, mid, b,
                                vrtxs.exceptPrefix(halfSize), rPositions, rNormals);

}

