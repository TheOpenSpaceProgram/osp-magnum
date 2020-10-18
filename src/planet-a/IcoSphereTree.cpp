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
#include "IcoSphereTree.h"


#include <cmath>
#include <iostream>

using namespace planeta;

using osp::Vector3;

void IcoSphereTree::initialize(float radius)
{

    // Set preferences to some magic numbers
    // TODO: implement a planet config file or something
    m_maxDepth = 5;
    m_minDepth = 0;

    m_maxVertice = 512;
    m_maxTriangles = 256;

    m_radius = radius;

    // Create an Icosahedron. Blender style, so there's a vertex directly on
    // top and directly on the bottom. Basicly, a sandwich of two pentagons,
    // rotated 180 apart from each other, and each 1/sqrt(5) above and below
    // the origin.
    //
    // Icosahedron indices viewed from above (Z)
    //
    //          5
    //  4
    //
    //        0      1
    //
    //  3
    //          2
    //
    // Useful page from wolfram:
    // https://mathworld.wolfram.com/RegularPentagon.html
    //
    // The 'radius' of the pentagons are NOT 1.0, as they are slightly above or
    // below the origin. They have to be slightly smaller to keep their
    // distance to the 3D origin as 1.0.
    //
    // it works out to be (2/5 * sqrt(5)) ~~ 90% the size of a typical pentagon
    //
    // Equations 5..8 from the wolfram page:
    // c1 = 1/4 * ( sqrt(5) - 1 )
    // c2 = 1/4 * ( sqrt(5) + 1 )
    // s1 = 1/4 * sqrt( 10 + 2*sqrt(5) )
    // s2 = 1/4 * sqrt( 10 - 2*sqrt(5) )
    //
    // Now multiply by (2/5 * sqrt(5)), using auto-simplify
    // let m = (2/5 * sqrt(5))
    // cxA = m * c1 = 1/2 - sqrt(5)/10
    // cxB = m * c2 = 1/2 + sqrt(5)/10
    // syA = m * s1 = 1/10 * sqrt( 10 * (5 + sqrt(5)) )
    // syN = m * s2 = 1/10 * sqrt( 10 * (5 - sqrt(5)) )

    using std::sqrt; // if only sqrt was constexpr

    float scl = 8.0f; // scale
    float pnt = scl * ( 2.0f/5.0f * sqrt(5.0f) );
    float hei = scl * ( 1.0f / sqrt(5.0f) );
    float cxA = scl * ( 1.0f/2.0f - sqrt(5.0f)/10.0f );
    float cxB = scl * ( 1.0f/2.0f + sqrt(5)/10.0f );
    float syA = scl * ( 1.0f/10.0f * sqrt( 10.0f*(5.0f + sqrt(5.0f)) ) );
    float syB = scl * ( 1.0f/10.0f * sqrt( 10.0f*(5.0f - sqrt(5.0f)) ) );

    float icosahedronVerts[] =
    {
        0.0f,   0.0f,    scl, // top point

         pnt,   0.0f,    hei, // 1 top pentagon
         cxA,   -syA,    hei, // 2
        -cxB,   -syB,    hei, // 3
        -cxB,    syB,    hei, // 4
         cxA,    syA,    hei, // 5

        -pnt,   0.0f,   -hei, // 6 bottom pentagon
        -cxA,   -syA,   -hei, // 7
         cxB,   -syB,   -hei, // 8
         cxB,    syB,   -hei, // 9
        -cxA,    syA,   -hei, // 10

        0.0f,   0.0f,   -scl  // 11 bottom point
    };

    m_vrtxCount = gc_icosahedronVertCount;

    // Reserve some space on the vertex buffer
    m_vrtxBuffer.resize(m_maxVertice * m_vrtxSize);

    float radiusScaleFactor = radius / scl;
    int icoCount = 0;

    // Add to vertex buffer, along with normals
    for (int i = 0; i < gc_icosahedronVertCount * m_vrtxSize;
         i += m_vrtxSize)
    {

        // Add vertex data
        m_vrtxBuffer[i + m_vrtxCompOffsetPos + 0]
                = icosahedronVerts[icoCount + 0] * radiusScaleFactor;
        m_vrtxBuffer[i + m_vrtxCompOffsetPos + 1]
                = icosahedronVerts[icoCount + 1] * radiusScaleFactor;
        m_vrtxBuffer[i + m_vrtxCompOffsetPos + 2]
                = icosahedronVerts[icoCount + 2] * radiusScaleFactor;

        // Add normal data (just a normalized vertex)
        m_vrtxBuffer[i + m_vrtxCompOffsetNrm + 0]
                = icosahedronVerts[icoCount + 0] / scl;
        m_vrtxBuffer[i + m_vrtxCompOffsetNrm + 1]
                = icosahedronVerts[icoCount + 1] / scl;
        m_vrtxBuffer[i + m_vrtxCompOffsetNrm + 2]
                = icosahedronVerts[icoCount + 2] / scl;



        std::cout << "(" << m_vrtxBuffer[i] << ", "
                         << m_vrtxBuffer[i + 1] << ", "
                         <<  m_vrtxBuffer[i + 2] << ") "
                  << "mag: " << sqrt(m_vrtxBuffer[i]*m_vrtxBuffer[i]
                                     + m_vrtxBuffer[i+1]*m_vrtxBuffer[i+1]
                                     + m_vrtxBuffer[i+2]*m_vrtxBuffer[i+2])
                  << "\n";

        icoCount += 3;
    }


    // Initialize first 20 triangles, indices from sc_icoTemplateTris
    for (int i = 0; i < gc_icosahedronFaceCount; i ++)
    {
        // Set triangles
        SubTriangle tri;

        // indices were already calculated beforehand
        set_verts(tri,
                  sc_icoTemplateTris[i * 3 + 0] * m_vrtxSize
                                                + m_vrtxCompOffsetPos,
                  sc_icoTemplateTris[i * 3 + 1] * m_vrtxSize
                                                + m_vrtxCompOffsetPos,
                  sc_icoTemplateTris[i * 3 + 2] * m_vrtxSize
                                                + m_vrtxCompOffsetPos);

        // which triangles neighboor which were calculated beforehand too
        set_neighbours(tri,
                       sc_icoTemplateneighbours[i * 3 + 0],
                       sc_icoTemplateneighbours[i * 3 + 1],
                       sc_icoTemplateneighbours[i * 3 + 2]);

        tri.m_bitmask = 0;
        tri.m_depth = 0;
        calculate_center(tri);
        m_triangles.push_back(std::move(tri));
    }

}

