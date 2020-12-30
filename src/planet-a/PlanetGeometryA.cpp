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

void PlanetGeometryA::initialize(std::shared_ptr<IcoSphereTree> sphere,
                                 unsigned chunkDiv, chindex_t maxChunks,
                                 vrindex_t maxShared)
{
    //m_subdivAreaThreshold = 0.02f;
    m_vrtxSharedMax = maxShared;
    m_chunkMax = maxChunks;

    //m_chunkAreaThreshold = 0.04f;
    m_chunkWidthB = (1 << chunkDiv);
    m_chunkWidth  = m_chunkWidthB + 1; // MUST BE (POWER OF 2) + 1. min: 5


    //m_icoTree = std::make_shared<IcoSphereTree>();
    m_icoTree = std::move(sphere);

    //m_icoTree->initialize(radius);

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
    m_vrtxSharedIcoCorners.resize(m_icoTree->get_vertex_buffer().size()
                                    / m_vrtxSize, gc_invalidVrtx);
    m_vrtxSharedIcoCornersReverse.resize(m_vrtxSharedMax, gc_invalidBufIndx);

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

// temporary
float debug_stupid_heightmap(Vector3 pos)
{
    float raise = 0; // add together a bunch of sin waves for terrain
    raise += (std::sin(pos.x() / 128.0f) + std::sin(pos.y() / 128.0f) + std::sin(pos.z() / 128.0f)) * 64.0f;
    raise += (std::sin(pos.x() / 500.0f) + std::sin(pos.y() / 500.0f) + std::sin(pos.z() / 500.0f)) * 128.0f;
    raise += (std::sin(pos.x() / 720.0f) + std::sin(pos.y() / 720.0f) + std::sin(pos.z() / 720.0f)) * 512.0f;
    return raise * 0.0f;
}

void PlanetGeometryA::chunk_add(trindex_t t)
{
    //std::cout << "chunk_add(" << t << ");\n";
    chunk_triangle_assure();

    SubTriangle &rTri = m_icoTree->get_triangle(t);
    SubTriangleChunk &rChunk = m_triangleChunks[t];

    if (rChunk.m_chunk != gc_invalidChunk)
    {
        // return if already chunked
        return;
    }

    // TODO: this function got pretty massive, maybe separate it more some time

    // Step 1: Find a space in the chunk index buffer

    if (m_chunkFree.empty())
    {
        if (m_chunkCount >= m_chunkMax)
        {
            // chunk limit reached
            return;
        }

        // Take the space at the end of the chunk buffer
        rChunk.m_chunk = m_chunkCount;
        m_chunkCount ++;
    }
    else
    {
        // Use empty space available. Get lowest value [last element] to make
        // it less likely that the data will end up getting moved by chunk_pack
        // when it's closer to the start of the buffer
        std::set<chindex_t>::iterator chunkToGet = --m_chunkFree.end();
        rChunk.m_chunk = *chunkToGet;
        m_chunkFree.erase(chunkToGet);
    }

    // Step 2: Find a space in the vertex buffer

    // Keep track of which part of the index buffer refers to which triangle
    m_chunkToTri[rChunk.m_chunk] = t;

    if (m_vrtxFree.empty())
    {
        rChunk.m_dataVrtx = m_vrtxSharedMax
                + rChunk.m_chunk * (m_vrtxPerChunk - m_vrtxSharedPerChunk);
    }
    else
    {
        // Use empty space available in the chunk vertex buffer
        rChunk.m_dataVrtx = m_vrtxFree.back();
        m_vrtxFree.pop_back();
    }

    // Step 3: Get neighbours with chunks to share vertices with

    for (int side = 0; side < 3; side ++)
    {
        trindex_t neighbourIndex = rTri.m_neighbours[side];

        trindex_t &rNeighbourChunked = rChunk.m_neighbourChunked[side];
        rNeighbourChunked = gc_invalidTri;

        // Neighbour that may be chunked, contains a chunk, or is part of a
        // chunk
        SubTriangle const &neighTri = m_icoTree->get_triangle(neighbourIndex);
        SubTriangleChunk const &neighChunk = m_triangleChunks[neighbourIndex];
        TriangleSideTransform &neighTransform = rChunk.m_neighbourTransform[side];

        neighTransform.m_scale = 1.0f;
        neighTransform.m_translation = 0.0f;

        // Get the neighbour, 3 possible cases:
        if (tri_is_chunked(neighChunk))
        {
            // Neighbour is chunked
            rNeighbourChunked = neighbourIndex;
        }
        else if (neighChunk.m_ancestorChunked != gc_invalidTri)
        {
            // Neighbour is part of a chunk
            rNeighbourChunked = neighChunk.m_ancestorChunked;
        }
        else if (neighChunk.m_descendentChunked != 0)
        {
            // Neighbour has chunked descendent, but has no chunk itself
            rNeighbourChunked = neighbourIndex;
            if (rTri.m_depth == neighTri.m_depth)
            {
                // if same depth, then set those descendent's chunked
                // neighbours to t

                uint8_t neighbourSide = rTri.m_neighbourSide[side];
                chunk_set_neighbour_recurse(neighbourIndex, neighbourSide, t);
            }
        }

        // If a neighbour was found:
        if (rNeighbourChunked != gc_invalidTri)
        {
            SubTriangle &rNeighBTri = m_icoTree->get_triangle(rNeighbourChunked);
            SubTriangleChunk &rNeighBChunk = m_triangleChunks[rNeighbourChunked];

            neighTransform = m_icoTree->transform_to_ancestor(t, side,
                    rNeighBTri.m_depth);

            uint8_t neighbourSide = rTri.m_neighbourSide[side];

            if (rTri.m_depth == rNeighBTri.m_depth)
            {

                rNeighBChunk.m_neighbourChunked[neighbourSide] = t;
                rNeighBChunk.m_neighbourTransform[side]
                        = TriangleSideTransform{0.0f, 1.0f};
            }
            else if (rTri.m_depth > rNeighBTri.m_depth)
            {
                rNeighBChunk.m_neighbourTransform[side]
                        = TriangleSideTransform{0.0f, 1.0f};
                if (rNeighBChunk.m_neighbourChunked[neighbourSide] == gc_invalidVrtx)
                {
                    rNeighBChunk.m_neighbourChunked[neighbourSide]
                            = rNeighBTri.m_neighbours[neighbourSide];
                }

            }
        }
    }

    // Step 4.1: Create initial triangle with the first 3 corners of the chunk.
    //           This either creates new vertices or takes one from a neighbour.

    using TriToSubdiv_t = std::array<VertexToSubdiv, 3>;

    TriToSubdiv_t initTri{VertexToSubdiv{0, 0, 0},
                          VertexToSubdiv{0, m_chunkWidthB, 0},
                          VertexToSubdiv{m_chunkWidthB, m_chunkWidthB, 0}};

    // indices to shared and non-shared vertices added when subdividing
    std::vector<vrindex_t> indices(m_vrtxPerChunk, gc_invalidVrtx);

    // take from neighbour or create 3 shared vertices for the first triangle
    for (int corner = 0; corner < 3; corner ++)
    {

        // corner 0 (top)   can be taken from side 1 (right)  and 2 (left)
        // corner 1 (left)  can be taken from side 2 (left)   and 0 (bottom)
        // corner 2 (right) can be taken from side 0 (bottom) and 1 (right)

        int sideA = (corner + 1) % 3;
        int sideB = (corner + 2) % 3;

        vrindex_t vertIndex = gc_invalidVrtx;

        vrindex_t triCorner = rTri.m_corners[corner] / m_icoTree->m_vrtxSize;
        vrindex_t &rSharedCorner = m_vrtxSharedIcoCorners[triCorner];

        if (rSharedCorner != gc_invalidVrtx)
        {
            // a shared triangle can be taken from a corner
            vertIndex = rSharedCorner;
            m_vrtxSharedUsers[vertIndex] ++;
        }

        if (vertIndex == gc_invalidVrtx)
        {

            // Get a vertex from either of the sides

            // try side A
            vertIndex = shared_from_neighbour(t, sideA, m_chunkWidthB);

            // if that fails, try side B
            if (vertIndex == gc_invalidVrtx)
            {
                vertIndex = shared_from_neighbour(t, sideB, 0);
            }

            // success, increment user count and stuff
            if (vertIndex != gc_invalidVrtx)
            {
                m_vrtxSharedUsers[vertIndex] ++;

                rSharedCorner = vertIndex;
                m_vrtxSharedIcoCornersReverse[vertIndex] = triCorner;
            }
        }

        // if all sharing attempts fail, then create a new one
        if (vertIndex == gc_invalidVrtx)
        {
            vertIndex = shared_create();

            rSharedCorner = vertIndex;
            m_vrtxSharedIcoCornersReverse[vertIndex] = triCorner;
        }


        buindex_t vrtxOffset = vertIndex * m_vrtxSize;
        auto vrtxOffsetIt = m_vrtxBuffer.begin() + vrtxOffset;

        // sideB because local ringed indices start at the bottom left
        indices[sideB * m_chunkWidthB] = vertIndex;
        initTri[corner].m_vrtxIndex = vertIndex;

        //float *vrtxDataRead = m_icoTree->get_vertex_pos(tri.m_corners[corner]);
        vrindex_t icoCornerVrtx = rTri.m_corners[corner];

        // Copy Icotree's position and normal data to vertex buffer

        Vector3 const& vrtxPos = *reinterpret_cast<Vector3 const*>(
                m_icoTree->get_vertex_pos(icoCornerVrtx));

        Vector3 const& vrtxNrm = *reinterpret_cast<Vector3 const*>(
                m_icoTree->get_vertex_nrm(icoCornerVrtx));

        Vector3 pos = vrtxPos;
        Vector3 nrm = pos.normalized();

        // temporary
        pos = nrm * float(m_icoTree->get_radius());
        pos += nrm * debug_stupid_heightmap(pos);

        std::copy(pos.data(), pos.data() + pos.Size,
                  vrtxOffsetIt + m_vrtxCompOffsetPos);

        std::copy(nrm.data(), nrm.data() + nrm.Size,
                  vrtxOffsetIt + m_vrtxCompOffsetNrm);

        // Make sure this data is sent to the GPU eventually
        update_range_insert(m_gpuUpdVrtxBuffer,
                            {vrtxOffset, vrtxOffset + m_vrtxSize});
    }

    // Step 4.2: Subdivide the initial triangle multiple times until the right
    //           level of detail is reached. This will fill the vertex buffer.

    std::stack<TriToSubdiv_t> m_toSubdiv;

    m_toSubdiv.push(initTri); // add the first triangle

    // keep track of the non-shared center chunk vertices
    unsigned centerIndex = 0;

    int iteration = 0; // used for debugging
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

                vrindex_t share = shared_from_neighbour(t, side, sideInd);
                //share = gc_invalidVrtx;
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
                mid[i].m_vrtxIndex = rChunk.m_dataVrtx + centerIndex;

                // Keep track of which middle index is being looped through
                centerIndex ++;

                //buindex_t vrtxDataMid = mid[i].m_vrtxIndex * m_vrtxSize;

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

            // temporary
            pos = nrm * float(m_icoTree->get_radius());
            pos += nrm * debug_stupid_heightmap(pos);

            // Add Position data to vertex buffer
            std::copy(pos.data(), pos.data() + pos.Size,
                      vrtxDataMidIt + m_vrtxCompOffsetPos);

            // Add Normal data to vertex buffer
            std::copy(nrm.data(), nrm.data() + nrm.Size,
                      vrtxDataMidIt + m_vrtxCompOffsetNrm);

            // Make sure the GPU knows about this individual vertex change
            update_range_insert(m_gpuUpdVrtxBuffer,
                                {vrtxDataMid, vrtxDataMid + m_vrtxSize});

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

        iteration ++;
    }

    // Make sure the newly added center vertices are updated to the GPU
    update_range_insert(m_gpuUpdVrtxBuffer,
                        {rChunk.m_dataVrtx,
                         rChunk.m_dataVrtx + m_vrtxPerChunk * m_vrtxSize});

    // Step 5: Vertices are done now. Connect the dots to fill the index buffer.

    // This data will be pushed directly into the chunk index buffer
    // * 3 because there are 3 indices in a triangle
    std::vector<unsigned> chunkIndData(m_indxPerChunk * 3);
    int i = 0;

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

    // Put the index data at the end of the buffer
    rChunk.m_dataIndx = rChunk.m_chunk * chunkIndData.size();
    std::copy(chunkIndData.begin(), chunkIndData.end(),
              m_indxBuffer.begin() + rChunk.m_dataIndx);

    update_range_insert(m_gpuUpdIndxBuffer,
                        {rChunk.m_dataIndx,
                         rChunk.m_dataIndx + m_indxPerChunk * 3});

    // Step 6: Update meta/tracking information for other uses.

    // Make sure descendents know that they're part of a chunk
    if (rTri.m_subdivided)
    {
        set_chunk_ancestor_recurse(rTri.m_children + 0, t);
        set_chunk_ancestor_recurse(rTri.m_children + 1, t);
        set_chunk_ancestor_recurse(rTri.m_children + 2, t);
        set_chunk_ancestor_recurse(rTri.m_children + 3, t);
    }

    // Let all ancestors know that one of their descendents had got chunky
    if (rTri.m_depth)
    {
        trindex_t curIndex = rTri.m_parent;
        SubTriangle *pCurTri;
        do
        {
            pCurTri = &(m_icoTree->get_triangle(curIndex));
            pCurTri->m_useCount ++;
            m_triangleChunks[curIndex].m_descendentChunked ++;
            curIndex = pCurTri->m_parent;
        }
        while (pCurTri->m_depth != 0);
    }

    // This new chunk depends on the IcoSphereTree's SubTriangle, so make sure
    // it isn't deleted using reference counting.
    rTri.m_useCount ++;

    // Step 7 (Maybe Temporary): Flatten seams on neighbouring chunks of
    //                           different levels of detail.

    for (uint8_t side = 0; side < 3; side ++)
    {
        trindex_t neighInd = rChunk.m_neighbourChunked[side];

        if (neighInd == gc_invalidTri)
        {
            continue;
        }

        SubTriangle const &neighTri = m_icoTree->get_triangle(neighInd);
        SubTriangleChunk const &neighChunk = m_triangleChunks[neighInd];

        if (neighChunk.m_chunk != gc_invalidChunk)
        {
            // Fix own edge to blend with larger chunked neighbour
            chunk_edge_transition(t, side, neighTri.m_depth);
        }
        else
        {
            // Fix neighbour's edge
            chunk_edge_transition_recurse(neighInd, rTri.m_neighbourSide[side],
                                          rTri.m_depth);
        }
    }
    //debug_verify_state();
}

