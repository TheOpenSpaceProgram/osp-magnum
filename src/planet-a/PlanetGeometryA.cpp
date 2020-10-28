/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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
#include "PlanetGeometryA.h"

#include <Magnum/Math/Functions.h>
#include <osp/types.h>

#include <algorithm>
#include <iostream>
#include <stack>
#include <array>

// maybe do something about how this file is almost a thousand lines long
// there's a lot of comments though

using namespace planeta;

using osp::Vector3;

void planeta::update_range_insert(std::vector<UpdateRangeSub>& range,
                                  UpdateRangeSub insert)
{
    // TODO: Merge update ranges to decrease number of glBufferSubData calls

    //UpdateRangeSub merged;

    range.push_back(insert);
}

void PlanetGeometryA::initialize(float radius)
{
    //m_subdivAreaThreshold = 0.02f;
    m_vrtxSharedMax = 10000;
    m_chunkMax = 100;

    //m_chunkAreaThreshold = 0.04f;
    m_chunkWidth = 17; // MUST BE (POWER OF 2) + 1. min: 5
    m_chunkWidthB = m_chunkWidth - 1; // used often in many calculations

    m_icoTree = std::make_shared<IcoSphereTree>();
    m_icoTree->initialize(radius);

    using Magnum::Math::pow;

    m_chunkCount = 0;
    m_vrtxSharedCount = 0;

    // triangular numbers formula
    m_vrtxPerChunk = m_chunkWidth * (m_chunkWidth + 1) / 2;

    // This is how many rendered triangles is in a chunk
    m_indxPerChunk = pow(m_chunkWidthB, 2u);
    m_vrtxSharedPerChunk = m_chunkWidthB * 3;

    // calculate total max vertices
    m_vrtxMax = m_vrtxSharedMax + m_chunkMax * (m_vrtxPerChunk
            - m_vrtxSharedPerChunk);

    // allocate vectors for dealing with chunks
    m_indxBuffer.resize(m_chunkMax * m_indxPerChunk * 3);
    m_vrtxBuffer.resize(m_vrtxMax * m_vrtxSize);
    m_chunkToTri.resize(m_chunkMax);
    m_vrtxSharedUsers.resize(m_vrtxSharedMax);


    // Calculate m_chunkSharedIndices;  use:

    // Example of verticies in a chunk:
    // 0
    // 1  2
    // 3  4  5
    // 6  7  8  9

    // Chunk index data is stored as an array of (top, left, right):
    // { 0, 1, 2,  1, 3, 4,  4, 2, 1,  2, 4, 5,  3, 6, 7, ... }
    // Each chunk has the same number of triangles,
    // and are equally spaced in the buffer.
    // There are duplicates of the same index

    // Vertices in the edges of a chunk are considered "shared vertices,"
    // shared with the edges of the chunk's neighboors.
    // (in the example above: all of the vertices except for #4 are shared)
    // Shared vertices are added and removed in an unspecified order.
    // Their reserved space in m_chunkVertBuf is [0 - m_chunkSharedCount]

    // Vertices in the middle are only used by one chunk
    // (only 4), at larger sizes, they outnumber the shared ones
    // They are equally spaced in the vertex buffer
    // Their reserved space in m_chunkVertBuf is [m_chunkSharedCount - max]

    // It would be convinient if the indicies of the edges of the chunk
    // (shared verticies) can be accessed as if they were ordered in a
    // clockwise fasion around the edge of the triangle.
    // (see get_index_ringed).

    // so m_chunkSharedIndices is filled with indices to indices
    // indexToSharedVertex = tri->m_chunkIndex + m_chunkSharedIndices[0]

    // An alterative to this would be storing a list of shared vertices per
    // chunk, which takes more memory

    m_indToShared.resize(m_vrtxSharedPerChunk);

    int i = 0;
    for (int y = 0; y < int(m_chunkWidthB); y ++)
    {
        for (int x = 0; x < y * 2 + 1; x ++)
        {
            // Only consider the up-pointing triangle
            // All of the shared vertices can still be covered
            if ((x % 2) == 0)
            {
                for (int j = 0; j < 3; j ++)
                {
                    unsigned localIndex;

                    switch (j)
                    {
                    case 0:
                        localIndex = get_index_ringed(x / 2, y);
                        break;
                    case 1:
                        localIndex = get_index_ringed(x / 2, y + 1);
                        break;
                    case 2:
                        localIndex = get_index_ringed(x / 2 + 1, y + 1);
                        break;
                    }

                    if (localIndex < m_vrtxSharedPerChunk)
                    {
                        m_indToShared[localIndex] = unsigned(i + j);
                    }

                }
            }
            i += 3;
        }
    }

    m_initialized = true;

    // temporary: test chunking
    //chunk_add(0);
    m_icoTree->subdivide_add(0); // adds triangles 20, 21, 22, 23
    //chunk_add(20);
    m_icoTree->subdivide_add(20); // adds triangles 24, 25, 26, 27

    //chunk_remove(24);
    //chunk_remove(25);
    //chunk_remove(26);
    //chunk_remove(27);


    chunk_add(1);
    chunk_add(2);
    chunk_add(3);
    chunk_add(4);
    chunk_add(5);
    chunk_add(6);
    chunk_add(7);
    chunk_add(8);
    chunk_add(9);
    chunk_add(10);
    chunk_add(11);
    chunk_add(12);
    chunk_add(13);
    chunk_add(14);
    chunk_add(15);
    chunk_add(16);
    chunk_add(17);
    chunk_add(18);
    chunk_add(19);


    chunk_add(21);
    chunk_add(22);
    chunk_add(23);

    chunk_add(24);
    chunk_add(25);
    chunk_add(26);
    chunk_add(27);

    // temporary: raise all shared vertices based on shared count
    if (false)
    {
        for (vrindex_t i = 0; i < m_vrtxSharedMax; i ++)
        {
            if (m_vrtxSharedUsers[i] == 0)
            {
                continue;
            }

            float raise = (m_vrtxSharedUsers[i] - 1) * 5.0f;

            float *vrtxData = m_vrtxBuffer.data() + i * m_vrtxSize;

            Vector3 &pos = *reinterpret_cast<Vector3*>(
                                            vrtxData + m_vrtxCompOffsetPos);
            Vector3 &nrm = *reinterpret_cast<Vector3*>(
                                            vrtxData + m_vrtxCompOffsetNrm);

            pos += nrm * raise;
        }
    }

}

