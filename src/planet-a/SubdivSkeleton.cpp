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

    // Release owners to neighbors for all 4 triangles in the group

    auto const clear_neighbor = [this, &rGroup, triId, groupId = rTri.children] (int childIdx, int neighborIdx, bool isSibling)
    {
        SkTriOwner_t &rOwner = rGroup.triangles[childIdx].neighbors[neighborIdx];
        if (rOwner.has_value())
        {
            // Since siblings neighbor each other in a known arrangement, skip to avoid
            // unnessesary lookups
            if ( ! isSibling )
            {
                SkeletonTriangle &rNeighbor = tri_at(rOwner);
                int const neighborEdgeIdx = rNeighbor.find_neighbor_index(tri_id(groupId, childIdx));
                tri_release(std::exchange(rNeighbor.neighbors[neighborEdgeIdx], {}));
            }
            tri_release(std::exchange(rOwner, {}));
        }
    };

    clear_neighbor(0, 0, false);
    clear_neighbor(0, 1, true);
    clear_neighbor(0, 2, false);
    clear_neighbor(1, 0, false);
    clear_neighbor(1, 1, false);
    clear_neighbor(1, 2, true);
    clear_neighbor(2, 0, true);
    clear_neighbor(2, 1, false);
    clear_neighbor(2, 2, false);
    clear_neighbor(3, 0, true);
    clear_neighbor(3, 1, true);
    clear_neighbor(3, 2, true);

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
        SkTriId                   const sktriId,
        osp::ArrayView<MaybeNewId<SkVrtxId>>  const edgeRte,
        osp::ArrayView<MaybeNewId<SkVrtxId>>  const edgeBtm,
        osp::ArrayView<MaybeNewId<SkVrtxId>>  const edgeLft)
{
    LGRN_ASSERT(   edgeRte.size() == m_chunkEdgeVrtxCount - 1
                && edgeBtm.size() == m_chunkEdgeVrtxCount - 1
                && edgeLft.size() == m_chunkEdgeVrtxCount - 1 );

    ChunkId const chunkId   = m_chunkIds.create();

    m_chunkToTri[chunkId] = sktriId;
    m_triToChunk[sktriId] = chunkId;

    SkeletonTriangle const &tri = rSkel.tri_at(sktriId);

    osp::ArrayView<SharedVrtxOwner_t> const chunkSharedVertices = shared_vertices_used(chunkId);

    auto const assign_shared_vertices = [this, &chunkSharedVertices, &tri, &rSkel] (osp::ArrayView< MaybeNewId<SkVrtxId> > const skvrtx, int edgeIdx)
    {
        auto const edgeSharedVertices = chunkSharedVertices.sliceSize(m_chunkEdgeVrtxCount * edgeIdx, m_chunkEdgeVrtxCount);

        // First vertex along the edge is the corner
        edgeSharedVertices[0] = shared_store(shared_get_or_create(tri.vertices[edgeIdx], rSkel));

        for (unsigned int i = 0; i < m_chunkEdgeVrtxCount-1; i ++)
        {
            edgeSharedVertices[i+1] = shared_store(shared_get_or_create(skvrtx[i].id, rSkel));
        }
    };

    assign_shared_vertices(edgeRte, 0);
    assign_shared_vertices(edgeBtm, 1);
    assign_shared_vertices(edgeLft, 2);

    return chunkId;
}

void SkeletonChunks::chunk_remove(ChunkId const chunkId, SkTriId const sktriId, SubdivTriangleSkeleton& rSkel) noexcept
{
    for (SharedVrtxOwner_t& rOwner : shared_vertices_used(chunkId))
    {
        shared_release(std::exchange(rOwner, {}), rSkel);
    }
    m_triToChunk[sktriId] = {};
    m_chunkToTri[chunkId] = {};
    m_chunkIds.remove(chunkId);
    //rSkel.tri_release(std::exchange(m_chunkToTri[chunkId], {}));
}



SharedVrtxId SkeletonChunks::shared_get_or_create(SkVrtxId const skVrtxId, SubdivTriangleSkeleton &rSkel)
{
    m_skVrtxToShared.resize(rSkel.vrtx_ids().capacity());
    SharedVrtxId &rShared = m_skVrtxToShared[skVrtxId];
    if (rShared == lgrn::id_null<SharedVrtxId>())
    {
        rShared = m_sharedIds.create();
        LGRN_ASSERTM(rShared.has_value(), "Exceeded max shared vertices!");
        m_sharedFaceCount[rShared] = 0;
        m_sharedToSkVrtx [rShared] = rSkel.vrtx_store(skVrtxId);
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

        //rSkel.tri_release(std::move(m_chunkToTri[chunk]));

        // Release all shared vertices
        for (SharedVrtxOwner_t& shared : shared_vertices_used(chunk))
        {
            shared_release(std::move(shared), rSkel);
        }
    }
    m_chunkToTri.clear();
    m_chunkSharedUsed.clear();

    // Release all associated skeleton vertices
    for (std::size_t sharedInt : m_sharedIds.bitview().zeros())
    {
        auto const shared = static_cast<SharedVrtxId>(sharedInt);

        rSkel.vrtx_release(std::move(m_sharedToSkVrtx[shared]));
    }
    m_sharedToSkVrtx.clear();
}