void PlanetGeometryA::chunk_edge_transition(trindex_t t, uint8_t side,
                                            uint8_t depth)
{
    SubTriangle const& tri = m_icoTree->get_triangle(t);
    SubTriangleChunk const& chunk = m_triangleChunks[t];

    //unsigned start = m_chunkWidthB * unsigned(side);
    //unsigned stop = start + m_chunkWidthB;
    unsigned step = (1 << (tri.m_depth - depth));

    for (unsigned i = 0; i < m_chunkWidthB; i += step)
    {
        vrindex_t vrtxA = shared_from_tri(chunk, side, i);
        vrindex_t vrtxB = shared_from_tri(chunk, side, i + step);
        Vector3 const &posA = get_vertex_component<Vector3>(vrtxA,
                                                      m_vrtxCompOffsetPos);
        Vector3 const &posB = get_vertex_component<Vector3>(vrtxB,
                                                      m_vrtxCompOffsetPos);

        Vector3 dir = (posB - posA) / step;

        //std::cout << "dir: " << dir.length() << "\n";

        for (unsigned j = 1; j < step; j ++)
        {
            vrindex_t vrtxMid = shared_from_tri(chunk, side, i + j);
            Vector3 &posMid = get_vertex_component<Vector3>(
                                vrtxMid, m_vrtxCompOffsetPos);

            posMid = posA + dir * j;
            //posMid *= 0;


            // Make sure this data is sent to the GPU eventually
            update_range_insert(m_gpuUpdVrtxBuffer,
                                {vrtxMid * m_vrtxSize, vrtxMid * (m_vrtxSize + 1)});
        }
    }
}