std::pair<PlanetGeometryA::IteratorTriIndexed,
          PlanetGeometryA::IteratorTriIndexed>
PlanetGeometryA::iterate_chunk(chindex_t c)
{
    IteratorTriIndexed begin(m_indxBuffer.begin() + m_indxPerChunk * 3 * c,
                             m_vrtxBuffer);
    IteratorTriIndexed end(m_indxBuffer.begin() + m_indxPerChunk * 3 * (c + 1),
                           m_vrtxBuffer);
    return {begin, end};
}

loindex_t PlanetGeometryA::get_index_ringed(unsigned x, unsigned y) const
{
    // || (x == y) ||
    if (y == m_chunkWidthB)
    {
        // Bottom edge
        return x;
    }
    else if (x == 0)
    {
        // Left edge
        return m_chunkWidthB * 2 + y;
    }
    else if (x == y)
    {
        // Right edge
        return m_chunkWidthB * 2 - y;
    }
    else
    {
        // Center
        return m_vrtxSharedPerChunk + get_index(x - 1, y - 2);
    }

}

constexpr loindex_t PlanetGeometryA::get_index(int x, int y) const
{
    return unsigned(y * (y + 1) / 2 + x);
}

struct VertexToSubdiv
{
    unsigned m_x, m_y;
    unsigned m_vrtxIndex;
};

