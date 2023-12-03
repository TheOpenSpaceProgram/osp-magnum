/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "terrain.h"


#include <planet-a/icosahedron.h>
#include <osp/core/math_2pow.h>

#include <fstream>

using namespace planeta;
using namespace osp;
using namespace osp::math;

namespace testapp::scenes
{

// max detail chunks don't have fans! useful fact for physics
// just leave some holes in the index buffer for the fans. max 2 to 4 per-chunk?
// RESTRICT FANS TO ONE SIDE ONLY!!!!

struct PlanetVertex
{
    osp::Vector3 pos;
    osp::Vector3 nrm;
};

using Vector2ui = Magnum::Math::Vector2<Magnum::UnsignedInt>;



Session setup_terrain(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData)
{
    std::array<planeta::SkVrtxId, 12>           icoVrtx;
    std::array<planeta::SkTriId, 20>            icoTri;
    osp::KeyedVec<planeta::SkVrtxId, Vector3l>  positions;
    osp::KeyedVec<planeta::SkVrtxId, Vector3>   normals;
    int const   scale   = 10;
    float const radius  = 10;

    SubdivTriangleSkeleton skeleton = create_skeleton_icosahedron(radius, scale, icoVrtx, icoTri, positions, normals);

    constexpr int const c_level = 6;
    constexpr int const c_edgeCount = (1u << c_level) - 1;

    for (planeta::SkTriId tri : icoTri)
    {
        auto &fish = skeleton.tri_at(tri);

        std::array<SkVrtxId, c_edgeCount> chunkEdgeA;
        std::array<SkVrtxId, c_edgeCount> chunkEdgeB;
        std::array<SkVrtxId, c_edgeCount> chunkEdgeC;
        skeleton.vrtx_create_chunk_edge_recurse(c_level, fish.m_vertices[0], fish.m_vertices[1], chunkEdgeA);
        skeleton.vrtx_create_chunk_edge_recurse(c_level, fish.m_vertices[1], fish.m_vertices[2], chunkEdgeB);
        skeleton.vrtx_create_chunk_edge_recurse(c_level, fish.m_vertices[2], fish.m_vertices[0], chunkEdgeC);

        positions.resize(skeleton.vrtx_ids().capacity());
        normals.resize(skeleton.vrtx_ids().capacity());

        ico_calc_chunk_edge_recurse(radius, scale, c_level, fish.m_vertices[0], fish.m_vertices[1], chunkEdgeA, positions, normals);
        ico_calc_chunk_edge_recurse(radius, scale, c_level, fish.m_vertices[1], fish.m_vertices[2], chunkEdgeB, positions, normals);
        ico_calc_chunk_edge_recurse(radius, scale, c_level, fish.m_vertices[2], fish.m_vertices[0], chunkEdgeC, positions, normals);
    }


//





    uint16_t const maxChunks = 69;
    uint16_t const maxShared = 40000;


    ChunkVrtxSubdivLUT chunkVrtxLut(c_level);

    SkeletonChunks skChunks{c_level};


    skChunks.chunk_reserve(maxChunks);
    skChunks.shared_reserve(maxShared);

    ChunkedTriangleMeshInfo info = make_chunked_mesh_info(skChunks, maxChunks, maxShared);


    {
        SkeletonTriangle const &fish = skeleton.tri_at(icoTri[0]);

        std::array<SkVrtxId, c_edgeCount> chunkEdgeA;
        std::array<SkVrtxId, c_edgeCount> chunkEdgeB;
        std::array<SkVrtxId, c_edgeCount> chunkEdgeC;
        skeleton.vrtx_create_chunk_edge_recurse(c_level, fish.m_vertices[0], fish.m_vertices[1], chunkEdgeA);
        skeleton.vrtx_create_chunk_edge_recurse(c_level, fish.m_vertices[1], fish.m_vertices[2], chunkEdgeB);
        skeleton.vrtx_create_chunk_edge_recurse(c_level, fish.m_vertices[2], fish.m_vertices[0], chunkEdgeC);

        ChunkId const chunk = skChunks.chunk_create(skeleton, icoTri[0], chunkEdgeA, chunkEdgeB, chunkEdgeC);
    }


    {
        SkeletonTriangle const &tri = skeleton.tri_at(icoTri[1]);

        std::array<SkVrtxId, 3> vertices = {tri.m_vertices[0], tri.m_vertices[1], tri.m_vertices[2]};
        std::array<SkVrtxId, 3> const middles = skeleton.vrtx_create_middles(vertices);
        SkTriGroupId triChildren = skeleton.tri_subdiv(icoTri[1], middles);

        positions.resize(skeleton.vrtx_ids().capacity());
        normals.resize(skeleton.vrtx_ids().capacity());

        ico_calc_middles(radius, scale, vertices, middles, positions, normals);

        SkTriId const tochunk = tri_id(triChildren, 0);
        SkeletonTriangle const &fish = skeleton.tri_at(tochunk);

        std::array<SkVrtxId, c_edgeCount> chunkEdgeA;
        std::array<SkVrtxId, c_edgeCount> chunkEdgeB;
        std::array<SkVrtxId, c_edgeCount> chunkEdgeC;
        skeleton.vrtx_create_chunk_edge_recurse(c_level,    fish.m_vertices[0], middles[0],         chunkEdgeA);
        skeleton.vrtx_create_chunk_edge_recurse(c_level,    middles[0],         middles[2],         chunkEdgeB);
        skeleton.vrtx_create_chunk_edge_recurse(c_level,    middles[2],         fish.m_vertices[0], chunkEdgeC);

        positions.resize(skeleton.vrtx_ids().capacity());
        normals.resize(skeleton.vrtx_ids().capacity());

        ico_calc_chunk_edge_recurse(radius, scale, c_level, fish.m_vertices[0], middles[0],         chunkEdgeA, positions, normals);
        ico_calc_chunk_edge_recurse(radius, scale, c_level, middles[0],         middles[2],         chunkEdgeB, positions, normals);
        ico_calc_chunk_edge_recurse(radius, scale, c_level, middles[2],         fish.m_vertices[0], chunkEdgeC, positions, normals);

        ChunkId const chunk = skChunks.chunk_create(skeleton, tochunk, chunkEdgeA, chunkEdgeB, chunkEdgeC);

    }


    osp::KeyedVec<VertexIdx, PlanetVertex> vrtxBuf;
    //std::vector<Vector2ui> indxBuf;

    vrtxBuf.resize(info.vbufSize, PlanetVertex{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} });

