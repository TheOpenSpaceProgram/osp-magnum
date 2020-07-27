#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "IcoSphereTree.h"

namespace osp
{

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
    buindex calc_index_count() { return m_chunkCount * m_indxPerChunk * 3; }

    IcoSphereTree* get_ico_tree() { return m_icoTree.get(); }


private:

    /**
     * Recursively check every triangle for which ones need
     * (un)subdividing or chunking
     * @param t [in] Triangle to start with
     */
    void sub_recurse(trindex t);

    /**
     *
     * @param t [in] Index of triangle to add chunk to
     * @param gpuIgnore
     */
    void chunk_add(trindex t);

    /**
     * @brief chunk_remove
     * @param t
     * @param gpuIgnore
     */
    void chunk_remove(trindex t);

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
    bool shared_from_tri(buindex& rSharedIndex, const SubTriangle& tri,
                         unsigned side, int pos);


    // 6 components per vertex
    // PosX, PosY, PosZ, NormX, NormY, NormZ
    int m_vrtxSize = 6;
    int m_vrtxCompOffsetPos = 0;
    int m_vrtxCompOffsetNrm = 3;

    std::shared_ptr<IcoSphereTree> m_icoTree;
    std::vector<unsigned> m_indxBuffer;
    std::vector<float> m_vrtxBuffer;

    chindex m_chunkCount; // How many chunks there are right now

    std::vector<trindex> m_chunkToTri; // Maps chunks to triangles
    // Spots in the index buffer that want to die
    //Urho3D::PODVector<chindex> m_chunkIndDeleteMe;

    // List of deleted chunk data to overwrite
    std::vector<buindex> m_vrtxFree;
    // same as above but for individual shared verticies
    std::vector<buindex> m_vrtxSharedFree;

    // it's impossible for a shared vertex to have more than 6 users
    // Delete a shared vertex when it's users goes to zero
    // And use user count to calculate normals

    // Count how many times each shared chunk vertex is being used
    std::vector<uint8_t> m_vrtxSharedUsers;

    // Maps shared vertex indices to the index buffer, so that shared vertices
    // can be obtained from a chunk's index data
    std::vector<buindex> m_indToShared;

    buindex m_vrtxSharedCount; // Current number of shared vertices

    float m_cameraDist;
    float m_threshold;

    // Approx. screen area a triangle can take before it should be subdivided
    float m_subdivAreaThreshold = 0.02f;

    // Preferred total size of chunk vertex buffer (m_vertBuffer)
    buindex m_vrtxMax;
    // How much of m_vertBuffer is reserved for shared vertices
    buindex m_vrtxSharedMax;
    chindex m_chunkMax; // Max number of chunks

    // How much screen area a triangle can take before it should be chunked
    float m_chunkAreaThreshold = 0.04f;

    unsigned m_chunkWidth; // How many vertices wide each chunk is
    unsigned m_chunkWidthB; // = m_chunkWidth - 1
    unsigned m_vrtxSharedPerChunk; // How many shared verticies per chunk
    unsigned m_vrtxPerChunk; // How many vertices there are in each chunk
    unsigned m_indxPerChunk; // How many triangles in each chunk

    // Not implented, for something like running a server
    //
    //bool m_noGPU = false;

    bool m_initialized = false;

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
    buindex m_start;
    buindex m_end;
};

}