void PlanetGeometryA::chunk_add(trindex_t t)
{
    chunk_triangle_assure();

    SubTriangle& tri = m_icoTree->get_triangle(t);
    SubTriangleChunk& chunk = m_triangleChunks[t];

    if (chunk.m_chunk != gc_invalidChunk)
    {
        // return if already chunked
        return;
    }

    if (m_chunkFree.empty())
    {

        if (m_chunkCount >= m_chunkMax)
        {
            // chunk limit reached
            return;
        }

        // Take the space at the end of the chunk buffer
        chunk.m_chunk = m_chunkCount;
        m_chunkCount ++;
    }
    else
    {
        // Use empty space available. Get lowest value [last element] to make
        // it less likely that the data will end up getting moved by chunk_pack
        // when it's closer to the start of the buffer
        std::set<chindex_t>::iterator chunkToGet = --m_chunkFree.end();
        chunk.m_chunk = *chunkToGet;
        m_chunkFree.erase(chunkToGet);
    }


    // Keep track of which part of the index buffer refers to which triangle
    m_chunkToTri[chunk.m_chunk] = t;

    if (m_vrtxFree.empty())
    {
        chunk.m_dataVrtx = m_vrtxSharedMax
                + chunk.m_chunk * (m_vrtxPerChunk - m_vrtxSharedPerChunk);
    }
    else
    {
        // Use empty space available in the chunk vertex buffer
        chunk.m_dataVrtx = m_vrtxFree.back();
        m_vrtxFree.pop_back();
    }

    // indices to shared and non-shared vertices added when subdividing
    std::vector<vrindex_t> indices(m_vrtxPerChunk, gc_invalidVrtx);

    using TriToSubdiv_t = std::array<VertexToSubdiv, 3>;

    //uint8_t neighbourDepths[3];
    SubTriangle* neighbours[3];
    SubTriangleChunk* neighbourChunks[3];
    int neighbourSide[3]; // Side of tri relative to neighbour's

    TriToSubdiv_t initTri{VertexToSubdiv{0, 0, 0},
                          VertexToSubdiv{0, m_chunkWidthB, 0},
                          VertexToSubdiv{m_chunkWidthB, m_chunkWidthB, 0}};

    // make sure shared corners can be tracked properly
    unsigned minSize = std::max({tri.m_corners[0] / m_icoTree->m_vrtxSize + 1,
                                 tri.m_corners[1] / m_icoTree->m_vrtxSize + 1,
                                 tri.m_corners[2] / m_icoTree->m_vrtxSize + 1,
                                 (unsigned) m_vrtxSharedIcoCorners.size()});
    m_vrtxSharedIcoCorners.resize(minSize, m_vrtxSharedMax);

    // Get neighbours
    for (int side = 0; side < 3; side ++)
    {
        trindex_t neighbourIndex = tri.m_neighbours[side];

        neighbours[side] = &(m_icoTree->get_triangle(neighbourIndex));
        neighbourSide[side] = m_icoTree->neighbour_side(*neighbours[side], t);
        neighbourChunks[side] = &m_triangleChunks[neighbourIndex];
        //neighbourDepths[i] = (triB->m_bitmask & gc_triangleMaskChunked);
    }

    // create 3 shared vertices for the first triangle
    for (int corner = 0; corner < 3; corner ++)
    {

        // corner 0 (top)   can be taken from side 1 (right)  and 2 (left)
        // corner 1 (left)  can be taken from side 2 (left)   and 0 (bottom)
        // corner 2 (right) can be taken from side 0 (bottom) and 1 (right)

        int sideA = (corner + 1) % 3;
        int sideB = (corner + 2) % 3;

        vrindex_t vertIndex;
        buindex_t &sharedCorner = m_vrtxSharedIcoCorners[
                                 tri.m_corners[corner] / m_icoTree->m_vrtxSize];

        if (sharedCorner < m_vrtxSharedMax)
        {
            // a shared triangle can be taken from a corner
            vertIndex = sharedCorner;
            m_vrtxSharedUsers[vertIndex] ++;
            std::cout << "Corner taken!\n";
        }
        else
        {
            bool dummy = false;

            // Get a vertex from either of the sides

            vertIndex  = shared_from_neighbour(t, sideA, m_chunkWidthB, dummy);

            if (vertIndex == gc_invalidVrtx)
            {
                vertIndex  = shared_from_neighbour(t, sideB, 0, dummy);
            }

            if (vertIndex == gc_invalidVrtx)
            {
                vertIndex = shared_create();
            }
            else
            {
                m_vrtxSharedUsers[vertIndex] ++;
            }
            sharedCorner = vertIndex;
        }

        auto vrtxOffset = vertIndex * m_vrtxSize;
        auto vrtxOffsetIt = m_vrtxBuffer.begin() + vrtxOffset;

        // sideB because local ringed indices start at the bottom left
        indices[sideB * m_chunkWidthB] = vertIndex;
        initTri[corner].m_vrtxIndex = vertIndex;

        float *vrtxDataRead = m_icoTree->get_vertex_pos(tri.m_corners[corner]);

        // Copy Icotree's position and normal data to vertex buffer

        std::copy(vrtxDataRead + m_icoTree->m_vrtxCompOffsetPos,
                  vrtxDataRead + m_icoTree->m_vrtxCompOffsetPos + 3,
                  vrtxOffsetIt + m_vrtxCompOffsetPos);

        std::copy(vrtxDataRead + m_icoTree->m_vrtxCompOffsetNrm,
                  vrtxDataRead + m_icoTree->m_vrtxCompOffsetNrm + 3,
                  vrtxOffsetIt + m_vrtxCompOffsetNrm);

        // Make sure this data is sent to the GPU eventually
        update_range_insert(m_gpuUpdVrtxBuffer,
                            {vrtxOffset, vrtxOffset + m_vrtxSize});
    }

    // subdivide the triangle multiple times

    std::stack<TriToSubdiv_t> m_toSubdiv;

    // add the first triangle
    m_toSubdiv.push(initTri);

    // keep track of the non-shared center chunk vertices
    unsigned centerIndex = 0;

    while (!m_toSubdiv.empty())
    {
        // top, left, right
        TriToSubdiv_t const triSub = m_toSubdiv.top();
        m_toSubdiv.pop();

        // subdivide and create middle vertices

        // v = vertices of current triangle
        // m = mid, middle vertices to calculate: bottom, left, right
        // t = center of next triangles to subdivide: top, left, right

        //            v0
        //
        //            t0
        //
        //      m1          m2
        //
        //      t1    t3    t2
        //
        // v1         m0         v2

        VertexToSubdiv mid[3];

        // loop through midverts
        for (int i = 0; i < 3; i ++)
        {
            VertexToSubdiv const &vrtxSubA = triSub[(2 + 3 - i) % 3];
            VertexToSubdiv const &vrtxSubB = triSub[(1 + 3 - i) % 3];

            mid[i].m_x = (vrtxSubA.m_x + vrtxSubB.m_x) / 2;
            mid[i].m_y = (vrtxSubA.m_y + vrtxSubB.m_y) / 2;

            loindex_t localIndex = get_index_ringed(mid[i].m_x, mid[i].m_y);
            bool between = false;

            if (indices[localIndex] != gc_invalidVrtx)
            {
                // skip if midvert already exists
                mid[i].m_vrtxIndex = indices[localIndex];
                continue;
            }

            if (localIndex < m_vrtxSharedPerChunk)
            {
                // Current vertex is shared (on the edge), try to grab an
                // existing vertex from a neighbouring triangle

                // Both of these should get optimized into a single div op
                uint8_t side = localIndex / m_chunkWidthB;
                loindex_t sideInd = localIndex % m_chunkWidthB;

                // Take a vertex from a neighbour, if possible


                vrindex_t share = shared_from_neighbour(
                                        t, side, sideInd, between);
                if (share == gc_invalidVrtx)
                {
                    share = shared_create();
                }
                else
                {
                    m_vrtxSharedUsers[share] ++;
                }
                mid[i].m_vrtxIndex = share;
            }
            else
            {
                // Current vertex is in the center and is not shared

                // Use a vertex from the space defined earler
                mid[i].m_vrtxIndex = chunk.m_dataVrtx + centerIndex;

                // Keep track of which middle index is being looped through
                centerIndex ++;

                buindex_t vrtxDataMid = mid[i].m_vrtxIndex * m_vrtxSize;

                // Make sure the GPU knows about this individual vertex change
                update_range_insert(m_gpuUpdVrtxBuffer,
                                    {vrtxDataMid, vrtxDataMid + m_vrtxSize});
            }

            indices[localIndex] = mid[i].m_vrtxIndex;

            float const* vrtxDataA = m_vrtxBuffer.data()
                                        + vrtxSubA.m_vrtxIndex * m_vrtxSize;
            float const* vrtxDataB = m_vrtxBuffer.data()
                                        + vrtxSubB.m_vrtxIndex * m_vrtxSize;

            Vector3 const& vrtxPosA = *reinterpret_cast<Vector3 const*>(
                                            vrtxDataA + m_vrtxCompOffsetPos);
            Vector3 const& vrtxPosB = *reinterpret_cast<Vector3 const*>(
                                            vrtxDataB + m_vrtxCompOffsetPos);

            Vector3 const& vertNrmA = *reinterpret_cast<Vector3 const*>(
                                            vrtxDataA + m_vrtxCompOffsetNrm);
            Vector3 const& vertNrmB = *reinterpret_cast<Vector3 const*>(
                                            vrtxDataB + m_vrtxCompOffsetNrm);

            buindex_t vrtxDataMid = mid[i].m_vrtxIndex * m_vrtxSize;
            auto vrtxDataMidIt = m_vrtxBuffer.begin() + vrtxDataMid;

            Vector3 pos = (vrtxPosA + vrtxPosB) * 0.5f;
            Vector3 nrm = pos.normalized();

            if (!between)
            {
                pos = nrm * float(m_icoTree->get_radius());
            }

            // Add Position data to vertex buffer
            std::copy(pos.data(), pos.data() + 3,
                      vrtxDataMidIt + m_vrtxCompOffsetPos);

            // Add Normal data to vertex buffer
            std::copy(nrm.data(), nrm.data() + 3,
                      vrtxDataMidIt + m_vrtxCompOffsetNrm);

        }

        // return if not subdividable further, distance between two verts
        // is 1
        if (Magnum::Math::abs<int>(triSub[1].m_x - triSub[2].m_x) == 2)
        {
            continue;
        }

        // next triangles to subdivide
        m_toSubdiv.emplace(TriToSubdiv_t{triSub[0],    mid[1],     mid[2]});
        m_toSubdiv.emplace(TriToSubdiv_t{   mid[1], triSub[1],     mid[0]});
        m_toSubdiv.emplace(TriToSubdiv_t{   mid[2],    mid[0],  triSub[2]});
        m_toSubdiv.emplace(TriToSubdiv_t{   mid[0],    mid[2],     mid[1]});
    }

    // Make sure the newly added center vertices are updated to the GPU
    update_range_insert(m_gpuUpdVrtxBuffer,
                        {chunk.m_dataVrtx,
                         chunk.m_dataVrtx + m_vrtxPerChunk * m_vrtxSize});

    // This data will be pushed directly into the chunk index buffer
    // * 3 because there are 3 indices in a triangle
    std::vector<unsigned> chunkIndData(m_indxPerChunk * 3);

    int i = 0;
    // indices array is now populated, connect the dots!
    for (int y = 0; y < int(m_chunkWidthB); y ++)
    {
        for (int x = 0; x < y * 2 + 1; x ++)
        {
            // alternate between true and false
            if (x % 2)
            {
                // upside down triangle
                // top, left, right
                chunkIndData[i + 0]
                        = indices[get_index_ringed(x / 2 + 1, y + 1)];
                chunkIndData[i + 1]
                        = indices[get_index_ringed(x / 2 + 1, y)];
                chunkIndData[i + 2]
                        = indices[get_index_ringed(x / 2, y)];
            }
            else
            {
                // up pointing triangle
                // top, left, rightkn
                chunkIndData[i + 0]
                        = indices[get_index_ringed(x / 2, y)];
                chunkIndData[i + 1]
                        = indices[get_index_ringed(x / 2, y + 1)];
                chunkIndData[i + 2]
                        = indices[get_index_ringed(x / 2 + 1, y + 1)];

            }
            //URHO3D_LOGINFOF("I: %i", i / 3);
            i += 3;
        }
    }

    // Make sure descendents know that they're part of a chunk
    if (tri.m_bitmask & gc_triangleMaskSubdivided)
    {
        set_chunk_ancestor_recurse(tri.m_children + 0, t);
        set_chunk_ancestor_recurse(tri.m_children + 1, t);
        set_chunk_ancestor_recurse(tri.m_children + 2, t);
        set_chunk_ancestor_recurse(tri.m_children + 3, t);
    }

    // Let all ancestors know that one of their descendents had got chunky
    if (tri.m_depth)
    {
        trindex_t curIndex = tri.m_parent;
        SubTriangle *curTri;
        do
        {
            curTri = &(m_icoTree->get_triangle(curIndex));
            m_triangleChunks[curIndex].m_descendentChunked ++;
            curIndex = curTri->m_parent;
        }
        while (curTri->m_depth != 0);
    }

    // Put the index data at the end of the buffer
    chunk.m_dataIndx = chunk.m_chunk * chunkIndData.size();
    std::copy(chunkIndData.begin(), chunkIndData.end(),
              m_indxBuffer.begin() + chunk.m_dataIndx);

    update_range_insert(m_gpuUpdIndxBuffer,
                        {chunk.m_dataIndx,
                         chunk.m_dataIndx + m_indxPerChunk * 3});
}


