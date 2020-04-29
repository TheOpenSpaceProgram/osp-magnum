#include <cmath>
#include <iostream>

#include "IcoSphereTree.h"

namespace osp
{

void IcoSphereTree::initialize(float radius)
{

    // Set preferences to some magic numbers
    // TODO: implement a planet config file or something
    m_maxDepth = 5;
    m_minDepth = 0;

    m_maxVertice = 512;
    m_maxTriangles = 256;

    // Create an Icosahedron. Blender style, so there's a vertex directly on
    // top and directly on the bottom. Basicly, a sandwich of two pentagons,
    // rotated 180 apart from each other, and each 1/sqrt(5) above and below
    // the origin.
    //
    // Icosahedron indices viewed from above
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

    using std::sqrt;

    float scl = radius; // scale
    float pnt = scl * (2.0f/5.0f * sqrt(5.0f));
    float hei = scl * (1.0f / sqrt(5.0f));
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

    // Reserve some space on the vertex buffer
    m_vertBuf.resize(m_maxVertice * m_vertCompCount);

    m_vertBuf.assign(icosahedronVerts,
                     icosahedronVerts + (gc_icosahedronVertCount * 3));

    for (int i = 0; i < gc_icosahedronVertCount * 3; i += 3)
    {
        std::cout << "(" << m_vertBuf[i] << ", "
                         << m_vertBuf[i + 1] << ", "
                         <<  m_vertBuf[i + 2] << ") "
                  << "mag: " << sqrt(m_vertBuf[i]*m_vertBuf[i]
                                     + m_vertBuf[i+1]*m_vertBuf[i+1]
                                     + m_vertBuf[i+2]*m_vertBuf[i+2])
                  << "\n";
    }

}


void IcoSphereTree::set_neighbours(SubTriangle& tri,
                                   trindex bot,
                                   trindex rte,
                                   trindex lft)
{
    tri.m_neighbours[0] = bot;
    tri.m_neighbours[1] = rte;
    tri.m_neighbours[2] = lft;
}

void IcoSphereTree::set_verts(SubTriangle& tri, trindex top,
                              trindex lft, trindex rte)
{
    tri.m_corners[0] = top;
    tri.m_corners[1] = lft;
    tri.m_corners[2] = rte;
}

int IcoSphereTree::neighbour_side(const SubTriangle& tri,
                                   const trindex lookingFor)
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

}
