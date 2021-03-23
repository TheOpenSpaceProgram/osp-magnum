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

#include <algorithm>
#include <cmath>
#include <iostream>

using namespace planeta;

using osp::Vector3;

void IcoSphereTree::initialize(float radius)
{

    // Set preferences to some magic numbers
    // TODO: implement a planet config file or something

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

        icoCount += 3;
    }


    // Initialize first 20 triangles, indices from sc_icoTemplateTris
    for (int i = 0; i < gc_icosahedronFaceCount; i ++)
    {
        // Set triangles
        SubTriangle &rTri = m_triangles.emplace_back();

        // indices were already calculated beforehand
        rTri.m_corners = {
            buindex_t(sc_icoTemplateTris[i * 3 + 0]) * m_vrtxSize
                + m_vrtxCompOffsetPos,
            buindex_t(sc_icoTemplateTris[i * 3 + 1]) * m_vrtxSize
                + m_vrtxCompOffsetPos,
            buindex_t(sc_icoTemplateTris[i * 3 + 2]) * m_vrtxSize
                + m_vrtxCompOffsetPos
        };

        // which triangles neighboor which were calculated beforehand too
        rTri.m_neighbours = {
            sc_icoTemplateneighbours[i * 3 + 0],
            sc_icoTemplateneighbours[i * 3 + 1],
            sc_icoTemplateneighbours[i * 3 + 2]
        };

        calculate_center(rTri);
    }

    // lazily set neighbourSide. This is on a separate loop because all
    // neighbours have to be set first
    for (trindex_t t = 0; t < gc_icosahedronFaceCount; t ++)
    {
        SubTriangle &rTri = get_triangle(t);
        for (triside_t j = 0; j < 3; j ++)
        {
            rTri.m_neighbourSide[j] = get_triangle(rTri.m_neighbours[j])
                                                .find_neighbour_side(t);
        }
    }
}

void IcoSphereTree::calculate_center(SubTriangle &rTri)
{
    // use average of 3 coners as the center

    Vector3 const& vertA = Vector3::from(get_vertex_pos(rTri.m_corners[0]));
    Vector3 const& vertB = Vector3::from(get_vertex_pos(rTri.m_corners[1]));
    Vector3 const& vertC = Vector3::from(get_vertex_pos(rTri.m_corners[2]));

    rTri.m_center = (vertA + vertB + vertC) / 3.0f;
}

void IcoSphereTree::set_side_recurse(SubTriangle& rTri, int side, trindex_t to)
{
    rTri.m_neighbours[side] = to;
    if (rTri.m_subdivided)
    {
        set_side_recurse(m_triangles[rTri.m_children + ((side + 1) % 3)],
                side, to);
        set_side_recurse(m_triangles[rTri.m_children + ((side + 2) % 3)],
                side, to);
    }
}

trindex_t IcoSphereTree::ancestor_at_depth(trindex_t start, uint8_t targetDepth)
{
    trindex_t t = start;
    while (true)
    {
        SubTriangle &tri = get_triangle(t);
        if (tri.m_depth != targetDepth)
        {
            t = tri.m_parent;
        }
        else
        {
            return t;
        }
    }
}