void PlanetGeometryA::chunk_triangle_assure()
{
    trindex_t min = std::max<trindex_t>(m_triangleChunks.size(),
                                        m_icoTree->triangle_count());
    m_triangleChunks.resize(min, {gc_invalidChunk, 0, gc_invalidTri, 0, 0});
}


void PlanetGeometryA::chunk_remove(trindex_t t)
{
    SubTriangle &tri = m_icoTree->get_triangle(t);
    SubTriangleChunk &chunk = m_triangleChunks[t];


    if (chunk.m_chunk == gc_invalidChunk)
    {
        // return if not chunked
        return;
    }


    // Delete Verticies

    // Mark middle vertices for replacement
    m_vrtxFree.push_back(chunk.m_dataVrtx);

    // Now delete shared vertices

    unsigned* triIndData = m_indxBuffer.data() + chunk.m_dataIndx;

    for (unsigned i = 0; i < m_vrtxSharedPerChunk; i ++)
    {
        buindex_t sharedIndex = *(triIndData + m_indToShared[i]);

        // Decrease number of users
        m_vrtxSharedUsers[sharedIndex] --;

        if (m_vrtxSharedUsers[sharedIndex] == 0)
        {
            // If users is zero, then delete
            std::cout << "shared removed\n";
            m_vrtxSharedFree.push_back(sharedIndex);
            m_vrtxSharedCount --;
        }
    }

    // loop through shared corners, to see if
    for (unsigned i = 0; i < 3; i ++)
    {
        buindex_t &sharedCorner = m_vrtxSharedIcoCorners[
                                 tri.m_corners[i] / m_icoTree->m_vrtxSize];
        if (m_vrtxSharedUsers[sharedCorner] == 0)
        {
            // corner was deleted, delete entry in m_vrtxSharedIcoCorners too
            sharedCorner = m_vrtxSharedMax;
            std::cout << "corner delete\n";
        }
    }

    // Make sure descendents know that they're no longer part of a chunk
    if (tri.m_bitmask & gc_triangleMaskSubdivided)
    {
        set_chunk_ancestor_recurse(tri.m_children + 0, gc_invalidTri);
        set_chunk_ancestor_recurse(tri.m_children + 1, gc_invalidTri);
        set_chunk_ancestor_recurse(tri.m_children + 2, gc_invalidTri);
        set_chunk_ancestor_recurse(tri.m_children + 3, gc_invalidTri);
    }

    // Let all ancestors know that lost one of their chunky descendents
    if (tri.m_depth != 0)
    {
        trindex_t curIndex = tri.m_parent;
        SubTriangle *curTri;
        do
        {
            curTri = &(m_icoTree->get_triangle(curIndex));
            m_triangleChunks[curIndex].m_descendentChunked --;
            curIndex = curTri->m_parent;
        }
        while (curTri->m_depth != 0);
    }

    // mark this chunk for replacement
    m_chunkFree.emplace(chunk.m_chunk);

    // set to not chunked
    chunk.m_chunk = gc_invalidChunk;
}