void IcoSphereTree::subdivide_add(trindex_t t)
{
    // if bottom triangle is deeper, use that vertex
    // same with left and right

    // Add the 4 new triangles
    // Top Left Right Center
    trindex_t childrenIndex;
    if (m_trianglesFree.size() == 0)
    {
        // Make new triangles
        childrenIndex = m_triangles.size();
        m_triangles.resize(childrenIndex + 4);
    }
    else
    {
        // Free triangles always come in groups of 4
        // as triangles are always deleted in groups of 4
        // pop doesn't return the value for Urho, so save last element then pop
        const trindex_t j = m_trianglesFree[m_trianglesFree.size() - 1];
        m_trianglesFree.pop_back();
        childrenIndex = j;
    }

    SubTriangle &tri = get_triangle(t);
    tri.m_children = childrenIndex;

    SubTriangle &childA = get_triangle(tri.m_children + 0),
                &childB = get_triangle(tri.m_children + 1),
                &childC = get_triangle(tri.m_children + 2),
                &childD = get_triangle(tri.m_children + 3);

    // set parent and sibling index
    childA.m_parent = childB.m_parent = childC.m_parent = childD.m_parent = t;
    childA.m_siblingIndex = 0;
    childB.m_siblingIndex = 1;
    childC.m_siblingIndex = 2;
    childD.m_siblingIndex = 3;

    // Set the neighboors of the top triangle to:
    // bottom neighboor = new middle triangle
    // right neighboor  = right neighboor of parent (tri)
    // left neighboor   = left neighboor of parent (tri)
    set_neighbours(childA, tri.m_children + 3,
                                tri.m_neighbours[1], tri.m_neighbours[2]);
    // same but for every other triangle
    set_neighbours(childB, tri.m_neighbours[0],
                                tri.m_children + 3, tri.m_neighbours[2]);
    set_neighbours(childC, tri.m_neighbours[0],
                                tri.m_neighbours[1], tri.m_children + 3);
    // the middle triangle is completely surrounded by its siblings
    set_neighbours(childD, tri.m_children + 0,
                                tri.m_children + 1, tri.m_children + 2);

    // Inherit m_depth
    childA.m_depth = childB.m_depth = childC.m_depth = childD.m_depth
                   = tri.m_depth + 1;
    // Set m_bitmasks to 0, for not visible, not subdivided, not chunked
    childA.m_bitmask = childB.m_bitmask = childC.m_bitmask = childD.m_bitmask
                     = 0;
    // Subdivide lines and add verticies, or take from other triangles

    // Loop through 3 sides of the triangle: Bottom, Right, Left
    // tri.sides refers to an index of another triangle on that side
    for (int i = 0; i < 3; i ++)
    {
        SubTriangle &triB = get_triangle(tri.m_neighbours[i]);
        // Check if the line is already subdivided,
        // or if there is no triangle on the other side
        if (!(triB.m_bitmask & gc_triangleMaskSubdivided)
                || (triB.m_depth != tri.m_depth))
        {
            // A new vertex has to be created in the middle of the line
            if (m_vrtxFree.size())
            {
                tri.m_midVrtxs[i] = m_vrtxFree[m_vrtxFree.size() - 1];
                m_vrtxFree.pop_back();
            } else {
                tri.m_midVrtxs[i] = m_vrtxCount * m_vrtxSize;
                m_vrtxCount ++;
            }

            Vector3 const& vertA = *reinterpret_cast<Vector3 const*>(
                                    get_vertex_pos(tri.m_corners[(i + 1) % 3]));
            Vector3 const& vertB = *reinterpret_cast<Vector3 const*>(
                                    get_vertex_pos(tri.m_corners[(i + 2) % 3]));

            Vector3 &midPos = *reinterpret_cast<Vector3*>(
                                        get_vertex_pos(tri.m_midVrtxs[i]));

            Vector3 &destNrm = *reinterpret_cast<Vector3*>(
                                        get_vertex_nrm(tri.m_midVrtxs[i]));

            //Vector3 vertM[2];
            destNrm = ((vertA + vertB) / 2).normalized();
            midPos = destNrm * float(m_radius);
        }
        else
        {
            // Which side tri is on triB
            int sideB = neighbour_side(triB, t);
            //printf("Vertex is being shared\n");

            // Instead of creating a new vertex, use the one from triB since
            // it's already subdivided
            tri.m_midVrtxs[i] = triB.m_midVrtxs[sideB];
            //console.log(i + ": Used existing vertex");

            // Set sides
            // Side 0(bottom) corresponds to child 1(left), 2(right)
            // Side 1(right) corresponds to child 2(right), 0(top)
            // Side 2(left) corresponds to child 0(top), 1(left)

            // triX/Y refers to the two triangles on the side of tri
            // triBX/Y refers to the two triangles on the side of triB
            trindex_t triX = tri.m_children + trindex_t((i + 1) % 3);
            trindex_t triY = tri.m_children + trindex_t((i + 2) % 3);
            trindex_t triBX = triB.m_children + trindex_t((sideB + 1) % 3);
            trindex_t triBY = triB.m_children + trindex_t((sideB + 2) % 3);

            // Assign the face of each triangle to the other triangle beside it
            m_triangles[triX].m_neighbours[i] = triBY;
            m_triangles[triY].m_neighbours[i] = triBX;
            //m_triangles[triBX].m_neighbours[sideB] = triY;
            //m_triangles[triBY].m_neighbours[sideB] = triX;
            set_side_recurse(m_triangles[triBX], uint8_t(sideB), triY);
            set_side_recurse(m_triangles[triBY], uint8_t(sideB), triX);

            //printf("Set Tri%u %u to %u", triBX)
        }

    }

    // Set verticies
    set_verts(childA, tri.m_corners[0], tri.m_midVrtxs[2], tri.m_midVrtxs[1]);
    set_verts(childB, tri.m_midVrtxs[2], tri.m_corners[1], tri.m_midVrtxs[0]);
    set_verts(childC, tri.m_midVrtxs[1], tri.m_midVrtxs[0], tri.m_corners[2]);
    // The center triangle is made up of purely middle vertices.
    set_verts(childD, tri.m_midVrtxs[0], tri.m_midVrtxs[1], tri.m_midVrtxs[2]);

    // Calculate centers of newly created children
    calculate_center(childA);
    calculate_center(childB);
    calculate_center(childC);
    calculate_center(childD);

    tri.m_bitmask ^= gc_triangleMaskSubdivided;

    // subdivide if below minimum depth
//    if (tri.m_depth < m_minDepth)
//    {
//        // Triangle vector might reallocate, so accessing tri after
//        // subdivide_add will cause undefiend behaviour
//        //URHO3D_LOGINFOF("depth: %i tri: %i", tri->m_depth, t);
//        trindex_t childs = tri.m_children;
//        subdivide_add(childs + 0);
//        subdivide_add(childs + 1);
//        subdivide_add(childs + 2);
//        subdivide_add(childs + 3);
//    }
}


