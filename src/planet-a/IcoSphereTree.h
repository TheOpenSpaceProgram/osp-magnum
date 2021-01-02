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

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <vector>

namespace planeta
{

// Index to a triangle
using trindex_t = uint32_t;
constexpr trindex_t gc_invalidTri = std::numeric_limits<trindex_t>::max();

// Index to a buffer
using buindex_t = uint32_t;
constexpr buindex_t gc_invalidBufIndx = std::numeric_limits<buindex_t>::max();

// Index to vertex
using vrindex_t = uint32_t;
constexpr vrindex_t gc_invalidVrtx = std::numeric_limits<vrindex_t>::max();


class IcoSphereTree;
struct SubTriangle;
struct TriangleSideTransform;


// The 20 faces of the icosahedron (Top, Left, Right)
// Each number pointing to a vertex
inline constexpr uint8_t sc_icoTemplateTris[20 * 3] {
//  TT  LL  RR   TT  LL  RR   TT  LL  RR   TT  LL  RR   TT  LL  RR
     0,  2,  1,   0,  3,  2,   0,  4,  3,   0,  5,  4,   0,  1,  5,
     8,  1,  2,   2,  7,  8,   7,  2,  3,   3,  6,  7,   6,  3,  4,
     4,  10, 6,  10,  4,  5,   5,  9, 10,   9,  5,  1,   1,  8,  9,
    11,  7,  6,  11,  8,  7,  11,  9,  8,  11, 10,  9,  11,  6, 10
};

// The 20 faces of the icosahedron (Bottom, Right, Left)
inline constexpr uint8_t sc_icoTemplateneighbours[20 * 3] {
//  BB  RR  LL   BB  RR  LL   BB  RR  LL   BB  RR  LL   BB  RR  LL
     5,  4,  1,   7,  0,  2,   9,  1,  3,  11,  2,  4,  13,  3,  0,
     0,  6, 14,  16,  5,  7,   1,  8,  6,  15,  7,  9,   2, 10,  8,
    19,  9, 11,   3, 12, 10,  18, 11, 13,   4, 14, 12,  17, 13,  5,
     8, 19, 16,   6, 15, 17,  14, 16, 18,  12, 17, 19,  10, 18, 15
};

// If these change, then the universe is broken
inline constexpr int gc_icosahedronFaceCount = 20;
inline constexpr int gc_icosahedronVertCount = 12;

template<typename INT_ENUM_T>
constexpr INT_ENUM_T cycle3(INT_ENUM_T in, int cycle)
{
    return INT_ENUM_T((int(in) + cycle) % 3);
}

using triside_t = int8_t;

enum ETriSibling : std::uint8_t
{
    TriSiblingTop       = 0,
    TriSiblingLeft      = 1,
    TriSiblingRight     = 2,
    TriSiblingCenter    = 3
};

/**
 * A simple 1D translation and scale
 */
struct TriangleSideTransform
{
    float m_translation{0.0f};
    float m_scale{1.0f};
};

class IcoSphereTreeObserver
{
public:
    using Handle_T = std::list<std::weak_ptr<IcoSphereTreeObserver>>::iterator;

    virtual void on_ico_triangles_added(std::vector<trindex_t> const&) = 0;
    virtual void on_ico_triangles_removed(std::vector<trindex_t> const&) = 0;
    virtual void on_ico_vertex_removed(std::vector<vrindex_t> const&) = 0;
};

/**
 * A triangle on the IcoSphereTree
 */
struct SubTriangle
{
    trindex_t m_parent;
    ETriSibling m_siblingIndex;

    std::array<trindex_t, 3> m_neighbours;
    std::array<buindex_t, 3> m_corners; // to vertex buffer, 3 corners of triangle

    osp::Vector3 m_center;

    bool m_subdivided{false};
    bool m_deleted{false};

    // 0 for first 20 triangles of icosahedron. +1 for each depth onwards
    uint8_t m_depth{0};

    // Data used when subdivided

    // index to first child, always has 4 children if subdivided
    // Top Left Right Center
    trindex_t m_children;
    std::array<buindex_t, 3> m_midVrtxs; // Bottom, Right, Left vertices in index buffer
    buindex_t m_index; // to index buffer

    // Index to Neighbour's m_neighbours that points to this triangle
    std::array<triside_t, 3> m_neighbourSide;

    // Number of times this triangle is being used externally
    // Make sure this number is zero before removing this triangle
    // Examples of uses
    // * used by both a separate renderer and a collider generator
    // * used across multiple scenes
    unsigned m_useCount;

    constexpr trindex_t get_child(int i) { return m_children + i; }