void PlanetGeometryA::chunk_remove_descendents_recurse(trindex_t t)
{
    SubTriangle &tri = m_icoTree->get_triangle(t);
    SubTriangleChunk &chunk = m_triangleChunks[t];

    if (chunk.m_chunk != gc_invalidChunk)
    {
        chunk_remove(t);
    }
    else if (chunk.m_descendentChunked != 0)
    {
        chunk_remove_descendents_recurse(tri.m_children + 0);
        chunk_remove_descendents_recurse(tri.m_children + 1);
        chunk_remove_descendents_recurse(tri.m_children + 2);
        chunk_remove_descendents_recurse(tri.m_children + 3);
    }
}

void PlanetGeometryA::chunk_remove_descendents(trindex_t t)
{
    SubTriangle &tri = m_icoTree->get_triangle(t);
    SubTriangleChunk &chunk = m_triangleChunks[t];

    if (chunk.m_descendentChunked != 0)
    {
        chunk_remove_descendents_recurse(tri.m_children + 0);
        chunk_remove_descendents_recurse(tri.m_children + 1);
        chunk_remove_descendents_recurse(tri.m_children + 2);
        chunk_remove_descendents_recurse(tri.m_children + 3);
    }
}

void PlanetGeometryA::chunk_pack()
{
    for (chindex_t chunk : m_chunkFree)
    {
        m_chunkCount --;

        // Get the associated triangle of removed chunk
        trindex_t replaceT = m_chunkToTri[chunk];

        // Get chunk and triangle data of removed
        SubTriangle &replaceTri = m_icoTree->get_triangle(replaceT);
        SubTriangleChunk &replaceChunk = m_triangleChunks[replaceT];

        // Get positions in index buffer
        unsigned* replaceData = m_indxBuffer.data() + replaceChunk.m_dataIndx;

        if (chunk == m_chunkCount)
        {
            // chunk on the end of the buffer was removed. the buffer size
            // shrinks and nothing happens.

            // Set chunked
            replaceChunk.m_chunk = gc_invalidChunk;
            continue;
        }

        // Get the associated triangle of the last chunk in the chunk buffer
        trindex_t moveT = m_chunkToTri[m_chunkCount];

        // Get chunk and triangle data of to move
        SubTriangle &moveTri = m_icoTree->get_triangle(moveT);
        SubTriangleChunk &moveChunk = m_triangleChunks[moveT];

        // Get positions in index buffer
        unsigned* moveData = m_indxBuffer.data() + moveChunk.m_dataIndx;


        // Do move

        // Replace tri's domain location with lastTriangle
        m_chunkToTri[chunk] = m_chunkToTri[moveChunk.m_chunk];

        // Move lastTri's index data to replace tri's data
        std::copy(m_indxBuffer.begin() + moveChunk.m_dataIndx,
                  m_indxBuffer.begin() + moveChunk.m_dataIndx + m_indxPerChunk * 3,
                  m_indxBuffer.begin() + replaceChunk.m_dataIndx);

        update_range_insert(m_gpuUpdIndxBuffer,
                            {replaceChunk.m_dataIndx,
                             replaceChunk.m_dataIndx + m_indxPerChunk * 3});

        // Change lastTriangle's chunk index to tri's
        moveChunk.m_dataIndx = replaceChunk.m_dataIndx;
        moveChunk.m_chunk = chunk;
    }

    m_chunkFree.clear();
}