int IcoSphereTree::subdivide_add(trindex_t triInd)
{
    if (m_vrtxCount + 3 >= m_maxVertice)
    {
        // error! max vertex count exceeded
        return 1;
    }

    // Add the 4 new triangles
    // Top Left Right Center
    trindex_t childrenIndex;
    if (m_trianglesFree.empty())
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

    SubTriangle &rTri = get_triangle(triInd);
    rTri.m_children = childrenIndex;

    SubTriangle &childA = get_triangle(rTri.m_children + 0);
    SubTriangle &childB = get_triangle(rTri.m_children + 1);
    SubTriangle &childC = get_triangle(rTri.m_children + 2);
    SubTriangle &childD = get_triangle(rTri.m_children + 3);

    // set parent and sibling index
    childA.m_parent = childB.m_parent = childC.m_parent = childD.m_parent
                    = triInd;
    childA.m_siblingIndex = TriSiblingTop;
    childB.m_siblingIndex = TriSiblingLeft;
    childC.m_siblingIndex = TriSiblingRight;
    childD.m_siblingIndex = TriSiblingCenter;

    // set all as not deleted
    childA.m_deleted = childB.m_deleted = childC.m_deleted = childD.m_deleted
            = false;

    // Later Notify observers about new triangles added
    m_trianglesAdded.push_back(rTri.m_children + 0);
    m_trianglesAdded.push_back(rTri.m_children + 1);
    m_trianglesAdded.push_back(rTri.m_children + 2);
    m_trianglesAdded.push_back(rTri.m_children + 3);

    // Set the neighboors of the top triangle to:
    // bottom neighboor = new middle triangle
    // right neighboor  = right neighboor of parent (tri)
    // left neighboor   = left neighboor of parent (tri)
    childA.m_neighbours
            = {rTri.m_children + 3, rTri.m_neighbours[1], rTri.m_neighbours[2]};
    // same but for every other triangle
    childB.m_neighbours
            = {rTri.m_neighbours[0], rTri.m_children + 3, rTri.m_neighbours[2]};
    childC.m_neighbours
            = {rTri.m_neighbours[0], rTri.m_neighbours[1], rTri.m_children + 3};
    // the middle triangle is completely surrounded by its siblings
    childD.m_neighbours
            = {rTri.m_children + 0, rTri.m_children + 1, rTri.m_children + 2};

    // Set neighbour-sides. This will allow a triangle to know which
    childA.m_neighbourSide
            = {0, rTri.m_neighbourSide[1], rTri.m_neighbourSide[2]};
    childB.m_neighbourSide
            = {rTri.m_neighbourSide[0], 1, rTri.m_neighbourSide[2]};
    childC.m_neighbourSide
            = {rTri.m_neighbourSide[0], rTri.m_neighbourSide[1], 2};
    childD.m_neighbourSide = {0, 1, 2};

    // Inherit m_depth
    childA.m_depth = childB.m_depth = childC.m_depth = childD.m_depth
                   = rTri.m_depth + 1;
    // Set m_bitmasks to 0, for not visible, not subdivided, not chunked
    childA.m_subdivided = childB.m_subdivided = childC.m_subdivided
                        = childD.m_subdivided = false;
    // Subdivide lines and add verticies, or take from other triangles

    // Loop through 3 edges of the triangle: Bottom, Right, Left
    // rTri.sides refers to an index of another triangle on that side
    for (int i = 0; i < 3; i ++)
    {
        SubTriangle &triB = get_triangle(rTri.m_neighbours[i]);

        // Check if neighbour is subdivided. If so, then take it's middle vertex
        // If not, then create a new vertex.
        if (!triB.m_subdivided || (triB.m_depth != rTri.m_depth))
               {
            // A new vertex has to be created in the middle of the line
            if (!m_vrtxFree.empty())
            {
                rTri.m_midVrtxs[i] = m_vrtxFree[m_vrtxFree.size() - 1];
                m_vrtxFree.pop_back();
            }
            else
            {
                rTri.m_midVrtxs[i] = m_vrtxCount * m_vrtxSize;
                m_vrtxCount ++;
            }

            Vector3 const& vertA = Vector3::from(
                                    get_vertex_pos(rTri.m_corners[(i + 1) % 3]));
            Vector3 const& vertB = Vector3::from(
                                    get_vertex_pos(rTri.m_corners[(i + 2) % 3]));

            Vector3 &midPos = Vector3::from(get_vertex_pos(rTri.m_midVrtxs[i]));

            Vector3 &destNrm = Vector3::from(get_vertex_nrm(rTri.m_midVrtxs[i]));

            //Vector3 vertM[2];
            destNrm = ((vertA + vertB) / 2).normalized();
            midPos = destNrm * float(m_radius);
        }
        else
        {


            // Which side tri is on triB
            triside_t sideB = get_triangle(triInd).m_neighbourSide[i];

            // Instead of creating a new vertex, use the one from triB since
            // it's already subdivided
            rTri.m_midVrtxs[i] = triB.m_midVrtxs[sideB];

            // this commented out console.log was created back when this was a
            // JS browser canvas test, survived being rewritten in Urho3D, and
            // still survives in this magnum repo
            //console.log(i + ": Used existing vertex");

            // Set sides
            // Side 0(bottom) corresponds to child triangle 1(left),  2(right)
            // Side 1(right)  corresponds to child triangle 2(right), 0(top)
            // Side 2(left)   corresponds to child triangle 0(top),   1(left)

            // The two triangles on the side of tri
            trindex_t const triX = rTri.m_children + trindex_t((i + 1) % 3);
            trindex_t const triY = rTri.m_children + trindex_t((i + 2) % 3);

            // The two child triangles on the side of triB
            trindex_t const triBX = triB.m_children + trindex_t((sideB + 1) % 3);
            trindex_t const triBY = triB.m_children + trindex_t((sideB + 2) % 3);

            // Assign neighbours of children to each other
            m_triangles[triX].m_neighbours[i] = triBY;
            m_triangles[triY].m_neighbours[i] = triBX;
            set_side_recurse(m_triangles[triBX], sideB, triY);
            set_side_recurse(m_triangles[triBY], sideB, triX);
        }

    }

    // Set corner verticies
    childA.m_corners = {rTri.m_corners[0], rTri.m_midVrtxs[2], rTri.m_midVrtxs[1]};
    childB.m_corners = {rTri.m_midVrtxs[2], rTri.m_corners[1], rTri.m_midVrtxs[0]};
    childC.m_corners = {rTri.m_midVrtxs[1], rTri.m_midVrtxs[0], rTri.m_corners[2]};
    // The center triangle is made up of purely middle vertices.
    childD.m_corners = {rTri.m_midVrtxs[0], rTri.m_midVrtxs[1], rTri.m_midVrtxs[2]};

    // Calculate centers of newly created children
    calculate_center(childA);
    calculate_center(childB);
    calculate_center(childC);
    calculate_center(childD);

    rTri.m_subdivided = true;

    debug_verify_state();

    return 0;
}