void PlanetGeometryA::chunk_edge_transition_recurse(trindex_t t, uint8_t side,
                                                    uint8_t depth)
{
    SubTriangle const &tri = m_icoTree->get_triangle(t);
    SubTriangleChunk const &chunk = m_triangleChunks[t];

    if (chunk.m_chunk != gc_invalidChunk)
    {
        chunk_edge_transition(t, side, depth);
    }
    else if (tri.m_subdivided)
    {
        // Side 0(bottom) corresponds to child triangle 1(left),  2(right)
        // Side 1(right)  corresponds to child triangle 2(right), 0(top)
        // Side 2(left)   corresponds to child triangle 0(top),   1(left)
        chunk_edge_transition_recurse(tri.m_children + ((side + 1) % 3),
                                      side, depth);
        chunk_edge_transition_recurse(tri.m_children + ((side + 2) % 3),
                                      side, depth);
    }
}

void PlanetGeometryA::chunk_set_neighbour_recurse(
        trindex_t t, uint8_t side, trindex_t to)
{
    SubTriangle const &tri = m_icoTree->get_triangle(t);
    SubTriangleChunk &rChunk = m_triangleChunks[t];

    if (rChunk.m_chunk != gc_invalidChunk)
    {
        rChunk.m_neighbourChunked[side] = to;

        if (to != gc_invalidTri)
        {
            rChunk.m_neighbourTransform[side] = m_icoTree->transform_to_ancestor(
                        t, side, m_icoTree->get_triangle(to).m_depth);
        }
        else
        {
            rChunk.m_neighbourTransform[side] = TriangleSideTransform{0.0f, 1.0f};
        }
        return;
    }
    else if (tri.m_subdivided)
    {
        chunk_set_neighbour_recurse(tri.m_children + ((side + 1) % 3),
                                    side, to);
        chunk_set_neighbour_recurse(tri.m_children + ((side + 2) % 3),
                                    side, to);
    }
}