    auto const to_vrtx = [offset = info.vbufSharedOffset] (SharedVrtxId in)
    {
        return VertexIdx( offset + std::uint32_t(in) );
    };

    // Calculate shared vertex positions
    float const scalepow = std::pow(2.0f, -scale);
    for (SharedVrtxId sharedVrtx : skChunks.m_sharedNewlyAdded)
    {
        SkVrtxId const skelVrtx = skChunks.m_sharedSkVrtx[sharedVrtx];

        //Vector3l const translated = positions[size_t(skelId)] + translaton;
        auto const scaled = Vector3d(positions[skelVrtx]) * scalepow;

        vrtxBuf[to_vrtx(sharedVrtx)].pos = Vector3(scaled);
    }
    skChunks.m_sharedNewlyAdded.clear();

    // Calculate fill vertex positions
    for (std::size_t const chunkIdInt : skChunks.m_chunkIds.bitview().zeros())
    {
        auto const chunk = ChunkId(chunkIdInt);

        std::size_t const fillOffset = info.vbufFillOffset + chunkIdInt*info.fillVrtxCount;

        osp::ArrayView<SharedVrtxOwner_t const> sharedUsed = skChunks.shared_vertices_used(chunk);

        for (ChunkVrtxSubdivLUT::ToSubdiv const& toSubdiv : chunkVrtxLut.data())
        {
            PlanetVertex const &vrtxA = vrtxBuf[chunkVrtxLut.index(sharedUsed, fillOffset, info.vbufSharedOffset, toSubdiv.m_vrtxA)];
            PlanetVertex const &vrtxB = vrtxBuf[chunkVrtxLut.index(sharedUsed, fillOffset, info.vbufSharedOffset, toSubdiv.m_vrtxB)];


            PlanetVertex &rVrtxC = vrtxBuf[info.vbufFillOffset + info.fillVrtxCount*chunkIdInt + std::uint32_t(toSubdiv.m_fillOut)];
            // Heightmap goes here

            osp::Vector3 const avg = (vrtxA.pos + vrtxB.pos) / 2.0f;
            float const avgLen = avg.length();
            float const roundness = radius - avgLen;

            LGRN_ASSERT(rVrtxC.pos.isZero());

            rVrtxC.pos = avg + (avg / avgLen) * roundness;
        }
    }



