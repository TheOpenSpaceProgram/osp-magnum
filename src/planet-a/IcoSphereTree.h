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

#include <osp/types.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace planeta
{

// Index to a triangle
using trindex_t = uint32_t;

// Index to a buffer
using buindex_t = uint32_t;

class IcoSphereTree;
struct SubTriangle;
struct TriangleSideTransform;

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
//static constexpr std::uint8_t gc_triangleMaskChunked    = 0b0010;



struct TriangleSideTransform
{
    float m_translation;
    float m_scale;
};


// Triangle on the IcoSphereTree
struct SubTriangle
{
    trindex_t m_parent;
    uint8_t m_siblingIndex; // 0:Top 1:Left 2:Right 3:Center

    trindex_t m_neighbours[3];
    buindex_t m_corners[3]; // to vertex buffer, 3 corners of triangle

    osp::Vector3 m_center;

    //bool subdivided;
    uint8_t m_bitmask;
    uint8_t m_depth;

    // Data used when subdivided

    // index to first child, always has 4 children if subdivided
    // Top Left Right Center
    trindex_t m_children;
    buindex_t m_midVrtxs[3]; // Bottom, Right, Left vertices in index buffer
    buindex_t m_index; // to index buffer

    // Data used when chunked
    //unsigned m_chunk; // Index to chunk. (First triangle ever chunked will be 0)
    //buindex_t m_chunkIndx; // Index to index data in the index buffer
    //buindex_t m_chunkVrtx; // Index to vertex data
};



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

    trindex_t triangle_count() { return m_triangles.size(); }

    constexpr bool tri_is_subdivided(SubTriangle &tri) const
    {
        return tri.m_bitmask & gc_triangleMaskSubdivided;
    };

    /**
     * Get triangle from vector of triangles
     * be careful of reallocation!
     * @param t [in] Index to triangle
     * @return Pointer to triangle
     */
    SubTriangle& get_triangle(trindex_t t)
    {
        return m_triangles[t];
    }

    constexpr std::vector<float> const& get_vertex_buffer()
    {
        return m_vrtxBuffer;
    }

    float* get_vertex_pos(buindex_t vrtOffset)
    {
        return m_vrtxBuffer.data() + vrtOffset + m_vrtxCompOffsetPos;
    }

    float* get_vertex_nrm(buindex_t nrmOffset)
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
    static void set_neighbours(SubTriangle& tri, trindex_t bot,
                               trindex_t rte, trindex_t lft);

    /**
     * A quick way to set vertices of a triangle
     * @param tri [ref] Reference to triangle
     * @param top [in] Top
     * @param lft Left
     * @param rte Right
     */
    static void set_verts(SubTriangle& tri, buindex_t top,
                          buindex_t lft, buindex_t rte);

    void set_side_recurse(SubTriangle& tri, int side, trindex_t to);

    /**
     * Find which side a triangle is on another triangle
     * @param [ref] tri Reference to triangle to be searched
     * @param [in] lookingFor Index of triangle to search for
     * @return Neighbour index (0 - 2), or bottom, left, or right
     */
    static int neighbour_side(const SubTriangle& tri,
                              const trindex_t lookingFor);

    /**
     * Subdivide a triangle into 4 more
     * @param [in] Triangle to subdivide
     */
    void subdivide_add(trindex_t t);

    /**
     * Unsubdivide a triangle.
     * Removes children and sets neighbours of neighbours
     * @param t [in] Index of triangle to unsubdivide
     */
    void subdivide_remove(trindex_t t);

    /**
     * Calculates and sets m_center
     * @param tri [ref] Reference to triangle
     */
    void calculate_center(SubTriangle& tri);

    std::pair<trindex_t, trindex_t> find_neighbouring_ancestors(trindex_t a,
                                                                trindex_t b);

    TriangleSideTransform transform_to_ancestor(trindex_t t, uint8_t side,
                                                uint8_t targetDepth, trindex_t *pAncestorOut = nullptr);

    /**
     * Checks all triangles for invalid states in order to squash some bugs
     * @return true when error detected
     */
    bool debug_verify_state();

private:

    //PODVector<PlanetWrenderer> m_viewers;
    std::vector<float> m_vrtxBuffer;
    std::vector<SubTriangle> m_triangles; // List of all triangles
    // List of indices to deleted triangles in the m_triangles
    std::vector<trindex_t> m_trianglesFree;
    std::vector<buindex_t> m_vrtxFree; // Deleted vertices in m_vertBuf
    // use "m_indDomain[buindex_t]" to get a triangle index

    uint8_t m_maxDepth;
    uint8_t m_minDepth; // never subdivide below this

    buindex_t m_maxVertice;
    buindex_t m_maxTriangles;

    buindex_t m_vrtxCount;

    float m_radius;
};
}
