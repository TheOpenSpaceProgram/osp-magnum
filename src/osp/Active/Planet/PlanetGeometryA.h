#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "IcoSphereTree.h"

namespace osp
{

// The 20 faces of the icosahedron (Top, Left, Right)
// Each number pointing to a vertex
static constexpr uint8_t sc_icoTemplateTris[20 * 3] {
//  TT  LL  RR   TT  LL  RR   TT  LL  RR   TT  LL  RR   TT  LL  RR
     0,  2,  1,   0,  3,  2,   0,  4,  3,   0,  5,  4,   0,  1,  5,
     8,  1,  2,   2,  7,  8,   7,  2,  3,   3,  6,  7,   6,  3,  4,
     4,  10, 6,  10,  4,  5,   5,  9, 10,   9,  5,  1,   1,  8,  9,
    11,  7,  6,  11,  8,  7,  11,  9,  8,  11, 10,  9,  11,  6, 10
};

// The 20 faces of the icosahedron (Bottom, Right, Left)
static constexpr uint8_t sc_icoTemplateneighbours[20 * 3] {
//  BB  RR  LL   BB  RR  LL   BB  RR  LL   BB  RR  LL   BB  RR  LL
     5,  4,  1,   7,  0,  2,   9,  1,  3,  11,  2,  4,  13,  3,  0,
     0,  6, 14,  16,  5,  7,   1,  8,  6,  15,  7,  9,   2, 10,  8,
    19,  9, 11,   3, 12, 10,  18, 11, 13,   4, 14, 12,  17, 13,  5,
     8, 19, 16,   6, 15, 17,  14, 16, 18,  12, 17, 19,  10, 18, 15
};

// If this changes, then the universe is broken
static constexpr int gc_icosahedronFaceCount = 20;

static constexpr std::uint8_t gc_triangleMaskSubdivided = 0b0001;
static constexpr std::uint8_t gc_triangleMaskChunked    = 0b0010;


// Similar to urho-osp PlanetWrenderer.cpp
class PlanetGeometryA
{

    std::shared_ptr<IcoSphereTree> m_icoTree;
    //std::shared_ptr<Urho3D::IndexBuffer> m_indBufChunk;
    //std::shared_ptr<Urho3D::VertexBuffer> m_chunkVertBuf;

    chindex m_chunkCount; // How many chunks there are right now

    std::vector<trindex> m_chunkIndDomain; // Maps chunks to triangles
    // Spots in the index buffer that want to die
    //Urho3D::PODVector<chindex> m_chunkIndDeleteMe;
    // List of deleted chunk data to overwrite
    std::vector<buindex> m_chunkVertFree;
    // same as above but for individual shared verticies
    std::vector<buindex> m_chunkVertFreeShared;

    // it's impossible for a vertex to have more than 6 users
    // Delete a shared vertex when it's users goes to zero
    // And use user count to calculate normals

    // Count how many times each shared chunk vertex is being used
    std::vector<uint8_t> m_chunkVertUsers;

    // Maps shared vertex indices to the index buffer, so that shared vertices
    // can be obtained from a chunk
    std::vector<buindex> m_chunkSharedIndices;

    buindex m_chunkVertCountShared; // Current number of shared vertices

    float m_cameraDist;
    float m_threshold;

    // Approx. screen area a triangle can take before it should be subdivided
    float m_subdivAreaThreshold = 0.02f;

    // Preferred total size of chunk vertex buffer (m_chunkVertBuf)
    buindex m_chunkMaxVert;
    // How much is reserved for shared vertices
    buindex m_chunkMaxVertShared;
    chindex m_maxChunks; // Max number of chunks

    // How much screen area a triangle can take before it should be chunked
    float m_chunkAreaThreshold = 0.04f;
    unsigned m_chunkResolution = 31; // How many vertices wide each chunk is
    unsigned m_chunkVertsPerSide; // = m_chunkResolution - 1
    unsigned m_chunkSharedCount; // How many shared verticies per chunk
    unsigned m_chunkSize; // How many vertices there are in each chunk
    unsigned m_chunkSizeInd; // How many triangles in each chunk

    // Not implented, for something like running a server
    //
    //bool m_noGPU = false;

    bool m_ready = false;

    // Vertex buffer data is divided unevenly for chunks
    // In m_chunkVertBuf:
    // [shared vertex data, shared vertices]
    //                     ^               ^
    //        (m_chunkMaxVertShared)    (m_chunkMaxVert)

    // if chunk resolution is 16, then...
    // Chunks are triangles of 136 vertices (m_chunkSize)
    // There are 45 vertices on the edges, (sides + corners)
    // = (14 + 14 + 14 + 3) = m_chunkSharedCount;
    // Which means there is 91 vertices left in the middle
    // (m_chunkSize - m_chunkSharedCount)

public:
    PlanetGeometryA() = default;
    ~PlanetGeometryA() = default;

    constexpr bool is_ready() const;

    /**
     * Calculate initial icosahedron and initialize buffers.
     * Call before drawing
     * @param size [in] Minimum height of planet, or radius
     */
    void initialize(double size);

    /**
     * Print out information on vertice count, chunk count, etc...
     */
    void log_stats() const;

protected:

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
     * @return true when a shared vertex can be taken from tri
     */
    bool get_shared_from_tri(buindex* sharedIndex, const SubTriangle& tri,
                             unsigned side, float pos) const;
};


}
