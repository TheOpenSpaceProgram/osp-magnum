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
#include "SubdivSkeleton.h"

using namespace planeta;

SubdivTriangleSkeleton::~SubdivTriangleSkeleton()
{
    // Release the 3 Vertex IDs of each triangle
    for (uint32_t i = 0; i < m_triIds.capacity(); i ++)
    {
        if ( ! m_triIds.exists(SkTriGroupId(i)))
        {
            continue;
        }

        for (SkeletonTriangle& rTri : m_triData[SkTriGroupId(i)].triangles)
        {
            for (SkVrtxOwner_t& rVrtx : rTri.vertices)
            {
                vrtx_release(std::exchange(rVrtx, {}));
            }
        }
    }
}

void SubdivTriangleSkeleton::vrtx_create_chunk_edge_recurse(
            std::uint8_t  const level,
            SkVrtxId      const vrtxA,
            SkVrtxId      const vrtxB,
            osp::ArrayView< MaybeNewId<SkVrtxId> > rOut)
{
    LGRN_ASSERTV(rOut.size() == ( (1<<level) - 1 ),
                 rOut.size(),   ( (1<<level) - 1 ));

    MaybeNewId<SkVrtxId> const mid      = vrtx_create_or_get_child(vrtxA, vrtxB);
    std::size_t          const halfSize = rOut.size() / 2;
    rOut[halfSize] = mid;

    if (level > 1)
    {
        vrtx_create_chunk_edge_recurse(level - 1, vrtxA,  mid.id, rOut.prefix(halfSize));
        vrtx_create_chunk_edge_recurse(level - 1, mid.id, vrtxB,  rOut.exceptPrefix(halfSize + 1));
    }
}

SkTriGroupPair SubdivTriangleSkeleton::tri_group_create(
        uint8_t const depth,
        SkTriId const parentId,
        SkeletonTriangle &rParent,
        std::array<std::array<SkVrtxId, 3>, 4> const vertices)
{
    SkTriGroupId const groupId = m_triIds.create();
    rParent.children = groupId;

    tri_group_resize_fit_ids(); // invalidates rParent

    SkTriGroup &rGroup = m_triData[groupId];
    rGroup.parent = parentId;
    rGroup.depth = depth;

    for (int i = 0; i < 4; i ++)
    {
        SkeletonTriangle &rTri = rGroup.triangles[i];
        rTri.vertices = {
            vrtx_store(vertices[i][0]),
            vrtx_store(vertices[i][1]),
            vrtx_store(vertices[i][2])
        };
    }
    return {groupId, rGroup};
}

SkTriGroupPair SubdivTriangleSkeleton::tri_group_create_root(
        uint8_t const depth,
        std::array<std::array<SkVrtxId, 3>, 4> const vertices)
{
    SkTriGroupId const groupId = m_triIds.create();

    tri_group_resize_fit_ids();

    SkTriGroup &rGroup = m_triData[groupId];
    rGroup.depth = depth;

    for (int i = 0; i < 4; i ++)
    {
        SkeletonTriangle &rTri = rGroup.triangles[i];
        rTri.vertices = {
            vrtx_store(vertices[i][0]),
            vrtx_store(vertices[i][1]),
            vrtx_store(vertices[i][2])
        };
    }
    return {groupId, rGroup};
}

SubdivTriangleSkeleton::NeighboringEdges SubdivTriangleSkeleton::tri_group_set_neighboring(SkTriGroupNeighboring lhs, SkTriGroupNeighboring rhs)
{
    auto const tri_group_edge = [] (SkTriGroupNeighboring const& x) -> SkTriGroupEdge
    {
        switch (x.edge)
        {
        case 0:
            return { x.rGroup.triangles[0].neighbors[0], x.rGroup.triangles[1].neighbors[0],
                           tri_id(x.id, 0),                    tri_id(x.id, 1) };
        case 1:
            return { x.rGroup.triangles[1].neighbors[1], x.rGroup.triangles[2].neighbors[1],
                           tri_id(x.id, 1),                    tri_id(x.id, 2) };
        case 2: default:
            return { x.rGroup.triangles[2].neighbors[2], x.rGroup.triangles[0].neighbors[2],
                           tri_id(x.id, 2),                    tri_id(x.id, 0) };
        }
    };
    NeighboringEdges const edges = { tri_group_edge(lhs), tri_group_edge(rhs) };

    edges.lhs.rNeighborA = tri_store(edges.rhs.childB);
    edges.lhs.rNeighborB = tri_store(edges.rhs.childA);
    edges.rhs.rNeighborA = tri_store(edges.lhs.childB);
    edges.rhs.rNeighborB = tri_store(edges.lhs.childA);

    return edges;
}

