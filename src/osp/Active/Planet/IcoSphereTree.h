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


// An icosahedron with subdividable faces
// it starts with 20 triangles, and each face can be subdivided into 4 more
class IcoSphereTree
{

    IcoSphereTree() = default;
    ~IcoSphereTree() = default;

public:

    void initialize();

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
    static void set_verts(SubTriangle& tri, trindex top,
                          trindex lft, trindex rte);

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



    // 6 components per vertex
    // PosX, PosY, PosZ, NormX, NormY, NormZ
    static constexpr int m_vertCompCount = 6;

    //PODVector<PlanetWrenderer> m_viewers;
    std::vector<float> m_vertBuf;
    std::vector<SubTriangle> m_triangles; // List of all triangles
    // List of indices to deleted triangles in the m_triangles
    std::vector<trindex> m_trianglesFree;
    std::vector<buindex> m_vertFree; // Deleted vertices in m_vertBuf
    // use "m_indDomain[buindex]" to get a triangle index

    unsigned m_maxDepth;
    unsigned m_minDepth; // never subdivide below this

    buindex m_maxVertice;
    buindex m_maxTriangles;

    buindex m_vertCount;

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
    buindex m_midVerts[3]; // Bottom, Right, Left vertices in index buffer
    buindex m_index; // to index buffer

    // Data used when chunked
    chindex m_chunk; // Index to chunk. (First triangle ever chunked will be 0)
    buindex m_chunkIndex; // Index to index data in the index buffer
    buindex m_chunkVerts; // Index to vertex data
};

}
