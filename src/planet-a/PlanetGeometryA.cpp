#include <iostream>

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
    m_chunkWidth = 5;
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


    // temporary: test chunking
    chunk_add(0);
    chunk_add(1);
    chunk_add(2);
    chunk_add(3);
    chunk_add(4);

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

    std::vector<float> const& icoVerts = m_icoTree->get_vertex_buffer();

    // top, left, and right vertices of triangle from IcoSphereTree
    Vector3 const& top = *reinterpret_cast<Vector3 const*>(
                                m_icoTree->get_vertex_pos(tri.m_corners[0]));
    Vector3 const& lft = *reinterpret_cast<Vector3 const*>(
                                m_icoTree->get_vertex_pos(tri.m_corners[1]));
    Vector3 const& rte = *reinterpret_cast<Vector3 const*>(
                                m_icoTree->get_vertex_pos(tri.m_corners[2]));


    Vector3 dirRte = (rte - lft) / m_chunkWidthB;
    Vector3 dirDwn = (lft - top) / m_chunkWidthB;

    std::cout << "lft: ("
              << lft.x() << ", "
              << lft.y() << ", "
              << lft.z() << ") "
              << "mag: " << top.length() << "\n";

    std::cout << "dirRte: ("
              << dirRte.x() << ", "
              << dirRte.y() << ", "
              << dirRte.z() << ") "
              << "mag: " << dirRte.length() << "\n";


    // Loop through neighbours and see which ones are already chunked to share
    // vertices with

    //uint8_t neighbourDepths[3];
    SubTriangle* neighbours[3];
    int neighbourSide[3]; // Side of tri relative to neighbour's

    for (int i = 0; i < 3; i ++)
    {
        neighbours[i] = &(m_icoTree->get_triangle(tri.m_neighbours[i]));
        neighbourSide[i] = m_icoTree->neighbour_side(*neighbours[i], t);
        //neighbourDepths[i] = (triB->m_bitmask & gc_triangleMaskChunked);
    }

    // Take the space at the end of the chunk buffer
    tri.m_chunk = m_chunkCount;

    if (m_vrtxFree.size() == 0) {
        //
        tri.m_chunkVrtx = m_vrtxSharedMax + m_chunkCount
                            * (m_vrtxPerChunk - m_vrtxSharedPerChunk);
    }
    else
    {
        // Use empty space available in the chunk vertex buffer
        tri.m_chunkVrtx = m_vrtxFree.back();
        m_vrtxFree.pop_back();
    }

    static std::vector<unsigned> indices;
    indices.resize(m_vrtxPerChunk);

    unsigned middleIndex = 0;
    int i = 0;
    for (int y = 0; y < int(m_chunkWidth); y ++)
    {
        // Loops once, then twice, then thrice, etc...
        // because a triangle starts with 1 vertex at the top
        // 2 next row, 3 next, 4 next, 5 next, etc...
        for (int x = 0; x <= y; x ++)
        {
            unsigned vertIndex;
            unsigned localIndex = get_index_ringed(x, y);

            if (localIndex < m_vrtxSharedPerChunk)
            {

                // Both of these should get optimized into a single div op
                unsigned side = localIndex / m_chunkWidthB;
                unsigned sideInd = localIndex % m_chunkWidthB;
                // side 0: Bottom
                // side 1: Right
                // side 2: Left

                float pos = 1.0f - float(sideInd + 1.0f) / float(m_chunkWidth);

                // Take a vertex from a neighbour, if possible
                if (get_shared_from_tri(&vertIndex, *neighbours[side],
                                        neighbourSide[side], pos))
                {
                    // increment number of users and stuff

                }
                else
                {
                    // If not, Make a new shared vertex

                    // Indices from 0 to m_vrtxSharedMax
                    if (m_vrtxSharedCount + 1 >= m_vrtxSharedMax)
                    {
                        //URHO3D_LOGERROR("Max Shared Vertices for Chunk");
                        return;
                    }
                    if (m_vrtxSharedFree.size() == 0) {
                        vertIndex = m_vrtxSharedCount;
                    }
                    else
                    {
                        vertIndex = m_vrtxSharedFree.back();
                        m_vrtxSharedFree.pop_back();
                    }

                    m_vrtxSharedCount ++;
                    m_vrtxSharedUsers[vertIndex] = 1;
                }
            }
            else
            {
                // Use a vertex from the space defined earler
                vertIndex = tri.m_chunkVrtx + middleIndex;

                // Keep track of which middle index is being looped through
                middleIndex ++;
            }

            Vector3 pos = top + (dirRte * x + dirDwn * y);
            Vector3 nrm = pos.normalized();

            pos = nrm * float(m_icoTree->get_radius());

            // Position and normal
            //Vector3 vertM[2] = {pos, normal};

            auto vrtxOffset = m_vrtxBuffer.begin() + vertIndex * m_vrtxSize;

            // Add Position data to vertex buffer
            std::copy(pos.data(), pos.data() + 3,
                      vrtxOffset + m_vrtxCompOffsetPos);

            // Add Normal data to vertex buffer
            std::copy(nrm.data(), nrm.data() + 3,
                      vrtxOffset + m_vrtxCompOffsetNrm);

            // maybe add something else to the buffer some day


            std::cout << "o " << pos.x() << " " << pos.y() << " " << pos.z() << "\n";

            indices[localIndex] = vertIndex;
            i ++;
        }
    }

    // The data that will be pushed directly into the chunk index buffer
    // * 3 because there are 3 indices in a triangle
    std::vector<unsigned> chunkIndData(m_indxPerChunk * 3);

    i = 0;
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


}


bool PlanetGeometryA::get_shared_from_tri(buindex* sharedIndex,
                                          SubTriangle const& tri,
                                          unsigned side, float pos) const
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

        buindex localIndex = side * m_chunkWidthB
                                + std::round(pos * float(m_chunkWidth));

        // Loop around when value gets too high, because it's a triangle
        localIndex %= m_vrtxSharedPerChunk;

        *sharedIndex = m_indxBuffer[tri.m_chunkIndx + m_indToShared[localIndex]];
        return true;
    }
    else if (tri.m_bitmask & gc_triangleMaskSubdivided)
    {
        // Children might be chunked, recurse into child
        *sharedIndex = 0;
        return true;
    }
    else
    {
        return false;
    }
}

}