SkTriGroupPair SubdivTriangleSkeleton::tri_subdiv(
        SkTriId                 const triId,
        SkeletonTriangle&             rTri,
        std::array<SkVrtxId, 3> const vrtxMid)
{
    LGRN_ASSERTM(!rTri.children.has_value(), "SkeletonTriangle is already subdivided");

    SkTriGroup const& parentGroup = m_triData[tri_group_id(triId)];

    std::array<SkVrtxId, 3> corner = {rTri.vertices[0], rTri.vertices[1], rTri.vertices[2]};

    auto middle = [&vrtxMid] (int i) -> SkVrtxId { return vrtxMid[i]; };

    // c?: Corner vertex corner(?) aka: rTri.m_vertices[?]
    // m?: Middle vertex middle(?) aka: vrtxMid[?]
    // t?: Skeleton Triangle
    //
    //          c0
    //          /\                 Vertex order reminder:
    //         /  \                0:Top   1:Left   2:Right
    //        / t0 \                        0
    //    m0 /______\ m2                   / \
    //      /\      /\                    /   \
    //     /  \ t3 /  \                  /     \
    //    / t1 \  / t2 \                1 _____ 2
    //   /______\/______\
    // c1       m1       c2
    //
    SkTriGroupPair const group = tri_group_create(
        parentGroup.depth + 1,
        triId,
        rTri,
        {{
            { corner[0], middle(0), middle(2) }, // 0: Top
            { middle(0), corner[1], middle(1) }, // 1: Left
            { middle(2), middle(1), corner[2] }, // 2: Right
            { middle(1), middle(2), middle(0) }  // 3: Center
        }}
    );

    // Middle Triangle (index 3) neighbors all of its siblings
    group.rGroup.triangles[0].neighbors[1] = tri_store(tri_id(group.id, 3));
    group.rGroup.triangles[1].neighbors[2] = tri_store(tri_id(group.id, 3));
    group.rGroup.triangles[2].neighbors[0] = tri_store(tri_id(group.id, 3));
    group.rGroup.triangles[3].neighbors = {
        tri_store(tri_id(group.id, 2)),
        tri_store(tri_id(group.id, 0)),
        tri_store(tri_id(group.id, 1))
    };

    return group;
}

void SubdivTriangleSkeleton::tri_unsubdiv(SkTriId triId, SkeletonTriangle &rTri)
{
    SkTriGroup &rGroup = tri_group_at(rTri.children);

    auto const clear_neighbor = [this, &rGroup, triId, groupId = rTri.children] (int childIdx, int neighborIdx, bool pair)
    {
        SkTriOwner_t &rOwner = rGroup.triangles[childIdx].neighbors[neighborIdx];
        if (rOwner.has_value())
        {
            if (pair)
            {
                SkeletonTriangle &rNeighbor = tri_at(rOwner);
                int const neighborEdgeIdx = rNeighbor.find_neighbor_index(tri_id(groupId, childIdx));
                tri_release(std::exchange(rNeighbor.neighbors[neighborEdgeIdx], {}));
            }
            tri_release(std::exchange(rOwner, {}));
        }

    };
    clear_neighbor(0, 0, true);
    clear_neighbor(0, 1, false);
    clear_neighbor(0, 2, true);
    clear_neighbor(1, 0, true);
    clear_neighbor(1, 1, true);
    clear_neighbor(1, 2, false);
    clear_neighbor(2, 0, false);
    clear_neighbor(2, 1, true);
    clear_neighbor(2, 2, true);
    clear_neighbor(3, 0, false);
    clear_neighbor(3, 1, false);
    clear_neighbor(3, 2, false);

    for (int i = 0; i < 4; i ++)
    {
        SkeletonTriangle &rChildTri = rGroup.triangles[i];
        LGRN_ASSERTM(rChildTri.children.has_value() == false, "Children must not be subdivided to unsubdivide parent");
        for (int j = 0; j < 3; j ++)
        {
            vrtx_release(std::exchange(rChildTri.vertices[j], {}));
        }

        LGRN_ASSERTMV(m_triRefCount[tri_id(rTri.children, i).value] == 0,
                     "Can't unsubdivide if a child has a non-zero refcount",
                     tri_id(rTri.children, i).value,
                     m_triRefCount[tri_id(rTri.children, i).value]);
    }

    m_triIds.remove(rTri.children);

    rGroup.parent = lgrn::id_null<SkTriId>();
    rTri.children = lgrn::id_null<SkTriGroupId>();


}