void PlanetGeometryA::chunk_triangle_assure()
{
    trindex_t min = std::max<trindex_t>(m_triangleChunks.size(),
                                        m_icoTree->triangle_count());
    m_triangleChunks.resize(min, {gc_invalidChunk, 0, gc_invalidTri, 0, 0,
                                  {gc_invalidTri, gc_invalidTri, gc_invalidTri},
                                  {}});
}


void PlanetGeometryA::chunk_remove(trindex_t t)
{
    //std::cout << "chunk_remove(" << t << ");\n";
    SubTriangle &rTri = m_icoTree->get_triangle(t);
    SubTriangleChunk &rChunk = m_triangleChunks[t];


    if (rChunk.m_chunk == gc_invalidChunk)
    {
        // return if not chunked
        return;
    }


    // Delete Verticies

    // Mark middle vertices for replacement
    m_vrtxFree.push_back(rChunk.m_dataVrtx);

    // Now delete shared vertices

    unsigned* triIndData = m_indxBuffer.data() + rChunk.m_dataIndx;

    for (unsigned i = 0; i < m_vrtxSharedPerChunk; i ++)
    {
        vrindex_t sharedIndex = *(triIndData + m_indToShared[i]);

        // Decrease number of users
        m_vrtxSharedUsers[sharedIndex] --;

        if (m_vrtxSharedUsers[sharedIndex] == 0)
        {
            // If users is zero, then delete
            //std::cout << "shared removed: " << sharedIndex << "\n";
            m_vrtxSharedFree.push_back(sharedIndex);
            m_vrtxSharedCount --;

            // check if a corner was deleted
            vrindex_t &rCorner = m_vrtxSharedIcoCornersReverse[sharedIndex];

            if (rCorner != gc_invalidVrtx)
            {
                // clear entry in m_vrtxSharedIcoCorners too
                m_vrtxSharedIcoCorners[rCorner] = gc_invalidVrtx;
                rCorner = gc_invalidVrtx;
            }
        }
    }

    for (uint8_t i = 0; i < 3; i ++)
    {
        // Remove from neighbours
        trindex_t &rNeighInd = rChunk.m_neighbourChunked[i];

        if (rNeighInd == gc_invalidTri)
        {
            continue;
        }

        SubTriangle const& neighTri = m_icoTree->get_triangle(rNeighInd);
        //SubTriangleChunk const& neighChunk = m_triangleChunks[rNeighInd];

        if (neighTri.m_depth == rTri.m_depth)
        {
            chunk_set_neighbour_recurse(rNeighInd, rTri.m_neighbourSide[i], gc_invalidTri);
        }

        rNeighInd = gc_invalidTri; // now delete own value
    }

    // Make sure descendents know that they're no longer part of a chunk
    if (rTri.m_subdivided)
    {
        set_chunk_ancestor_recurse(rTri.m_children + 0, gc_invalidTri);
        set_chunk_ancestor_recurse(rTri.m_children + 1, gc_invalidTri);
        set_chunk_ancestor_recurse(rTri.m_children + 2, gc_invalidTri);
        set_chunk_ancestor_recurse(rTri.m_children + 3, gc_invalidTri);
    }

    // Let all ancestors know that lost one of their chunky descendents
    if (rTri.m_depth != 0)
    {
        trindex_t curIndex = rTri.m_parent;
        SubTriangle *pCurrentTri;
        do
        {
            pCurrentTri = &(m_icoTree->get_triangle(curIndex));
            pCurrentTri->m_useCount --;
            m_triangleChunks[curIndex].m_descendentChunked --;
            curIndex = pCurrentTri->m_parent;
        }
        while (pCurrentTri->m_depth != 0);
    }

    rTri.m_useCount --;

    // mark this chunk for replacement
    m_chunkFree.emplace(rChunk.m_chunk);

    // set to not chunked
    rChunk.m_chunk = gc_invalidChunk;
}