    /**
     * Find which side a triangle is on another triangle
     * @param [in] lookingFor Index of triangle to search for
     * @return Neighbour index (0 - 2), or bottom, left, or right
     */
    constexpr int find_neighbour_side(const trindex_t lookingFor)
    {
        // Loop through neighbours on the edges. child 4 (center) is not considered
        // as all it's neighbours are its siblings
        if (this->m_neighbours[0] == lookingFor) { return 0; }
        if (this->m_neighbours[1] == lookingFor) { return 1; }
        if (this->m_neighbours[2] == lookingFor) { return 2; }

        // Neighbour not found
        return -1;
    }
};




/**
 * An Icosahedron with subdividable faces
 * it starts with 20 triangles, and each face can be subdivided into 4 more
 */
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

    /**
     * Get triangle from vector of triangles
     * be careful of reallocation!
     * @param triInd [in] Index to triangle
     * @return Pointer to triangle
     */
    SubTriangle& get_triangle(trindex_t triInd)
    {
        return m_triangles[triInd];
    }

    constexpr std::vector<float> const& get_vertex_buffer()
    {
        return m_vrtxBuffer;
    }

    /**
     * @return Position component of vertex
     */
    float* get_vertex_pos(vrindex_t vrtOffset)
    {
        return m_vrtxBuffer.data() + vrtOffset + m_vrtxCompOffsetPos;
    }

    /**
     * @return Normal component of vertex
     */
    float* get_vertex_nrm(vrindex_t nrmOffset)
    {
        return m_vrtxBuffer.data() + nrmOffset + m_vrtxCompOffsetNrm;
    }

    float get_radius()
    {
        return m_radius;
    }

    /**
     * Calculates and sets m_center of a SubTriangle
     * @param tri [ref] Reference to triangle
     */
    void calculate_center(SubTriangle& rTri);

    /**
     * Set a neighbour of a triangle, and apply for all of it's children's
     * @param tri [ref] Reference to triangle
     * @param side [in] Which side to set
     * @param to [in] Neighbour to operate on
     */
    void set_side_recurse(SubTriangle& rTri, int side, trindex_t to);

    trindex_t ancestor_at_depth(trindex_t start, uint8_t targetDepth);

    /**
     * Subdivide a triangle into 4 more
     * @param [in] Triangle to subdivide
     * @return 0 if subdivision is succesful
     */
    int subdivide_add(trindex_t triInd);

    /**
     * Unsubdivide a triangle.
     * Removes children and sets neighbours of neighbours
     * @param t [in] Index of triangle to unsubdivide
     * @return 0 if removal is succesful
     */
    int subdivide_remove(trindex_t triInd);

    /**
     * Calls subdivide_remove on all triangles that have a 0 m_useCount
     */
    void subdivide_remove_all_unused();

    /**
     * Adds an observer to listen for events
     * @param observer [in] Observer to notify for events
     * @return Iterator to observer once added to internal container, intended
     *         to allow removing the observer later on.
     */
    IcoSphereTreeObserver::Handle_T event_add(
            std::weak_ptr<IcoSphereTreeObserver> observer);

    /**
     * Remove an observer [TODO]
     * @param observer
     */
    void event_remove(IcoSphereTreeObserver::Handle_T observer);

    /**
     * Notify all observers about changes in triangles and vertices.
     */
    void event_notify();

    /**
     * Create a TriangleSideTransform that converts coordinate space from child
     * to ancestor. If 0.0 to 1.0 refers to a position along the edge of a
     * child, then transforming to the parent's coordinate space will result in
     * a number between 0.0 and 0.5, or 0.5 to 1.0.
     *
     * @param t             [in] Triangle with coordinates to transform from
     * @param side          [in] Which side of that triangle to use
     * @param targetDepth   [in] Depth of ancestor to transform to
     * @param pAncestorOut  [out] optional index of ancestor triangle once found
     * @return A TriangleSideTransform that can be used to transform positions
     */
    TriangleSideTransform transform_to_ancestor(
            trindex_t triInd, triside_t side, uint8_t targetDepth,
            trindex_t *pAncestorOut = nullptr);

    /**
     * Checks all triangles for invalid states in order to squash some bugs
     * @return true when error detected
     */
    bool debug_verify_state();

private:

    std::vector<float> m_vrtxBuffer;
    std::vector<SubTriangle> m_triangles; // List of all triangles

    // List of indices to deleted triangles in the m_triangles
    std::vector<trindex_t> m_trianglesFree;
    std::vector<buindex_t> m_vrtxFree; // Deleted vertices in m_vertBuf

    std::vector<trindex_t> m_trianglesAdded;
    std::vector<trindex_t> m_trianglesRemoved;
    std::vector<trindex_t> m_vrtxRemoved;

    std::list<std::weak_ptr<IcoSphereTreeObserver>> m_observers;

    buindex_t m_maxVertice;
    buindex_t m_maxTriangles;

    buindex_t m_vrtxCount;

    float m_radius;
};
}