void PlanetGeometryA::set_chunk_ancestor_recurse(trindex_t triInd,
                                                 trindex_t setTo)
{
    SubTriangle &tri = m_icoTree->get_triangle(triInd);

    m_triangleChunks[triInd].m_ancestorChunked = setTo;

    if (!(tri.m_bitmask & gc_triangleMaskSubdivided))
    {
        return;
    }

    // recurse into all children
    set_chunk_ancestor_recurse(tri.m_children + 0, setTo);
    set_chunk_ancestor_recurse(tri.m_children + 1, setTo);
    set_chunk_ancestor_recurse(tri.m_children + 2, setTo);
    set_chunk_ancestor_recurse(tri.m_children + 3, setTo);
}


vrindex_t PlanetGeometryA::shared_from_tri(SubTriangleChunk const& chunk,
                                           uint8_t side, loindex_t pos)
{
    //                  8
    //                9   7
    // [side 2]     10  12  6     [side 1]
    //            11  13  14  5
    //          0   1   2   3   4
    //               [side 0]

    // ie. if side = 1, pos will index {8, 9, 10, 11, 0}

    buindex_t localIndex = (side * m_chunkWidthB + pos) % m_vrtxSharedPerChunk;

    vrindex_t sharedOut = m_indxBuffer[chunk.m_dataIndx + m_indToShared[localIndex]];
    //m_vrtxSharedUsers[sharedOut] ++;

    std::cout << "grabbing vertex from side " << side << " pos: " << pos << "\n";
    return sharedOut;
}