void PlanetGeometryA::chunk_remove_descendents_recurse(trindex_t t)
{
    SubTriangle const &tri = m_icoTree->get_triangle(t);
    SubTriangleChunk const &chunk = m_triangleChunks[t];

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
    SubTriangle const &tri = m_icoTree->get_triangle(t);
    SubTriangleChunk const &chunk = m_triangleChunks[t];

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
    std::cout << "chunk_pack();\n";
    for (chindex_t chunk : m_chunkFree)
    {
        m_chunkCount --;

        // Get the associated triangle of removed chunk
        trindex_t replaceT = m_chunkToTri[chunk];

        // Get chunk and triangle data of removed
        //SubTriangle &replaceTri = m_icoTree->get_triangle(replaceT);
        SubTriangleChunk &rReplaceChunk = m_triangleChunks[replaceT];

        if (chunk == m_chunkCount)
        {
            // chunk on the end of the buffer was removed. the buffer size
            // shrinks and nothing happens.

            // Set chunked
            rReplaceChunk.m_chunk = gc_invalidChunk;
            continue;
        }

        // Get the associated triangle of the last chunk in the chunk buffer
        trindex_t moveT = m_chunkToTri[m_chunkCount];

        // Get chunk and triangle data of to move
        //SubTriangle &moveTri = m_icoTree->get_triangle(moveT);
        SubTriangleChunk &moveChunk = m_triangleChunks[moveT];

        // Do move

        // Replace tri's domain location with lastTriangle
        m_chunkToTri[chunk] = m_chunkToTri[moveChunk.m_chunk];

        // Move lastTri's index data to replace tri's data
        std::copy(m_indxBuffer.begin() + moveChunk.m_dataIndx,
                  m_indxBuffer.begin() + moveChunk.m_dataIndx
                                       + m_indxPerChunk * 3,
                  m_indxBuffer.begin() + rReplaceChunk.m_dataIndx);

        update_range_insert(m_gpuUpdIndxBuffer,
                            {rReplaceChunk.m_dataIndx,
                             rReplaceChunk.m_dataIndx + m_indxPerChunk * 3});

        // Change lastTriangle's chunk index to tri's
        moveChunk.m_dataIndx = rReplaceChunk.m_dataIndx;
        moveChunk.m_chunk = chunk;
    }

    m_chunkFree.clear();

    //debug_verify_state();
}

