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

namespace planeta
{

SubdivTriangleSkeleton create_skeleton_icosahedron(
        double                                                  const radius,
        Corrade::Containers::StaticArrayView<12, SkVrtxId>      const vrtxIds,
        Corrade::Containers::StaticArrayView<5,  SkTriGroupId>  const groupIds,
        Corrade::Containers::StaticArrayView<20, SkTriId>       const triIds,
        SkeletonVertexData                                            &rSkData)
{
    // Create the skeleton
    SubdivTriangleSkeleton skeleton;

    // Create initial 12 vertices
    for (SkVrtxId &rVrtxId : vrtxIds)
    {
        rVrtxId = skeleton.vrtx_create_root();
    }

    // Copy and scale Icosahedron vertex data from tables

    rSkData.positions.resize(skeleton.vrtx_ids().capacity());
    rSkData.normals.resize  (skeleton.vrtx_ids().capacity());
    double const totalScale = radius * std::exp2(rSkData.precision);
    for (int i = 0; i < gc_icoVrtxCount; i ++)
    {
        rSkData.positions[vrtxIds[i]] = osp::Vector3l(gc_icoVrtxPos[i] * totalScale);
        rSkData.normals  [vrtxIds[i]] = osp::Vector3 (gc_icoVrtxPos[i]);
    }

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
 * @brief Calculate midpoint between two vertices on a skeleton mesh
 */
static void calc_midpoint_spherical(
        SkVrtxId              const a,
        SkVrtxId              const mid,
        SkVrtxId              const b,
        double                const radius,
        double                const scale,
        SkeletonVertexData          &rSkData)
{
    // Midpoint is first calculated with only integers, then curvature is added on afterwards.
    // This is intended to prevent gargantuan numbers from squaring Vertex3l (int64) positions.
    osp::Vector3l const midPos    = (rSkData.positions[a] + rSkData.positions[b]) / 2;
    osp::Vector3d const midPosDbl = osp::Vector3d(midPos) / scale;
    double        const midLen    = midPosDbl.length();
    double        const curvature = radius - midLen;

    rSkData.normals[mid]   = osp::Vector3(midPosDbl / midLen);
    rSkData.positions[mid] = midPos + osp::Vector3l(osp::Vector3d(rSkData.normals[mid]) * (curvature * scale));
}


void ico_calc_middles(
        double                                    const radius,
        std::array<SkVrtxId, 3>                   const vrtxCorner,
        std::array<osp::MaybeNewId<SkVrtxId>, 3>  const vrtxMid,
        SkeletonVertexData                              &rSkData)
{
    double const scale = std::exp2(double(rSkData.precision));

    if (vrtxMid[0].isNew)
    {
        calc_midpoint_spherical(vrtxCorner[0], vrtxMid[0].id, vrtxCorner[1], radius, scale, rSkData);
    }
    if (vrtxMid[1].isNew)
    {
        calc_midpoint_spherical(vrtxCorner[1], vrtxMid[1].id, vrtxCorner[2], radius, scale, rSkData);
    }
    if (vrtxMid[2].isNew)
    {
        calc_midpoint_spherical(vrtxCorner[2], vrtxMid[2].id, vrtxCorner[0], radius, scale, rSkData);
    }
}

using ChunkEdgeView_t = osp::ArrayView<osp::MaybeNewId<SkVrtxId> const>;

void ico_calc_chunk_edge(
        double                const radius,
        std::uint8_t          const level,
        SkVrtxId              const cornerA,
        SkVrtxId              const cornerB,
        ChunkEdgeView_t       const vrtxEdge,
        SkeletonVertexData          &rSkData)
{
    if (level <= 1)
    {
        return;
    }

    float const scale = std::exp2(float(rSkData.precision));

    auto const recurse = [scale, radius, &rSkData] (auto &&self, SkVrtxId const a, SkVrtxId const b, int const currentLevel, ChunkEdgeView_t const view) noexcept -> void
    {
        auto const halfSize = view.size() / 2;
        osp::MaybeNewId<SkVrtxId> const mid = view[halfSize];

        if (mid.isNew)
        {
            calc_midpoint_spherical(a, mid.id, b, radius, scale, rSkData);
        }

        if (currentLevel != 1)
        {
            self(self, a, mid.id, currentLevel - 1, view.prefix(halfSize));
            self(self, mid.id, b, currentLevel - 1, view.exceptPrefix(halfSize));
        }
    };

    recurse(recurse, cornerA, cornerB, level, vrtxEdge);
}


void ico_calc_sphere_tri_center(
        SkTriGroupId            const groupId,
        double                  const maxRadius,
        double                  const height,
        SubdivTriangleSkeleton  const &rSkel,
        SkeletonVertexData&           rSkData)
{
    SkTriGroup const &group = rSkel.tri_group_at(groupId);
    LGRN_ASSERT(group.depth < gc_icoTowerOverHorizonVsLevel.size());

    double const terrainMaxHeight = height + maxRadius * gc_icoTowerOverHorizonVsLevel[group.depth];
    float  const scale            = std::exp2(float(rSkData.precision));

    for (int i = 0; i < 4; ++i)
    {
        SkTriId          const  sktriId = tri_id(groupId, i);
        SkeletonTriangle const& tri     = group.triangles[i];

        SkVrtxId const va = tri.vertices[0].value();
        SkVrtxId const vb = tri.vertices[1].value();
        SkVrtxId const vc = tri.vertices[2].value();

        // Divide components individually to prevent potential overflow
        osp::Vector3l const posAverage = rSkData.positions[va] / 3
                                       + rSkData.positions[vb] / 3
                                       + rSkData.positions[vc] / 3;

        osp::Vector3  const nrmAverage = (  rSkData.normals[va]
                                          + rSkData.normals[vb]
                                          + rSkData.normals[vc]) / 3.0;

        osp::Vector3l const highestPoint = osp::Vector3l(nrmAverage * float(terrainMaxHeight * scale));

        rSkData.centers[sktriId] = posAverage + highestPoint;
    }
}

} // namespace planeta