void IcoSphereTree::set_neighbours(SubTriangle& tri,
                                   trindex_t bot, trindex_t rte, trindex_t lft)
{
    tri.m_neighbours[0] = bot;
    tri.m_neighbours[1] = rte;
    tri.m_neighbours[2] = lft;
}

void IcoSphereTree::set_verts(SubTriangle& tri, buindex_t top,
                              buindex_t lft, buindex_t rte)
{
    tri.m_corners[0] = top;
    tri.m_corners[1] = lft;
    tri.m_corners[2] = rte;
}

/**
 * Set a neighbour of a triangle, and apply for all of it's children's
 * @param tri [ref] Reference to triangle
 * @param side [in] Which side to set
 * @param to [in] Neighbour to operate on
 */
void IcoSphereTree::set_side_recurse(SubTriangle& tri, int side, trindex_t to)
{
    tri.m_neighbours[side] = to;
    if (tri.m_bitmask & gc_triangleMaskSubdivided) {
        set_side_recurse(m_triangles[tri.m_children + ((side + 1) % 3)],
                side, to);
        set_side_recurse(m_triangles[tri.m_children + ((side + 2) % 3)],
                side, to);
    }
}

int IcoSphereTree::neighbour_side(const SubTriangle& tri,
                                  const trindex_t lookingFor)
{
    // Loop through neighbours on the edges. child 4 (center) is not considered
    // as all it's neighbours are its siblings
    if(tri.m_neighbours[0] == lookingFor) return 0;
    if(tri.m_neighbours[1] == lookingFor) return 1;
    if(tri.m_neighbours[2] == lookingFor) return 2;

    // this means there's an error
    //assert(false);
    return 255;
}

