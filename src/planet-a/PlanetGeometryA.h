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
#pragma once

#include "IcoSphereTree.h"

#include <Corrade/Containers/EnumSet.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <queue>
#include <set>
#include <vector>

namespace planeta
{

// Index to a chunk
using chindex_t = uint32_t;

// Index local to a chunk, from (0 ... m_vrtxPerChunk)
using loindex_t = uint32_t;

// Index to a vertex
using vrindex_t = uint32_t;

constexpr chindex_t gc_invalidChunk = std::numeric_limits<chindex_t>::max();
constexpr loindex_t gc_invalidLocal = std::numeric_limits<loindex_t>::max();
constexpr trindex_t gc_invalidTri = std::numeric_limits<trindex_t>::max();
constexpr vrindex_t gc_invalidVrtx = std::numeric_limits<vrindex_t>::max();

enum class EChunkUpdateAction { Nothing, Subdivide, Unsubdivide,
                                Chunk, Unchunk };

struct UpdateRangeSub;

// based on urho-osp PlanetWrenderer.cpp
// variable names changed:
// m_chunkCount           -> m_chunkCount
// m_chunkIndDomain       -> m_chunkToTri
// m_chunkVertFree        -> m_vrtxFree
// m_chunkVertFreeShared  -> m_vrtxSharedFree
// m_chunkVertUsers       -> m_vrtxSharedUsers
// m_chunkSharedIndices   -> m_indToShared
// m_chunkVertCountShared -> m_vrtxSharedCount
// m_chunkMaxVert         -> m_vrtxMax
// m_chunkMaxVertShared   -> m_vrtxSharedMax
// m_maxChunks            -> m_chunkMax
// m_chunkAreaThreshold   -> m_chunkAreaThreshold
// m_chunkResolution      -> m_chunkWidth
// m_chunkVertsPerSide    -> m_chunkWidthB
// m_chunkSharedCount     -> m_vrtxSharedPerChunk
// m_chunkSize            -> m_vrtxPerChunk
// m_chunkSizeInd         -> m_indPerChunk

//enum class EChunk: uint8_t
//{
//};


struct SubTriangleChunk
{
    // Index to chunk. (First triangle ever chunked will be 0)
    // set to m_chunkMax when not chunked
    chindex_t m_chunk;

    // Number of descendents that are chunked.
    // Used to make sure that triangles aren't chunked when they already have
    // chunked children, and for some shared vertex calculations
    unsigned m_descendentChunked;

    // Index to chunked ancestor if present
    // set to IcoSphereTree.m_maxTriangles if invalid
    trindex_t m_ancestorChunked;

    buindex_t m_dataIndx; // Index to index data in the index buffer
    buindex_t m_dataVrtx; // Index to vertex data
};


struct UpdateRangeSub
{
    buindex_t m_start;
    buindex_t m_end;
};

void update_range_insert(std::vector<UpdateRangeSub>& range,
                         UpdateRangeSub insert);

class PlanetGeometryA
{

public:

    class IteratorTriIndexed;

    PlanetGeometryA() = default;
    ~PlanetGeometryA() = default;

    constexpr bool is_initialized() const { return m_initialized; }

    constexpr unsigned indices_per_chunk() const { return m_indxPerChunk; }

    /**
     * Calculate initial icosahedron and initialize buffers.
     * Call before drawing
     * @param size [in] Minimum height of planet, or radius
     */
    void initialize(float size);

    /**
     * Print out information on vertice count, chunk count, etc...
     */
    void log_stats() const;

    /**
     * @tparam FUNC_T
     */
    template<typename FUNC_T>
    void chunk_geometry_update_all(FUNC_T condition);

    std::pair<IteratorTriIndexed, IteratorTriIndexed> iterate_chunk(chindex_t c);

    constexpr std::vector<float> const& get_vertex_buffer() const { return m_vrtxBuffer; }
    constexpr std::vector<unsigned> const& get_index_buffer() const { return m_indxBuffer; }

    constexpr std::vector<UpdateRangeSub>& get_vertex_buffer_updates() { return m_gpuUpdVrtxBuffer; }
    constexpr std::vector<UpdateRangeSub>& get_index_buffer_updates() { return m_gpuUpdIndxBuffer; }

    constexpr buindex_t calc_index_count() { return m_chunkCount * m_indxPerChunk * 3; }
    constexpr chindex_t chunk_count() { return m_chunkCount; }

    IcoSphereTree* get_ico_tree() { return m_icoTree.get(); }

private:

    /**
     * Recursively check every triangle for which ones need
     * (un)subdividing or chunking
     * @param t [in] Triangle to start with
     */
    void sub_recurse(trindex_t t);

    /**
     *
     * @param t [in] Index of triangle to add chunk to
     */
    void chunk_add(trindex_t t);

    /**
     * @brief chunk_remove
     * @param t
     * @param gpuIgnore
     */
    void chunk_remove(trindex_t t);