vrindex_t PlanetGeometryA::shared_from_neighbour(trindex_t triInd,
                                                 uint8_t side, loindex_t posIn, bool &rBetween)
{
    vrindex_t sharedOut;

    // if (neighbour is same depth)
    //     if (neighbour is chunked)
    //         call shared_from_tri right away
    //     else if (neighbour is subdivided and has chunked children)
    //         set found chunk, and use general algorithm
    //     else if (neighbour is part of a chunk)
    //         set found chunk, and use general algorithm
    //     else
    //         create new shared vertex
    // else (if neighbour is less depth, aka: it's larger)
    //     if (neighbour is chunked)
    //         set found chunk, and use general algorithm
    //     else if (neighbour is part of chunk)
    //         set found chunk, and use general algorithm
    //     else if (neighbour is subdivided and has chunked children)
    //         THIS CANT HAPPEN, ERROR!
    // else if neighbour is more depth (smaller)
    //     THIS CANT HAPPEN, ERROR!

    // general algorithm
    // Get a chunked triangle on the neighbour
    // find parents with same depth
    // transform pos

    SubTriangle& tri = m_icoTree->get_triangle(triInd);
    SubTriangle& neighbourTri = m_icoTree->get_triangle(tri.m_neighbours[side]);
    SubTriangleChunk& neighbourChunk = m_triangleChunks[tri.m_neighbours[side]];

    uint8_t neighbourSide = 255;// = m_icoTree->neighbour_side(neighbourTri, t);

    trindex_t neighbourWithChunk = gc_invalidTri;

    float posTransformed = float(posIn) / float(m_chunkWidthB);

    if (tri.m_depth == neighbourTri.m_depth)
    {
        neighbourSide = m_icoTree->neighbour_side(neighbourTri, triInd);
        if (neighbourChunk.m_chunk != gc_invalidChunk)
        {
            // ideal case, same size and is chunked
            return shared_from_tri(neighbourChunk, neighbourSide,
                                   m_chunkWidthB - posIn);
        }
        else if (neighbourChunk.m_descendentChunked != 0)
        {
            // TODO: deal with 0.5f
            // has chunked children, grand children, etc... search for them
            // side 0 (bottom) : children 1->2
            // side 1  (right) : children 2->0
            // side 2   (left) : children 0->1

            float posChild = 1.0f - posTransformed;

            SubTriangleChunk *triChildChunk = &neighbourChunk;
            SubTriangle *triChildTri = &neighbourTri;
            trindex_t triNextChild;

            do
            {
                if (neighbourChunk.m_descendentChunked == 0
                    || !(triChildTri->m_bitmask & gc_triangleMaskSubdivided))
                {
                    return gc_invalidVrtx;
                }

                bool useLatterChild = posChild > 0.5f;
                triNextChild = triChildTri->m_children
                             + (neighbourSide + 1 + useLatterChild) % 3;

                triChildTri = &(m_icoTree->get_triangle(triNextChild));
                triChildChunk = &m_triangleChunks[triNextChild];

                posChild -= 0.5f * useLatterChild;
                posChild *= 2;

            }
            while (triChildChunk->m_chunk == gc_invalidChunk);

            neighbourWithChunk = triNextChild;

        }
        else if (neighbourChunk.m_ancestorChunked != gc_invalidTri)
        {
            // part of a chunk
            neighbourWithChunk = neighbourChunk.m_ancestorChunked;
        }
        else
        {
            return gc_invalidVrtx;
        }
    }
    else if (tri.m_depth > neighbourTri.m_depth)
    {
        if (neighbourChunk.m_chunk != gc_invalidChunk)
        {
            neighbourWithChunk = tri.m_neighbours[side];
        }
        else if (neighbourChunk.m_ancestorChunked != gc_invalidTri)
        {
            // part of a chunk
            neighbourWithChunk = neighbourChunk.m_ancestorChunked;
        }
        else
        {
            return gc_invalidVrtx;
        }

    }
    else
    {
        // TODO: throw some error here
        return gc_invalidVrtx;
    }

    SubTriangle &chunkedNeighbourTri = m_icoTree->get_triangle(neighbourWithChunk);

    // Detect if trying to get a space between vertices using divisibility by
    // powers of 2
    if ((tri.m_depth > chunkedNeighbourTri.m_depth))
    {
        if (posIn % (1 << (tri.m_depth - chunkedNeighbourTri.m_depth)) != 0)
        {
            rBetween = true;
            return gc_invalidVrtx;
        }
    }

    uint8_t commonDepth = std::min(
                tri.m_depth,
                chunkedNeighbourTri.m_depth);

    trindex_t ancestor;

    TriangleSideTransform tranFrom = m_icoTree->transform_to_ancestor(triInd, side, commonDepth, &ancestor);

    if (neighbourSide == 255)
    {
        neighbourSide = m_icoTree->neighbour_side(chunkedNeighbourTri, ancestor);
    }

    TriangleSideTransform tranTo = m_icoTree->transform_to_ancestor(neighbourWithChunk, neighbourSide,
                                     commonDepth);

    // normalize
    //float posTransformed = float(posIn) / float(m_chunkWidthB);
    // apply transform from
    posTransformed = posTransformed * tranFrom.m_scale + tranFrom.m_translation;
    // invert
    posTransformed = 1.0f - posTransformed;
    // inverse with transform to
    posTransformed = (posTransformed - tranTo.m_translation) / tranTo.m_scale;

    loindex_t posOut = std::round(posTransformed * m_chunkWidthB);

    return shared_from_tri(m_triangleChunks[neighbourWithChunk], neighbourSide, posOut);
}

vrindex_t PlanetGeometryA::shared_create()
{
    vrindex_t sharedOut;

    // Indices from 0 to m_vrtxSharedMax
    if (m_vrtxSharedCount + 1 >= m_vrtxSharedMax)
    {
        // TODO: shared vertex buffer full, error!
        return false;
    }
    if (m_vrtxSharedFree.size() == 0)
    {
        sharedOut = m_vrtxSharedCount;
    }
    else
    {
        sharedOut = m_vrtxSharedFree.back();
        m_vrtxSharedFree.pop_back();
    }

    m_vrtxSharedCount ++;
    m_vrtxSharedUsers[sharedOut] = 1; // set reference count

    return sharedOut;
}