//-----------------------------------------------------------------------------


ChunkId SkeletonChunks::chunk_create(
        SubdivTriangleSkeleton&         rSkel,
        SkTriId                   const skTri,
        osp::ArrayView<SkVrtxId>  const edgeRte,
        osp::ArrayView<SkVrtxId>  const edgeBtm,
        osp::ArrayView<SkVrtxId>  const edgeLft)
{
    LGRN_ASSERT(   edgeRte.size() == m_chunkWidth - 1
                && edgeBtm.size() == m_chunkWidth - 1
                && edgeLft.size() == m_chunkWidth - 1 );

    ChunkId const chunkId   = m_chunkIds.create();
    SkTriOwner_t &rChunkTri = m_chunkTris[chunkId];

    rChunkTri = rSkel.tri_store(skTri);

    SkeletonTriangle const &tri = rSkel.tri_at(skTri);

    osp::ArrayView<SharedVrtxOwner_t> const sharedSpace = shared_vertices_used(chunkId);

    std::array const edges = {edgeRte, edgeBtm, edgeLft};

    for (int i = 0; i < 3; i ++)
    {
        std::size_t const cornerOffset = m_chunkWidth * i;
        std::size_t const edgeOffset = m_chunkWidth * i + 1;

        osp::ArrayView<SkVrtxId> const edge = edges[i];
        {
            SharedVrtxId const sharedId = shared_get_or_create(tri.vertices[i], rSkel);
            sharedSpace[cornerOffset] = shared_store(sharedId);
        }
        for (unsigned int j = 0; j < m_chunkWidth - 1; j ++)
        {
            SharedVrtxId const sharedId = shared_get_or_create(edge[j], rSkel);
            sharedSpace[edgeOffset + j] = shared_store(sharedId);
        }
    }

    return chunkId;
}



SharedVrtxId SkeletonChunks::shared_get_or_create(SkVrtxId const skVrtxId, SubdivTriangleSkeleton &rSkel)
{
    m_skVrtxToShared.resize(m_sharedIds.capacity());
    SharedVrtxId &rShared = m_skVrtxToShared[skVrtxId];
    if (rShared == lgrn::id_null<SharedVrtxId>())
    {
        rShared = m_sharedIds.create();
        m_sharedFaceCount[rShared] = 0;
        m_sharedSkVrtx[rShared] = rSkel.vrtx_store(skVrtxId);
        m_sharedNewlyAdded.push_back(rShared);
    }

    return rShared;
}


void SkeletonChunks::clear(SubdivTriangleSkeleton& rSkel)
{
    // Release associated skeleton triangles from chunks
    for (std::size_t chunkInt : m_chunkIds.bitview().zeros())
    {
        auto const chunk = static_cast<ChunkId>(chunkInt);

        rSkel.tri_release(std::exchange(m_chunkTris[chunk], {}));

        // Release all shared vertices
        for (SharedVrtxOwner_t& shared : shared_vertices_used(chunk))
        {
            shared_release(shared);
        }
    }

    // Release all associated skeleton vertices
    for (std::size_t sharedInt : m_sharedIds.bitview().zeros())
    {
        auto const shared = static_cast<SharedVrtxId>(sharedInt);

        rSkel.vrtx_release(std::exchange(m_sharedSkVrtx[shared], {}));
    }
}