void IcoSphereTree::calculate_center(SubTriangle &tri)
{
    // use average of 3 coners as the center

    Vector3 const& vertA = *reinterpret_cast<Vector3 const*>(
                                get_vertex_pos(tri.m_corners[0]));
    Vector3 const& vertB = *reinterpret_cast<Vector3 const*>(
                                get_vertex_pos(tri.m_corners[1]));
    Vector3 const& vertC = *reinterpret_cast<Vector3 const*>(
                                get_vertex_pos(tri.m_corners[2]));

    tri.m_center = (vertA + vertB + vertC) / 3.0f;
}

std::pair<trindex_t, trindex_t> IcoSphereTree::find_neighbouring_ancestors(
                                                    trindex_t a, trindex_t b)
{
    std::pair<trindex_t, trindex_t> out{a, b};

    SubTriangle &triA = get_triangle(a);
    SubTriangle &triB = get_triangle(b);

    trindex_t *outIndex;
    trindex_t curIndex;
    uint8_t targetDepth;

    if (triA.m_depth > triB.m_depth)
    {
        // A needs to get set
        targetDepth = triB.m_depth;
        outIndex = &(out.first);
        curIndex = triA.m_parent;
    }
    else if (triA.m_depth < triB.m_depth)
    {
        // B needs to get set
        targetDepth = triA.m_depth;
        outIndex = &(out.second);
        curIndex = triB.m_parent;
    }
    else
    {
        return out;
    }

    SubTriangle *curTri;
    std::cout << "wtf\n";

    // move up chain of parents to the right depth
    do
    {
        *outIndex = curIndex;
        curTri = &get_triangle(*outIndex);
        curIndex = curTri->m_parent;
    }
    while (curTri->m_depth != targetDepth);

    return out;
}

TriangleSideTransform IcoSphereTree::transform_to_ancestor(
        trindex_t t, uint8_t side, uint8_t targetDepth, trindex_t *pAncestorOut)
{
    TriangleSideTransform out{0.0f, 1.0f};

    trindex_t curT = t;
    SubTriangle *tri;

    while (true)
    {
        tri = &m_triangles[curT];

        if (tri->m_depth == targetDepth)
        {
            if (pAncestorOut)
            {
                *pAncestorOut = curT;
            }

            return out;
        }
        else
        {
            // 0:top, 1:left, 2:right

            // side 0 (bottom) : children 1, 2
            // side 1  (right) : children 2, 0
            // side 2   (left) : children 0, 1

            // if sibling index: 0, side: 2, then translation +0.0
            // if sibling index: 1, side: 2, then translation +0.5
            // if sibling index: 1, side: 0, then translation +0.0
            // if sibling index: 2, side: 0, then translation +0.5
            // if sibling index: 2, side: 1, then translation +0.0
            // if sibling index: 0, side: 1, then translation +0.5

            out.m_scale *= 0.5f;

            out.m_translation
                    = out.m_translation * 0.5f
                    + 0.5f * (side == (tri->m_siblingIndex + 1) % 3);
        }

        curT = tri->m_parent;
    }


}