    std::ofstream objfile;
    objfile.open("planetdebug.obj");

    objfile << "o Planet\n";


//    for (std::size_t const skVrtxInt : skeleton.vrtx_ids().bitview().zeros())
//    {
//        auto pos = Vector3(positions[skVrtxInt]) / int_2pow<int>(scale);

//        objfile << "v " << (pos.x()) << " "
//                        << (pos.y()) << " "
//                        << (pos.z()) << "\n";

//    }

    // how to deal with normals if chunks are separate index and vertex buffers?
    // single vertex buffer?
    // 'stitcher'
    //
    // separate the fill buffers lol

    for (PlanetVertex v : vrtxBuf)
    {
        objfile << "v " << (v.pos.x()) << " "
                        << (v.pos.y()) << " "
                        << (v.pos.z()) << "\n";
    }

    for (PlanetVertex v : vrtxBuf)
    {
        objfile << "vn " << (v.nrm.x()) << " "
                         << (v.nrm.y()) << " "
                         << (v.nrm.z()) << "\n";
    }

    // future optimization: LUT some of these too

    using Vector2us = ChunkVrtxSubdivLUT::Vector2us;

    using Vector3ui = Magnum::Math::Vector3<Magnum::UnsignedInt>;
    //uint32_t indexOffset = a.index_chunk_offset(chunk);

    auto const emit_face = [&objfile] (Magnum::UnsignedInt a, Magnum::UnsignedInt b, Magnum::UnsignedInt c)
    {
        objfile << "f " << (a+1) << "//" << (a+1) << " "
                        << (b+1) << "//" << (b+1) << " "
                        << (c+1) << "//" << (c+1) << "\n";
    };