int IcoSphereTree::subdivide_remove(trindex_t triInd)
{
    SubTriangle &rTri = get_triangle(triInd);

    if (!rTri.m_subdivided)
    {
        // If not subdivided
        return 1;
    }

    if (rTri.m_useCount != 0)
    {
        // If still has users
        return 2;
    }

    // try unsubdividing children if any are
    for (trindex_t i = 0; i < 4; i ++)
    {
        if (get_triangle(rTri.m_children + i).m_subdivided)
        {
            return 3;
            //int result = subdivide_remove(tri.m_children + i);
            //if (result != 0)
            //{
            //    // can't unsubdivide child
            //    return result;
            //}
        }
    }


    // Loop through 3 sides of triangle
    for (int i = 0; i < 3; i ++)
    {
        // get neighbour on that side
        SubTriangle &rTriB = get_triangle(rTri.m_neighbours[i]);

        // If the neighbour triangle side is not subdivided, then the it is not
        // sharing the middle vertex. It can now be removed.
        if (!rTriB.m_subdivided || rTriB.m_depth != rTri.m_depth)
        {
            // Mark side vertex for replacement
            m_vrtxFree.push_back(rTri.m_midVrtxs[i]);

            m_vrtxRemoved.push_back(rTri.m_midVrtxs[i] / m_vrtxSize);
        }
        // else leave it alone, neighbour is still using the vertex

        // Set neighbours, so that they don't reference deleted triangles
        if (rTriB.m_depth == rTri.m_depth)
        {
            int sideB = rTri.m_neighbourSide[i];
            set_side_recurse(rTriB, sideB, triInd);
        }
    }

    // Now mark triangles for removal. they're always in groups of 4.
    m_trianglesFree.push_back(rTri.m_children);

    // mark all children as deleted
    // try unsubdividing children if any are
    for (trindex_t i = 0; i < 4; i ++)
    {
        get_triangle(rTri.m_children + i).m_deleted = true;

        // Later Notify observers about new triangles removed
        m_trianglesRemoved.push_back(rTri.m_children + i);
    }

    rTri.m_subdivided = false;
    rTri.m_children = gc_invalidTri;

    return 0;
}

void IcoSphereTree::subdivide_remove_all_unused()
{
    for (trindex_t t = 0; t < m_triangles.size(); t ++)
    {
        SubTriangle tri = get_triangle(t);
        if (!tri.m_deleted && !tri.m_subdivided && tri.m_useCount == 0)
        {
            subdivide_remove(tri.m_parent);
        }
    }
}

TriangleSideTransform IcoSphereTree::transform_to_ancestor(
        trindex_t triInd, triside_t side, uint8_t targetDepth, trindex_t *pAncestorOut)
{
    TriangleSideTransform out{0.0f, 1.0f};

    trindex_t curT = triInd;
    SubTriangle *pTri;

    while (true)
    {
        pTri = &m_triangles[curT];

        if (pTri->m_depth == targetDepth)
        {
            if (pAncestorOut != nullptr)
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

            out.m_translation = out.m_translation * 0.5f
                              + 0.5f * (side == triside_t(cycle3(pTri->m_siblingIndex, 1)));
        }

        curT = pTri->m_parent;
    }

}