void PlanetGeometryA::set_chunk_ancestor_recurse(trindex_t triInd,
                                                 trindex_t setTo)
{
    SubTriangle &tri = m_icoTree->get_triangle(triInd);

    m_triangleChunks[triInd].m_ancestorChunked = setTo;

    if (!tri.m_subdivided)
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

    vrindex_t localIndex = (side * m_chunkWidthB + pos) % m_vrtxSharedPerChunk;

    vrindex_t sharedOut = m_indxBuffer[
            chunk.m_dataIndx + m_indToShared[localIndex]];
    //m_vrtxSharedUsers[sharedOut] ++;

    return sharedOut;
}

vrindex_t PlanetGeometryA::shared_from_neighbour(
        trindex_t triInd, uint8_t side,loindex_t posIn)
{
    SubTriangle& tri = m_icoTree->get_triangle(triInd);
    SubTriangleChunk& chunk = m_triangleChunks[triInd];

    trindex_t neighInd = chunk.m_neighbourChunked[side];

    if (neighInd == gc_invalidTri)
    {
        return gc_invalidVrtx;
    }

    float posTransformed = float(posIn) / float(m_chunkWidthB);

    trindex_t takeInd;
    uint8_t takeSide = tri.m_neighbourSide[side];

    {
        SubTriangle& neighTri = m_icoTree->get_triangle(neighInd);
        SubTriangleChunk& neighChunk = m_triangleChunks[neighInd];

        if (neighChunk.m_chunk != gc_invalidChunk)
        {
            takeInd = chunk.m_neighbourChunked[side];
        }
        else // if neigh is same depth and contains chunked descendents.
        {
            // TODO: deal with 0.5f
            // has chunked children, grand children, etc... search for them
            // side 0 (bottom) : children 1->2
            // side 1  (right) : children 2->0
            // side 2   (left) : children 0->1

            float posChild = 1.0f - posTransformed;

            SubTriangleChunk *triChildChunk;
            SubTriangle *triChildTri = &neighTri;
            trindex_t triNextChild;

            do
            {
                if (!triChildTri->m_subdivided)
                {
                    return gc_invalidVrtx;
                }

                bool useLatterChild = posChild > 0.5f;
                triNextChild = triChildTri->m_children
                             + (takeSide + 1 + useLatterChild) % 3;

                triChildTri = &(m_icoTree->get_triangle(triNextChild));
                triChildChunk = &m_triangleChunks[triNextChild];

                posChild -= 0.5f * useLatterChild;
                posChild *= 2;

            }
            while (triChildChunk->m_chunk == gc_invalidChunk);

            takeInd = triNextChild;
        }
    }

    SubTriangle &takeTri = m_icoTree->get_triangle(takeInd);
    SubTriangleChunk &takeChunk = m_triangleChunks[takeInd];

    // Detect if trying to get a space between vertices using divisibility by
    // powers of 2
    if ((tri.m_depth > takeTri.m_depth))
    {
        if (posIn % (1 << (tri.m_depth - takeTri.m_depth)) != 0)
        {
            return gc_invalidVrtx;
        }
    }

    TriangleSideTransform tranFrom = chunk.m_neighbourTransform[side];
    TriangleSideTransform tranTo = takeChunk.m_neighbourTransform[takeSide];

    // apply transform from
    posTransformed = posTransformed * tranFrom.m_scale + tranFrom.m_translation;
    // invert
    posTransformed = 1.0f - posTransformed;
    // inverse with transform to
    posTransformed = (posTransformed - tranTo.m_translation) / tranTo.m_scale;

    loindex_t posOut = std::round(posTransformed * m_chunkWidthB);

    return shared_from_tri(takeChunk, takeSide, posOut);
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
    if (m_vrtxSharedFree.empty())
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

    //std::cout << "shared create: " << sharedOut << "\n";

    return sharedOut;
}