    void chunk_pack();

    /**
     * @tparam FUNC_T
     */
    template<typename FUNC_T>
    void chunk_geometry_update_recurse(FUNC_T condition, trindex_t t,
                                       std::vector<trindex_t> &toChunk);

    /**
     * Convert XY coordinates to a triangular number index
     *
     * 0
     * 1  2
     * 3  4  5
     * 6  7  8  9
     * x = right, y = down
     *
     * @param x [in]
     * @param y [in]
     * @return
     */
    constexpr loindex_t get_index(int x, int y) const;

    /**
     * Similar to the normal get_index, but the first possible indices returned
     * makes a border around the triangle
     *
     * 6
     * 7  5
     * 8  9  4
     * 0  1  2  3
     * x = right, y = down
     *
     * 0, 1, 2, 3, 4, 5, 6, 7, 8 makes a ring
     *
     * @param x [in]
     * @param y [in]
     * @return
     */
    loindex_t get_index_ringed(unsigned x, unsigned y) const;

    /**
     * Grab a shared vertex from the side of a triangle.
     * @param tri [in] Triangle to grab a
     * @param side [in] 0: bottom, 1: right, 2: left
     * @param pos [in] float from (usually) 0.0-1.0, position of vertex to grab
     * @return true when a shared vertex is grabbed successfully
     *         false when a new shared vertex is created
     */
    vrindex_t shared_from_tri(SubTriangleChunk const& chunk, uint8_t side, loindex_t pos);

    /**
     *
     * @return
     */
    vrindex_t shared_from_neighbour(trindex_t triInd, uint8_t side, loindex_t posIn, bool &rBetween);

    /**
     * Create a new shared vertex. This will get a vertex from m_vrtxSharedFree
     * or create a new one entirely.
     * @return index to new shared vertex, or gc_invalidVrtx if buffer full
     */
    vrindex_t shared_create();

    void set_chunk_ancestor_recurse(trindex_t triInd, trindex_t setTo);

    bool m_initialized = false;

    std::shared_ptr<IcoSphereTree> m_icoTree;

    // 6 components per vertex
    // PosX, PosY, PosZ, NormX, NormY, NormZ
    int m_vrtxSize = 6;
    int m_vrtxCompOffsetPos = 0;
    int m_vrtxCompOffsetNrm = 3;


    // Main buffer stuff

    std::vector<unsigned> m_indxBuffer;
    std::vector<float> m_vrtxBuffer;

    // How much of m_vertBuffer is reserved for shared vertices
    buindex_t m_vrtxSharedMax;

    buindex_t m_vrtxMax; // Calculated max number of vertices

    // Chunk stuff

    chindex_t m_chunkCount; // How many chunks there are right now

    chindex_t m_chunkMax; // Max number of chunks

    // parallel with IcoSphereTree's m_triangles
    std::vector<SubTriangleChunk> m_triangleChunks;

    std::vector<trindex_t> m_chunkToTri; // Maps chunks to triangles

    // Deleted chunks to overwrite
    // Make sure this is empty before rendering
    std::set<chindex_t, std::greater<chindex_t>> m_chunkFree;

    std::vector<buindex_t> m_vrtxFree; // Deleted chunk vertex data to overwrite

    unsigned m_vrtxSharedPerChunk; // How many shared verticies per chunk
    unsigned m_vrtxPerChunk; // How many vertices there are in each chunk
    unsigned m_indxPerChunk; // How many triangles in each chunk
    unsigned m_chunkWidth; // How many vertices wide each chunk is
    unsigned m_chunkWidthB; // = m_chunkWidth - 1

    // Shared Vertex stuff

    buindex_t m_vrtxSharedCount; // Current number of shared vertices

    // individual shared vertices that are deleted
    std::vector<buindex_t> m_vrtxSharedFree;

    // Count how many times each shared chunk vertex is being used
    // it's impossible for a shared vertex to have more than 6 users
    // Delete a shared vertex when it's users goes to zero
    // And use user count to calculate normals
    std::vector<uint8_t> m_vrtxSharedUsers;

    // Associates IcoSphereTree verticies with a shared vertex
    // Indices to m_vrtxSharedUsers, parallel with IcoSphereTree m_vrtxBuffer
    std::vector<buindex_t> m_vrtxSharedIcoCorners;

    // Maps shared vertex indices to the index buffer, so that shared vertices
    // can be obtained from a chunk's index data
    std::vector<buindex_t> m_indToShared;

    // GPU update stuff

    std::vector<UpdateRangeSub> m_gpuUpdVrtxBuffer;
    std::vector<UpdateRangeSub> m_gpuUpdIndxBuffer;

    // How much screen area a triangle can take before it should be chunked
    //float m_chunkAreaThreshold = 0.04f;

    // Not implented, for something like running a server
    //
    //bool m_noGPU = false;

