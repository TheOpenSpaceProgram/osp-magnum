#pragma once

#include "IcoSphereTree.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace osp
{

// Index to a chunk
using chindex_t = uint32_t;

// Index to a vertex
using vrindex_t = uint32_t;

constexpr chindex_t gc_invalidChunk = std::numeric_limits<chindex_t>::max();

struct UpdateRange;

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


struct SubTriangleChunk
{
    // Index to chunk. (First triangle ever chunked will be 0)
    // equal to m_chunkMax when not chunked
    chindex_t m_chunk;
    buindex_t m_chunkIndx; // Index to index data in the index buffer
    buindex_t m_chunkVrtx; // Index to vertex data
};


class PlanetGeometryA
{


public:

    PlanetGeometryA() = default;
    ~PlanetGeometryA() = default;

    constexpr bool is_initialized() const { return m_initialized; }

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

    std::vector<float> const& get_vertex_buffer() { return m_vrtxBuffer; }
    std::vector<unsigned> const& get_index_buffer() { return m_indxBuffer; }
    buindex_t calc_index_count() { return m_chunkCount * m_indxPerChunk * 3; }

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
    constexpr unsigned get_index(int x, int y) const;

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
    unsigned get_index_ringed(unsigned x, unsigned y) const;

    /**
     * Grab a shared vertex from the side of a triangle.
     * @param sharedIndex [out] Set to index to shared vertex when successful
     * @param tri [in] Triangle to grab a
     * @param side [in] 0: bottom, 1: right, 2: left
     * @param pos [in] float from (usually) 0.0-1.0, position of vertex to grab
     * @return true when a shared vertex is grabbed successfully
     *         false when a new shared vertex is created
     */
    bool shared_from_tri(vrindex_t& rSharedIndex, const SubTriangleChunk& chunk,
                         unsigned side, int pos);


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

    std::vector<buindex_t> m_vrtxFree; // Deleted chunk data to overwrite

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

struct UpdateRange
{
    buindex_t m_start;
    buindex_t m_end;
};

}
