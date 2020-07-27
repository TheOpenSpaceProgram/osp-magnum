#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace osp
{

// Index to a triangle
using trindex = uint32_t;

// Index to a chunk
using chindex = uint32_t;

// Index to a buffer
using buindex = uint32_t;

class IcoSphereTree;
struct SubTriangle;

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

// If these change, then the universe is broken
static constexpr int gc_icosahedronFaceCount = 20;
static constexpr int gc_icosahedronVertCount = 12;

static constexpr std::uint8_t gc_triangleMaskSubdivided = 0b0001;
static constexpr std::uint8_t gc_triangleMaskChunked    = 0b0010;

// An icosahedron with subdividable faces
// it starts with 20 triangles, and each face can be subdivided into 4 more
class IcoSphereTree
{

public:

    // these aren't suppose to be const
    // 6 components per vertex
    // PosX, PosY, PosZ, NormX, NormY, NormZ
    static constexpr int m_vrtxSize = 6;
    static constexpr int m_vrtxCompOffsetPos = 0;
    static constexpr int m_vrtxCompOffsetNrm = 3;

    // 1/2 of arc angle of icosahedron edge against circumscribed circle
    // (1/2)theta = asin(2/sqrt(10 + 2sqrt(5)))
    //static constexpr float sc_icoEdgeAngle = 0.553574358897f;

    IcoSphereTree() = default;
    ~IcoSphereTree() = default;

    void initialize(float radius);

    /**
     * Get triangle from vector of triangles
     * be careful of reallocation!
     * @param t [in] Index to triangle
     * @return Pointer to triangle
     */
    SubTriangle& get_triangle(trindex t)
    {
        return m_triangles[t];
    }

    std::vector<float> const& get_vertex_buffer()
    {
        return m_vrtxBuffer;
    }

    float* get_vertex_pos(buindex vrtOffset)
    {
        return m_vrtxBuffer.data() + vrtOffset + m_vrtxCompOffsetPos;
    }

    float* get_vertex_nrm(buindex nrmOffset)
    {
        return m_vrtxBuffer.data() + nrmOffset + m_vrtxCompOffsetNrm;
    }

    float get_radius()
    {
        return m_radius;
    }

    /**
     * A quick way to set neighbours of a triangle
     * @param tri [ref] Reference to triangle
     * @param bot [in] Bottom
     * @param rte [in] Right
     * @param lft [in] Left
     */
    static void set_neighbours(SubTriangle& tri, trindex bot,
                               trindex rte, trindex lft);

    /**
     * A quick way to set vertices of a triangle
     * @param tri [ref] Reference to triangle
     * @param top [in] Top
     * @param lft Left
     * @param rte Right
     */
    static void set_verts(SubTriangle& tri, buindex top,
                          buindex lft, buindex rte);

    void set_side_recurse(SubTriangle& tri, int side, trindex to);

    /**
     * Find which side a triangle is on another triangle
     * @param [ref] tri Reference to triangle to be searched
     * @param [in] lookingFor Index of triangle to search for
     * @return Neighbour index (0 - 2), or bottom, left, or right
     */
    static int neighbour_side(const SubTriangle& tri,
                              const trindex lookingFor);


    /**
     * Subdivide a triangle into 4 more
     * @param [in] Triangle to subdivide
     */
    void subdivide_add(trindex t);

    /**
     * Unsubdivide a triangle.
     * Removes children and sets neighbours of neighbours
     * @param t [in] Index of triangle to unsubdivide
     */
    void subdivide_remove(trindex t);

    /**
     * Calculates and sets m_center
     * @param tri [ref] Reference to triangle
     */
    void calculate_center(SubTriangle& tri);

private:

    //PODVector<PlanetWrenderer> m_viewers;
    std::vector<float> m_vrtxBuffer;
    std::vector<SubTriangle> m_triangles; // List of all triangles
    // List of indices to deleted triangles in the m_triangles
    std::vector<trindex> m_trianglesFree;
    std::vector<buindex> m_vrtxFree; // Deleted vertices in m_vertBuf
    // use "m_indDomain[buindex]" to get a triangle index

    unsigned m_maxDepth;
    unsigned m_minDepth; // never subdivide below this

    buindex m_maxVertice;
    buindex m_maxTriangles;

    buindex m_vrtxCount;

    float m_radius;
};


// Triangle on the IcoSphereTree
struct SubTriangle
{
    trindex m_parent;
    trindex m_neighbours[3];
    buindex m_corners[3]; // to vertex buffer, 3 corners of triangle

    //bool subdivided;
    uint8_t m_bitmask;
    uint8_t m_depth;

    // Data used when subdivided

    // index to first child, always has 4 children if subdivided
    trindex m_children;
    buindex m_midVrtxs[3]; // Bottom, Right, Left vertices in index buffer
    buindex m_index; // to index buffer

    // Data used when chunked
    chindex m_chunk; // Index to chunk. (First triangle ever chunked will be 0)
    buindex m_chunkIndx; // Index to index data in the index buffer
    buindex m_chunkVrtx; // Index to vertex data
};

}