IcoSphereTreeObserver::Handle_T IcoSphereTree::event_add(
        std::weak_ptr<IcoSphereTreeObserver> observer)
{
    return m_observers.emplace(m_observers.end(), std::move(observer));
}

void IcoSphereTree::event_remove(IcoSphereTreeObserver::Handle_T observer)
{
    // TODO
}

void IcoSphereTree::event_notify()
{
    if (!m_trianglesAdded.empty())
    {
        for (std::weak_ptr<IcoSphereTreeObserver> ob : m_observers)
        {
            // TODO: get rid of expired ones
            ob.lock()->on_ico_triangles_added(m_trianglesAdded);
        }
        m_trianglesAdded.clear();
    }

    if (!m_trianglesRemoved.empty())
    {
        for (std::weak_ptr<IcoSphereTreeObserver> ob : m_observers)
        {
            // TODO: get rid of expired ones
            ob.lock()->on_ico_triangles_removed(m_trianglesRemoved);
        }
        m_trianglesRemoved.clear();
    }

    if (!m_vrtxRemoved.empty())
    {
        for (std::weak_ptr<IcoSphereTreeObserver> ob : m_observers)
        {
            // TODO: get rid of expired ones
            ob.lock()->on_ico_vertex_removed(m_vrtxRemoved);
        }
        m_vrtxRemoved.clear();
    }
}

bool IcoSphereTree::debug_verify_state()
{
    // Iterate triangle array and verify (skip triangles in trianglesFree):
    // 1. Verify Hieratchy (skip if root):
    //    * parent is not deleted
    //    * triangle is parent's child, check m_siblingIndex
    //    * m_depth is parent's m_index + 1
    //    * if subdivided:
    //      * make sure children are valid index and are not deleted
    // 2. Verify neighbours
    //    * Check if triangle can be found in neighbour's neighbours

    //std::cout << "IcoSphereTree Verify:\n";

    bool error = false;

    for (trindex_t t = 0; t < m_triangles.size(); t ++)
    {
        // skip deleted triangles
        if (std::find(m_trianglesFree.begin(), m_trianglesFree.end(), t - t % 4)
            !=  m_trianglesFree.end())
        {
            continue;
        }

        SubTriangle &tri = get_triangle(t);

        // verify hierarchy, for non-root triangles.
        // the first 20 triangles are the root ones
        if (t >= gc_icosahedronFaceCount)
        {
            SubTriangle &parent = get_triangle(tri.m_parent);

            if (std::find(m_trianglesFree.begin(), m_trianglesFree.end(),
                          tri.m_parent - tri.m_parent % 4)
                !=  m_trianglesFree.end())
            {
                std::cout << "* Invalid triangle " << t << ": "
                          << "Parent is deleted\n";
                error = true;
            }

            if (parent.m_children + tri.m_siblingIndex != t)
            {
                std::cout << "* Invalid triangle " << t << ": "
                          << "Parent does not have child\n";
                error = true;
            }

            if (tri.m_subdivided
                && std::find(m_trianglesFree.begin(), m_trianglesFree.end(),
                             tri.m_children)
                !=  m_trianglesFree.end())
            {
                std::cout << "* Invalid triangle " << t << ": "
                          << "Children are deleted\n";
                error = true;
            }
        }

        // verify neighbours. there are always 3 neighbours
        for (int i = 0; i < 3; i ++)
        {
            trindex_t neighbour =  tri.m_neighbours[i];

            SubTriangle *neighbourTri = &get_triangle(neighbour);

            if (tri.m_depth == neighbourTri->m_depth)
            {
                int side = neighbourTri->find_neighbour_side(t);

                if (side == -1)
                {
                    std::cout << "* Invalid triangle " << t << ": "
                              << "Neighbour " << i  << "does not recognize "
                              << "this triangle as a neighbour\n";
                    error = true;
                }
                else if (side != tri.m_neighbourSide[i])
                {
                    std::cout << "* Invalid triangle " << t << ": "
                              << "Incorrect Neighbour's side";
                    error = true;
                }
            }
            else if (tri.m_depth < neighbourTri->m_depth)
            {
                std::cout << "* Invalid triangle " << t << ": "
                          << "Neighbour " << i  << "has larger depth\n";
                error = true;
            }

        }
    }

    return error;
}