void PlanetGeometryA::updates_clear()
{
    m_gpuUpdIndxBuffer.clear();
    m_gpuUpdVrtxBuffer.clear();
}


unsigned PlanetGeometryA::debug_chunk_count_descendents(SubTriangle const& tri)
{
    unsigned count = 0;

    if (tri.m_subdivided)
    {
        for (trindex_t i = 0; i < 4; i ++)
        {
            SubTriangle const &childTri = m_icoTree->get_triangle(tri.m_children + i);
            SubTriangleChunk const &childChunk = m_triangleChunks[tri.m_children + i];

            if (tri_is_chunked(childChunk))
            {
                count ++;
            }

            count += debug_chunk_count_descendents(childTri);
        }
    }

    return count;
}

bool PlanetGeometryA::debug_verify_state()
{
    // Things to do:
    // Verify shared vertices:
    // * For all chunked triangles, count shared vertex uses
    // * Make sure no deleted vertices are being used
    // Verify that m_chunkCount is the same as number of chunked triangles
    // Verify chunk hierarchy (m_descendentChunked and m_ancestorChunked):
    // * Recurse through through all descendents and count chunked ones
    // * Go up chain of parents and see if there's a chunked one
    // Verify vertex sharing if chunked
    // * Loop through shared vertices and make sure the neighbours use them too

    std::cout << "PlanetGeometryA Verify:\n";

    std::vector<uint8_t> recountVrtxSharedUsers(m_vrtxSharedUsers.size(), 0);

    bool error = false;

    unsigned chunkCount = 0;

    for (trindex_t t = 0; t < m_triangleChunks.size(); t ++)
    {
        SubTriangle &tri = m_icoTree->get_triangle(t);
        SubTriangleChunk &chunk = m_triangleChunks[t];

        if (tri.m_deleted)
        {
            continue;
        }

        // Verify chunk hierarchy

        // Count chunked descendents

        unsigned countDescendentChunked = debug_chunk_count_descendents(tri);

        if (countDescendentChunked != chunk.m_descendentChunked)
        {
            std::cout << "* Invalid chunk " << t << ": "
                      << "Incorrect chunked descendent count\n";
            error = true;
        }

        // Verify chunked ancestor

        trindex_t ancestorChunked = gc_invalidChunk;

        if (tri.m_depth > 0)
        {
            trindex_t curIndex = tri.m_parent;
            SubTriangle const *curTri;
            do
            {
                curTri = &(m_icoTree->get_triangle(curIndex));
                if (tri_is_chunked(m_triangleChunks[curIndex]))
                {
                    ancestorChunked = curIndex;
                    break;
                }
                curIndex = curTri->m_parent;
            }
            while (curTri->m_depth != 0);

            if (ancestorChunked != chunk.m_ancestorChunked)
            {
                std::cout << "* Invalid chunk " << t << ": "
                          << "Incorrect chunked ancestor\n";
                error = true;
            }
        }

        if (tri_is_chunked(chunk))
        {
            chunkCount ++;

            // Count shared vertices

            unsigned* triIndData = m_indxBuffer.data() + chunk.m_dataIndx;

            for (unsigned i = 0; i < m_vrtxSharedPerChunk; i ++)
            {
                vrindex_t sharedIndex = *(triIndData + m_indToShared[i]);
                recountVrtxSharedUsers[sharedIndex] ++;
            }
        }
    }

    if (chunkCount + m_chunkFree.size() != m_chunkCount)
    {
        std::cout << "* Invalid chunk count\n";
        error = true;
    }

    if (recountVrtxSharedUsers != m_vrtxSharedUsers)
    {
        std::cout << "* Invalid Shared vertex user count\n";
        for (unsigned i = 0; i < m_vrtxSharedMax; i ++)
        {
            if (m_vrtxSharedUsers[i] != recountVrtxSharedUsers[i])
            {
                std::cout << "  * Vertex: " << i << "\n"
                          << "\n    * Expected: " << int(recountVrtxSharedUsers[i])
                          << "\n    * Obtained: " << int(m_vrtxSharedUsers[i]);
            }
        }

        std::cout << "\n";

        error = true;
    }

    return error;
}


void PlanetGeometryA::debug_raise_by_share_count()
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

        pos = nrm * m_icoTree->get_radius() + nrm * raise;
    }
}

void PlanetGeometryA::on_ico_triangles_added(
        std::vector<trindex_t> const& added)
{
    //std::cout << "foo!\n";
}

void PlanetGeometryA::on_ico_triangles_removed(
        std::vector<trindex_t> const& removed)
{
    //std::cout << "bar!\n";
    for (trindex_t t : removed)
    {
        m_triangleChunks[t].m_ancestorChunked = gc_invalidTri;
        m_triangleChunks[t].m_descendentChunked = 0;

        // remove some shared corners
    }
}

void PlanetGeometryA::on_ico_vertex_removed(
        std::vector<vrindex_t> const& vrtxRemoved)
{
    for (vrindex_t vrtx : vrtxRemoved)
    {
        vrindex_t &shared = m_vrtxSharedIcoCorners[vrtx];

        if (shared != gc_invalidVrtx) // some corners are deleted properly
        {
            // Mark as deleted from both arrays
            m_vrtxSharedIcoCornersReverse[shared] = gc_invalidVrtx;
            shared = gc_invalidVrtx;
        }
    }
}