    // Vertex buffer data is divided unevenly for chunks
    // In m_chunkVertBuf:
    // [shared vertex data, shared vertices]
    //                     ^               ^
    //        (m_vrtxSharedMax)    (m_chunkMaxVert)

    // if chunk resolution is 16, then...
    // Chunks are triangles of 136 vertices (m_chunkSize)
    // There are 45 vertices on the edges, (sides + corners)
    // = (14 + 14 + 14 + 3) = m_chunkSharedCount;
    // Which means there is 91 vertices left in the middle
    // (m_chunkSize - m_chunkSharedCount)

};

class PlanetGeometryA::IteratorTriIndexed :
        std::vector<unsigned>::const_iterator
{
public:

    using IndIt_t = std::vector<unsigned>::const_iterator;

    IteratorTriIndexed() = default;
    IteratorTriIndexed(std::vector<unsigned>::const_iterator indx,
                       std::vector<float> const& vrtxBuffer) :
            std::vector<unsigned>::const_iterator(indx),
            m_vrtxBuffer(&vrtxBuffer) {};
    IteratorTriIndexed(IteratorTriIndexed const &copy) = default;
    IteratorTriIndexed(IteratorTriIndexed &&move) = default;

    IteratorTriIndexed& operator=(IteratorTriIndexed&& move) = default;

    float const* position()
    {
        return m_vrtxBuffer->data() + (**this * m_vrtxSize) + m_vrtxCompOffsetPos;
    };
    float const* normal()
    {
        return m_vrtxBuffer->data() + (**this * m_vrtxSize) + m_vrtxCompOffsetNrm;
    };

    //using std::vector<unsigned>::const_iterator::operator==;

    //using std::vector<unsigned>::const_iterator::operator+;
    //using std::vector<unsigned>::const_iterator::operator-;
    using std::vector<unsigned>::const_iterator::operator++;
    using std::vector<unsigned>::const_iterator::operator--;
    using std::vector<unsigned>::const_iterator::operator+=;
    using std::vector<unsigned>::const_iterator::operator-=;

    IteratorTriIndexed operator+(difference_type rhs) { return IteratorTriIndexed(static_cast<IndIt_t>(*this) + rhs, *m_vrtxBuffer); }


    bool operator==(IteratorTriIndexed const &rhs) { return static_cast<IndIt_t>(*this) == static_cast<IndIt_t>(rhs); }
    bool operator!=(IteratorTriIndexed const &rhs) { return static_cast<IndIt_t>(*this) != static_cast<IndIt_t>(rhs); }

private:

    //unsigned m_indxPerChunk;
    //std::vector<unsigned> const* m_indxBuffer;

    // TODO: set these properly
    int m_vrtxSize = 6;
    int m_vrtxCompOffsetPos = 0;
    int m_vrtxCompOffsetNrm = 3;

    std::vector<float> const* m_vrtxBuffer;
};

template<typename FUNC_T>
void PlanetGeometryA::chunk_geometry_update_all(FUNC_T condition)
{
    // loop through triangles, see which ones to chunk, subdivide, etc...
    std::vector<trindex_t> toChunk;

    // subdivision/unsubdivision can be done right away
    // un-chunking can be done right away
    // chunking should all be done at the end

    // Loop through initial 20 triangles of icosahedron
    for (trindex_t t = 0; t < gc_icosahedronFaceCount; t ++)
    {
        chunk_geometry_update_recurse(condition, t, toChunk);
    }

    for (trindex_t t : toChunk)
    {
        chunk_add(t);
    }

    if (m_chunkFree.size() != 0)
    {
        // there's some chunks to move
        chunk_pack();
    }
}

template<typename FUNC_T>
void PlanetGeometryA::chunk_geometry_update_recurse(FUNC_T condition,
        trindex_t t, std::vector<trindex_t> &toChunk)
{
    SubTriangle const& tri = m_icoTree->get_triangle(t);
    SubTriangleChunk const& chunk = m_triangleChunks[t];

    // use condition to determine what should be done to this triangle
    EChunkUpdateAction action = condition(tri, chunk, t);

    switch (action)
    {
    case EChunkUpdateAction::Chunk:
        toChunk.push_back(t);
        break;
    case EChunkUpdateAction::Unchunk:
        chunk_remove(t); // chunks can be removed right away
        break;
    case EChunkUpdateAction::Subdivide:
        m_icoTree->subdivide_add(t);
        break;
    case EChunkUpdateAction::Unsubdivide:
        //m_icoTree->subdivide_remove(t);
        break;
    }

    if (tri.m_bitmask & gc_triangleMaskSubdivided)
    {
        // Recurse into children
        chunk_geometry_update_recurse(condition, tri.m_children + 0, toChunk);
        chunk_geometry_update_recurse(condition, tri.m_children + 1, toChunk);
        chunk_geometry_update_recurse(condition, tri.m_children + 2, toChunk);
        chunk_geometry_update_recurse(condition, tri.m_children + 3, toChunk);
    }
}


}
