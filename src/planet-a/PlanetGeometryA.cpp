#include <algorithm>
#include <iostream>
#include <stack>
#include <array>

#include <Magnum/Math/Functions.h>
#include <osp/types.h>


#include "PlanetGeometryA.h"

namespace osp
{

void PlanetGeometryA::initialize(float radius)
{
    m_subdivAreaThreshold = 0.02f;
    m_vrtxSharedMax = 10000;
    m_chunkMax = 100;

    m_chunkAreaThreshold = 0.04f;
    m_chunkWidth = 17; // MUST BE (POWER OF 2) + 1
    m_chunkWidthB = m_chunkWidth - 1; // used often in many calculations

    m_icoTree = std::make_shared<IcoSphereTree>();
    m_icoTree->initialize(radius);

    using Magnum::Math::pow;

    m_chunkCount = 0;
    m_vrtxSharedCount = 0;

    // triangular numbers formula
    m_vrtxPerChunk = m_chunkWidth * (m_chunkWidth + 1) / 2;

    // This is how many rendered triangles is in a chunk
    m_indxPerChunk = pow(m_chunkWidthB, 2u);
    m_vrtxSharedPerChunk = m_chunkWidthB * 3;

    // calculate total max vertices
    m_vrtxMax = m_vrtxSharedMax + m_chunkMax * (m_vrtxPerChunk
            - m_vrtxSharedPerChunk);

    // allocate vectors for dealing with chunks
    m_indxBuffer.resize(m_chunkMax * m_indxPerChunk * 3);
    m_vrtxBuffer.resize(m_vrtxMax * m_vrtxSize);
    m_chunkToTri.resize(m_chunkMax);
    m_vrtxSharedUsers.resize(m_vrtxSharedMax);


    // Calculate m_chunkSharedIndices;  use:

    // Example of verticies in a chunk:
    // 0
    // 1  2
    // 3  4  5
    // 6  7  8  9

    // Chunk index data is stored as an array of (top, left, right):
    // { 0, 1, 2,  1, 3, 4,  4, 2, 1,  2, 4, 5,  3, 6, 7, ... }
    // Each chunk has the same number of triangles,
    // and are equally spaced in the buffer.
    // There are duplicates of the same index

    // Vertices in the edges of a chunk are considered "shared vertices,"
    // shared with the edges of the chunk's neighboors.
    // (in the example above: all of the vertices except for #4 are shared)
    // Shared vertices are added and removed in an unspecified order.
    // Their reserved space in m_chunkVertBuf is [0 - m_chunkSharedCount]

    // Vertices in the middle are only used by one chunk
    // (only 4), at larger sizes, they outnumber the shared ones
    // They are equally spaced in the vertex buffer
    // Their reserved space in m_chunkVertBuf is [m_chunkSharedCount - max]

    // It would be convinient if the indicies of the edges of the chunk
    // (shared verticies) can be accessed as if they were ordered in a
    // clockwise fasion around the edge of the triangle.
    // (see get_index_ringed).

    // so m_chunkSharedIndices is filled with indices to indices
    // indexToSharedVertex = tri->m_chunkIndex + m_chunkSharedIndices[0]

    // An alterative to this would be storing a list of shared vertices per
    // chunk, which takes more memory

    m_indToShared.resize(m_vrtxSharedPerChunk);

    int i = 0;
    for (int y = 0; y < int(m_chunkWidthB); y ++)
    {
        for (int x = 0; x < y * 2 + 1; x ++)
        {
            // Only consider the up-pointing triangle
            // All of the shared vertices can still be covered
            if (!(x % 2))
            {
                for (int j = 0; j < 3; j ++)
                {
                    unsigned localIndex;

                    switch (j)
                    {
                    case 0:
                        localIndex = get_index_ringed(x / 2, y);
                        break;
                    case 1:
                        localIndex = get_index_ringed(x / 2, y + 1);
                        break;
                    case 2:
                        localIndex = get_index_ringed(x / 2 + 1, y + 1);
                        break;
                    }

                    if (localIndex < m_vrtxSharedPerChunk)
                    {
                        m_indToShared[localIndex] = unsigned(i + j);
                    }

                }
            }
            i += 3;
        }
    }

    m_initialized = true;

    // temporary: test chunking
    //chunk_add(0);
    m_icoTree->subdivide_add(0); // adds triangles 20, 21, 22, 23
    //chunk_add(20);
    m_icoTree->subdivide_add(20); // adds triangles 24, 25, 26, 27
    chunk_add(24);
    chunk_add(25);
    chunk_add(26);
    chunk_add(27);

    chunk_add(21);
    chunk_add(22);
    chunk_add(23);

    chunk_add(1);
    chunk_add(2);
    chunk_add(3);
    chunk_add(4);
    chunk_add(5);
    chunk_add(6);
    chunk_add(7);
    chunk_add(8);
    chunk_add(9);
    chunk_add(10);
    chunk_add(11);
    chunk_add(12);
    chunk_add(13);
    chunk_add(14);
    chunk_add(15);
    chunk_add(16);
    chunk_add(17);
    chunk_add(18);
    chunk_add(19);

}


unsigned PlanetGeometryA::get_index_ringed(unsigned x, unsigned y) const
{
    // || (x == y) ||
    if (y == m_chunkWidthB)
    {
        // Bottom edge
        return x;
    }
    else if (x == 0)
    {
        // Left edge
        return m_chunkWidthB * 2 + y;
    }
    else if (x == y)
    {
        // Right edge
        return m_chunkWidthB * 2 - y;
    }
    else
    {
        // Center
        return m_vrtxSharedPerChunk + get_index(x - 1, y - 2);
    }

}

constexpr unsigned PlanetGeometryA::get_index(int x, int y) const
{
    return unsigned(y * (y + 1) / 2 + x);
}

struct VertexToSubdiv
{
    unsigned m_x, m_y;
    unsigned m_vrtxIndex;
};

void PlanetGeometryA::chunk_add(trindex t)
{
    SubTriangle& tri = m_icoTree->get_triangle(t);

    if (m_chunkCount >= m_chunkMax)
    {
        //URHO3D_LOGERRORF("Chunk limit reached");
        return;
    }

    if (tri.m_bitmask & (gc_triangleMaskChunked | gc_triangleMaskSubdivided))
    {
        // return if already chunked, or is subdivided
        return;
    }

    // Think of tri as a right triangle like this
    //
    // dirDown  dirRight--->
    // |
    // |   o
    // |   oo
    // V   ooo
    //     oooo
    //     ooooo    o = a single vertex
    //     oooooo
    //
    //     <----> m_chunkResolution

    //std::vector<float> const& icoVerts = m_icoTree->get_vertex_buffer();

    // top, left, and right vertices of triangle from IcoSphereTree
    Vector3 const& top = *reinterpret_cast<Vector3 const*>(
                                m_icoTree->get_vertex_pos(tri.m_corners[0]));
    Vector3 const& lft = *reinterpret_cast<Vector3 const*>(
                                m_icoTree->get_vertex_pos(tri.m_corners[1]));
    Vector3 const& rte = *reinterpret_cast<Vector3 const*>(
                                m_icoTree->get_vertex_pos(tri.m_corners[2]));

    //Vector3 dirRte = (rte - lft) / m_chunkWidthB;
    //Vector3 dirDwn = (lft - top) / m_chunkWidthB;

//    std::cout << "lft: ("
//              << lft.x() << ", "
//              << lft.y() << ", "
//              << lft.z() << ") "
//              << "mag: " << top.length() << "\n";

//    std::cout << "dirRte: ("
//              << dirRte.x() << ", "
//              << dirRte.y() << ", "
//              << dirRte.z() << ") "
//              << "mag: " << dirRte.length() << "\n";


    // Loop through neighbours and see which ones are already chunked to share
    // vertices with

    // Take the space at the end of the chunk buffer
    tri.m_chunk = m_chunkCount;

    if (m_vrtxFree.size() == 0) {
        //
        tri.m_chunkVrtx = m_vrtxSharedMax
                        + m_chunkCount
                            * (m_vrtxPerChunk - m_vrtxSharedPerChunk);
    }
    else
    {
        // Use empty space available in the chunk vertex buffer
        tri.m_chunkVrtx = m_vrtxFree.back();
        m_vrtxFree.pop_back();
    }

    std::vector<int> indices(m_vrtxPerChunk, -1);


    using TriToSubdiv_t = std::array<VertexToSubdiv, 3>;

    //uint8_t neighbourDepths[3];
    SubTriangle* neighbours[3];
    int neighbourSide[3]; // Side of tri relative to neighbour's

    TriToSubdiv_t initTri{
                VertexToSubdiv{0, 0, 0},
                VertexToSubdiv{0, m_chunkWidthB, 0},
                VertexToSubdiv{m_chunkWidthB, m_chunkWidthB, 0}};

    // make sure shared corners can be tracked properly
    unsigned minSize = std::max({tri.m_corners[0] / m_icoTree->m_vrtxSize + 1,
                                 tri.m_corners[1] / m_icoTree->m_vrtxSize + 1,
                                 tri.m_corners[2] / m_icoTree->m_vrtxSize + 1,
                                 (unsigned) m_vrtxSharedIcoCorners.size()});
    m_vrtxSharedIcoCorners.resize(minSize, m_vrtxSharedMax);

    // Get neighbours and get/create 3 shared vertices for the first triangle
    for (int side = 0; side < 3; side ++)
    {
        neighbours[side] = &(m_icoTree->get_triangle(tri.m_neighbours[side]));
        neighbourSide[side] = m_icoTree->neighbour_side(*neighbours[side], t);
        //neighbourDepths[i] = (triB->m_bitmask & gc_triangleMaskChunked);

        int corner = (side + 2) % 3;

        unsigned vertIndex;

        buindex &sharedCorner = m_vrtxSharedIcoCorners[
                                 tri.m_corners[corner] / m_icoTree->m_vrtxSize];

        if (sharedCorner < m_vrtxSharedMax)
        {
            // a shared triangle can be taken from a corner
            vertIndex = sharedCorner;
            m_vrtxSharedUsers[vertIndex] ++;
            std::cout << "Corner taken!\n";
        }
        else
        {
            if (shared_from_tri(vertIndex, *neighbours[side],
                                            neighbourSide[side], 0))
            {
                // shared triangle taken from edge
                // as of now this isn't possible yet
                std::cout << "THIS SHOULDN'T HAPPEN, take note of this.\n";
            }
            else
            {
                // new shared triangle created
            }

            sharedCorner = vertIndex;
        }

        auto vrtxOffset = m_vrtxBuffer.begin() + vertIndex * m_vrtxSize;

        // side 0 sets corner 1
        // side 1 sets corner 2
        // side 2 sets corner 0



        initTri[corner].m_vrtxIndex = vertIndex;
        indices[((side + 1) % 3) * m_chunkWidthB] = vertIndex;

        float *vrtxDataRead = m_icoTree->get_vertex_pos(tri.m_corners[corner]);

        // Copy Icotree's position and normal data to vertex buffer

        std::copy(vrtxDataRead + m_icoTree->m_vrtxCompOffsetPos,
                  vrtxDataRead + m_icoTree->m_vrtxCompOffsetPos + 3,
                  vrtxOffset + m_vrtxCompOffsetPos);

        std::copy(vrtxDataRead + m_icoTree->m_vrtxCompOffsetNrm,
                  vrtxDataRead + m_icoTree->m_vrtxCompOffsetNrm + 3,
                  vrtxOffset + m_vrtxCompOffsetNrm);



        //std::cout << "corner: " << (corner * m_chunkWidthB) << "\n";
    }

    // subdivide the triangle multiple times

    // top, left, right

    std::stack<TriToSubdiv_t> m_toSubdiv;

    // add the first triangle
    m_toSubdiv.push(initTri);
    //m_toSubdiv.emplace(TriToSubdiv_t{
    //        VertexToSubdiv{0, 0, 0},
    //        VertexToSubdiv{0, m_chunkWidthB, m_chunkWidthB},
    //        VertexToSubdiv{m_chunkWidthB, m_chunkWidthB, 2 * m_chunkWidthB}});

    // keep track of the non-shared center chunk vertices
    unsigned centerIndex = 0;

    while (!m_toSubdiv.empty())
    {
        // top, left, right
        TriToSubdiv_t const triSub = m_toSubdiv.top();
        m_toSubdiv.pop();

        // subdivide and create middle vertices

        // v = vertices of current triangle
        // m = mid, middle vertices to calculate: bottom, left, right
        // t = center of next triangles to subdivide: top, left, right

        //            v0
        //
        //            t0
        //
        //      m1          m2
        //
        //      t1    t3    t2
        //
        // v1         m0         v2

        std::cout << "newtri\n";

        VertexToSubdiv mid[3];

        // loop through midverts
        for (int i = 0; i < 3; i ++)
        {
            VertexToSubdiv const &vrtxSubA = triSub[(2 + 3 - i) % 3];
            VertexToSubdiv const &vrtxSubB = triSub[(1 + 3 - i) % 3];

            mid[i].m_x = (vrtxSubA.m_x + vrtxSubB.m_x) / 2;
            mid[i].m_y = (vrtxSubA.m_y + vrtxSubB.m_y) / 2;

            unsigned localIndex = get_index_ringed(mid[i].m_x, mid[i].m_y);
            //unsigned vertIndex;


            std::cout << localIndex << "\n";


            if (indices[localIndex] != -1)
            {
                // skip if midvert already exists
                mid[i].m_vrtxIndex = indices[localIndex];
                continue;
            }

            if (localIndex < m_vrtxSharedPerChunk)
            {
                // Current vertex is shared (on the edge), try to grab an
                // existing vertex from a neighbouring triangle

                // Both of these should get optimized into a single div op
                unsigned side = localIndex / m_chunkWidthB;
                unsigned sideInd = localIndex % m_chunkWidthB;

                float pos = 1.0f - float(sideInd + 1.0f) / float(m_chunkWidth);

                // Take a vertex from a neighbour, if possible
                shared_from_tri(mid[i].m_vrtxIndex, *neighbours[side],
                                neighbourSide[side],
                                int(pos * float(m_chunkWidth) + 0.5f));
            }
            else
            {
                // Current vertex is in the center and is not shared

                // Use a vertex from the space defined earler
                mid[i].m_vrtxIndex = tri.m_chunkVrtx + centerIndex;

                // Keep track of which middle index is being looped through
                centerIndex ++;
            }

            indices[localIndex] = mid[i].m_vrtxIndex;

            float const* vrtxDataA = m_vrtxBuffer.data()
                                        + vrtxSubA.m_vrtxIndex * m_vrtxSize;
            float const* vrtxDataB = m_vrtxBuffer.data()
                                        + vrtxSubB.m_vrtxIndex * m_vrtxSize;

            Vector3 const& vrtxPosA = *reinterpret_cast<Vector3 const*>(
                                            vrtxDataA + m_vrtxCompOffsetPos);
            Vector3 const& vrtxPosB = *reinterpret_cast<Vector3 const*>(
                                            vrtxDataB + m_vrtxCompOffsetPos);

            Vector3 const& vertNrmA = *reinterpret_cast<Vector3 const*>(
                                            vrtxDataA + m_vrtxCompOffsetPos);
            Vector3 const& vertNrmB = *reinterpret_cast<Vector3 const*>(
                                            vrtxDataB + m_vrtxCompOffsetPos);

            auto vrtxDataMid = m_vrtxBuffer.begin() + mid[i].m_vrtxIndex * m_vrtxSize;

            Vector3 pos = (vrtxPosA + vrtxPosB) * 0.5f;
            Vector3 nrm = pos.normalized();

            pos = nrm * float(m_icoTree->get_radius());

            // Add Position data to vertex buffer
            std::copy(pos.data(), pos.data() + 3,
                      vrtxDataMid + m_vrtxCompOffsetPos);

            // Add Normal data to vertex buffer
            std::copy(nrm.data(), nrm.data() + 3,
                      vrtxDataMid + m_vrtxCompOffsetNrm);
        }

        //Vector2i midvs[3] = {(verts[2] + verts[1]) / 2,
        //                     (verts[1] + verts[0]) / 2,
        //                     (verts[0] + verts[2]) / 2};

        // return if not subdividable further, distance between two verts
        // is 1
        if (Magnum::Math::abs<int>(triSub[1].m_x - triSub[2].m_x) == 2)
        {
            continue;
        }

        // next triangles to subdivide
        m_toSubdiv.emplace(TriToSubdiv_t{triSub[0],    mid[1],     mid[2]});
        m_toSubdiv.emplace(TriToSubdiv_t{   mid[1], triSub[1],     mid[0]});
        m_toSubdiv.emplace(TriToSubdiv_t{   mid[2],    mid[0],  triSub[2]});
        m_toSubdiv.emplace(TriToSubdiv_t{   mid[0],    mid[2],     mid[1]});
    }

    // The data that will be pushed directly into the chunk index buffer
    // * 3 because there are 3 indices in a triangle
    std::vector<unsigned> chunkIndData(m_indxPerChunk * 3);


    int i = 0;
    // indices array is now populated, connect the dots!
    for (int y = 0; y < int(m_chunkWidthB); y ++)
    {
        for (int x = 0; x < y * 2 + 1; x ++)
        {
            // alternate between true and false
            if (x % 2)
            {
                // upside down triangle
                // top, left, right
                chunkIndData[i + 0]
                        = indices[get_index_ringed(x / 2 + 1, y + 1)];
                chunkIndData[i + 1]
                        = indices[get_index_ringed(x / 2 + 1, y)];
                chunkIndData[i + 2]
                        = indices[get_index_ringed(x / 2, y)];
            }
            else
            {
                // up pointing triangle
                // top, left, rightkn
                chunkIndData[i + 0]
                        = indices[get_index_ringed(x / 2, y)];
                chunkIndData[i + 1]
                        = indices[get_index_ringed(x / 2, y + 1)];
                chunkIndData[i + 2]
                        = indices[get_index_ringed(x / 2 + 1, y + 1)];

                //URHO3D_LOGINFOF("Triangle: %u %u %u", chunkIndData[i + 0],
                //chunkIndData[i + 1], chunkIndData[i + 2]);
            }
            //URHO3D_LOGINFOF("I: %i", i / 3);
            i += 3;
        }
    }

    // Keep track of which part of the index buffer refers to which triangle
    m_chunkToTri[m_chunkCount] = t;

    // Put the index data at the end of the buffer
    tri.m_chunkIndx = m_chunkCount * chunkIndData.size();
    std::copy(chunkIndData.begin(), chunkIndData.end(),
              m_indxBuffer.begin() + tri.m_chunkIndx);
    //m_indBufChunk->SetDataRange(chunkIndData.Buffer(), tri->m_chunkIndex,
    //                            m_chunkSizeInd * 3);

    m_chunkCount ++;

    //m_geometryChunk->SetDrawRange(Urho3D::TRIANGLE_LIST, 0,
    //                              m_chunkCount * chunkIndData.Size());

    // The triangle is now chunked
    tri.m_bitmask ^= gc_triangleMaskChunked;


//    buindex muxi;
//    static int fu = 0;
//    shared_from_tri(muxi, tri, 0, fu);
//    fu++;

//    Vector3 & vrtxPosG = *reinterpret_cast<Vector3 *>(
//                   m_vrtxBuffer.data() + muxi * m_vrtxSize + m_vrtxCompOffsetPos);

//    vrtxPosG *= 0.0f;
}


bool PlanetGeometryA::shared_from_tri(buindex& rSharedIndex,
                                      SubTriangle const& tri,
                                      unsigned side, int pos)
{

    if (tri.m_bitmask & gc_triangleMaskChunked)
    {
        // Index buffer data of tri
        //unsigned* triIndData = reinterpret_cast<unsigned*>(
        //                        m_indBufChunk->GetShadowData())
        //                        + tri.m_chunkIndex;

        // m_chunkSharedIndices is a previously calculated array that maps
        // indices local of a triangle, to indices in the index buffer that
        // point to its shared vertices
        //
        //            6
        // [side 2]   7  5     [side 1]
        //            8  9  4
        //            0  1  2  3
        //             [side 0]
        //            <-------->
        // if resolution is 4 (4 vertices per edge), then localIndex is
        // a number from 0 to 8

        buindex localIndex = side * m_chunkWidthB + pos;

        // Loop around when value gets too high, because it's a triangle
        localIndex %= m_vrtxSharedPerChunk;

        rSharedIndex = m_indxBuffer[tri.m_chunkIndx + m_indToShared[localIndex]];

        m_vrtxSharedUsers[rSharedIndex] ++;

        std::cout << "grabbing vertex from side " << side << " pos: " << pos << "\n";

        return true;
    }
    else if (tri.m_bitmask & gc_triangleMaskSubdivided)
    {
        // TODO: Children might be chunked, recurse into child
    }
    //else
    //{
    //    // no communism today
    //}

    // If not, Make a new shared vertex

    // Indices from 0 to m_vrtxSharedMax
    if (m_vrtxSharedCount + 1 >= m_vrtxSharedMax)
    {
        // TODO: shared vertex buffer full, error!
        return false;
    }
    if (m_vrtxSharedFree.size() == 0)
    {
        rSharedIndex = m_vrtxSharedCount;
    }
    else
    {
        rSharedIndex = m_vrtxSharedFree.back();
        m_vrtxSharedFree.pop_back();
    }

    m_vrtxSharedCount ++;
    m_vrtxSharedUsers[rSharedIndex] = 1; // set reference count

    return false;
}

}