    for (std::size_t const chunkIdInt : skChunks.m_chunkIds.bitview().zeros())
    {
        auto const chunk = ChunkId(chunkIdInt);
        int trisAdded = 0;
        for (unsigned int y = 0; y < skChunks.m_chunkWidth; y ++)
        for (unsigned int x = 0; x < y * 2 + 1; x ++)
        {
            // alternate between up-pointing and down-pointing triangles
            bool const upPointing = (x % 2 == 0);
            bool const onEdge = (x == 0) || (x == y * 2)
                                || (upPointing && y == skChunks.m_chunkWidth - 1);

            auto const indx
                = upPointing
                ? Vector3ui{ chunk_coord_to_vrtx(skChunks, info, chunk, x / 2,      y       ),
                             chunk_coord_to_vrtx(skChunks, info, chunk, x / 2,      y + 1   ),
                             chunk_coord_to_vrtx(skChunks, info, chunk, x / 2 + 1,  y + 1   ) }
                : Vector3ui{ chunk_coord_to_vrtx(skChunks, info, chunk, x / 2 + 1,  y + 1   ),
                             chunk_coord_to_vrtx(skChunks, info, chunk, x / 2 + 1,  y       ),
                             chunk_coord_to_vrtx(skChunks, info, chunk, x / 2,      y       ) };

            std::array<PlanetVertex*, 3> const vrtxData
            {
                &vrtxBuf[indx.x()],
                &vrtxBuf[indx.y()],
                &vrtxBuf[indx.z()]
            };

            // Calculate face normal
            Vector3 const u = vrtxData[1]->pos - vrtxData[0]->pos;
            Vector3 const v = vrtxData[2]->pos - vrtxData[0]->pos;
            Vector3 const faceNorm = Magnum::Math::cross(u, v).normalized();

            for (int i = 0; i < 3; i ++)
            {
                Vector3 &rVrtxNorm = vrtxData[i]->nrm;

                if (info.is_vertex_shared(indx[i]))
                {
                    if ( ! onEdge)
                    {
                        // Shared vertices can have a variable number of
                        // connected faces

                        SharedVrtxId const shared   = info.vertex_to_shared(indx[i]);
                        uint8_t &rFaceCount         = skChunks.m_sharedFaceCount[shared];

                        // Add new face normal to the average
                        rVrtxNorm = (rVrtxNorm * rFaceCount + faceNorm) / (rFaceCount + 1);
                        rFaceCount ++;

                    }
                    // edge triangles handled by fans, and are left empty
                }
                else
                {
                    // All fill vertices have 6 connected faces
                    // (just look at a picture of a triangular tiling)

                    // Fans with multiple triangles may be connected to a fill
                    // vertex, but the normals are calculated as if there was
                    // only one triangle to (potentially) improve blending

                    rVrtxNorm += faceNorm / 6.0f;
                }
            }

            if ( ! onEdge)
            {
                emit_face(indx[0], indx[1], indx[2]);
                trisAdded ++;
            }
        }

        LGRN_ASSERT(trisAdded == info.fillFaceCount);

        auto stitcher = make_chunk_fan_stitcher(skeleton, skChunks, info, chunk, emit_face);

        int sideDetailX2 = 0;

        using enum ECornerDetailX2;

        switch (sideDetailX2)
        {
        case 0:
            stitcher.corner <0, Left>();
            stitcher.edge   <0, true>();
            stitcher.corner <1, Left>();
            stitcher.edge   <1, true>();
            stitcher.corner <2, Left>();
            stitcher.edge   <2, true>();
            break;
        default:
            stitcher.corner <0, None>();
            stitcher.edge   <0, false>();
            stitcher.corner <1, None>();
            stitcher.edge   <1, false>();
            stitcher.corner <2, None>();
            stitcher.edge   <2, false>();
        }

        // write function to fill gaps

        // 3 separate functions for stich

        /*
         * side fanned: {0, 1, 2, none}
         *
         * fan() single() single();
         * constexpr if@
         *
         *
         *
         *
         * stitch(shared, shared, mid)
         *
         * switch()
         * case a
         * case b
         * case c
         * case d
         *
         *
         */


    }





//    for (Vector3ui i : indxBuf)
//    {
//        Vector3ui iB = i + Vector3ui(1, 1, 1); // obj indices start at 1
//        objfile << "f " << iB.x() << "//" << iB.x() << " "
//                        << iB.y() << "//" << iB.y() << " "
//                        << iB.z() << "//" << iB.z() << "\n";
//    }

    objfile.close();

    /*
    //ChunkedTriangleMeshInfo a = make_subdivtrimesh_general(10, c_level, scale);

    std::vector<PlanetVertex> vrtxBuf(a.vertex_count_max());
    auto const get_vrtx = [&vrtxBuf] (VertexId vrtxId) -> PlanetVertex& { return vrtxBuf[size_t(vrtxId)]; };

    std::vector<Vector3ui> indxBuf(a.index_count_max());

    ChunkVrtxSubdivLUT chunkVrtxLut(c_level);

    ChunkId chunk = a.chunk_create(skeleton, tri_id(triChildren, 3), chunkEdgeA, chunkEdgeB, chunkEdgeC);

    using Corrade::Containers::arrayCast;

    ArrayView_t<PlanetVertex> const vrtxBufShared{
        &vrtxBuf[a.vertex_offset_shared()],
        a.shared_count_max()
    };

    ArrayView_t<PlanetVertex> const vrtxBufChunkFill(
        &vrtxBuf[a.vertex_offset_fill(chunk)],
        a.chunk_vrtx_fill_count()
    );

    // Set positions of shared vertices

    float scalepow = std::pow(2.0f, -scale);


    */

    // notes:
    // 'custom mesh system'
    // just use magnum MeshData
    // https://doc.magnum.graphics/magnum/classMagnum_1_1Trade_1_1MeshData.html#Trade-MeshData-populating-non-owned
    //
    // figure out how to get two GL::Mesh pointing at the same vertex buffer?
    // keep separate vertex and index GL::Buffers
    //
    // don't write a custom mesh system
    // write a thing that just takes the current planet stuff and syncs with GL buffers and shit.
    // 1 gl buffer per chunk for indices
    //

    Session out;

    return out;
}

} // namespace testapp::scenes
