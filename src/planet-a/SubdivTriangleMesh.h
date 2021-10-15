/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "SubdivSkeleton.h"

#include <Magnum/Mesh.h>

#include <memory>
#include <variant>

namespace planeta
{

enum class ChunkId : uint16_t {};
enum class SharedVrtxId : uint16_t {};

struct Chunk
{
    SkTriId m_skeletonTri;
};

struct SharedVertex
{
    SkVrtxId m_skeletonVertex;
};

struct ChunkedTriangleMesh
{
    using Vector3ui_t = Magnum::Math::Vector3<Magnum::UnsignedInt>;
    using Vector3us_t = Magnum::Math::Vector3<Magnum::UnsignedShort>;

    unsigned int m_chunkMax;

    IdRegistry<ChunkId, true> m_chunkIds;

    std::unique_ptr<Chunk[]> m_chunkData;
    std::unique_ptr<SharedVrtxId[]> m_chunkVrtx;

    IdRegistry<SharedVrtxId, true> m_sharedIds;
    std::unique_ptr<SharedVertex[]> m_sharedData;

    // Vertex and index buffer for GPU

    // Vertex buffer is split between large blocks of Chunked vertices, and
    // individual Shared vertices:
    // * Chunked vertices are fixed-size groups of vertices associated with
    //   a SkeletonTriangle. They help form the triangular tiling mesh in the
    //   middle of a chunked SkeletonTriangle, and are not shared with other
    //   SkeletonTriangles.
    // * Shared vertices reside along the edges of SkeletonTriangles, and are
    //   shared between them. They are associated with a VrtxId in m_vrtxIdTree
    //
    // Chunk vertex data:
    // [vertex] [vertex] [vertex] [vertex] [vertex] ...
    // <--------------  m_vrtxChunkSize  -------------->
    //
    // Vertex buffer:
    // [chunk..] [chunk..] [chunk..] ... [shared] [shared] [shared] ...
    // <----  m_vrtxSharedOffset  ---->  <----  m_vrtxSharedMax  ---->
    std::unique_ptr<unsigned char[]> m_vrtxBuffer;

    unsigned int m_vrtxChunkSize;
    unsigned int m_vrtxSharedOffset; // = m_vrtxChunkSize * m_chunkMax
    unsigned int m_vrtxSharedMax;

    // Index buffer consists of large fixed-size blocks forming a mesh patched
    // overtop of a SkeletonTriangle, known as a Chunk. Each chunk consists of
    // a fixed number of mesh triangles to form most of the triangular tiling
    // of the chunk, but the triangles along the edges are specially treated as
    // a variable (but limited) number of 'Fan triangles' that may form fan-like
    // structures to smoothly transition to other chunks of a higher detail.
    //
    // Chunk index data:
    // [tri] [tri] [tri] [tri] ...  [fan tri] [fan tri] [fan tri] ... [empty]
    // <-  m_indxChunkFanOffset  ->
    // <-------------------------  m_indxChunkSize  ------------------------->
    //
    // Index buffer:
    // [chunk..] [chunk..] [chunk..] [chunk..] [chunk..] [chunk..] ...
    // <---------------  m_chunkMax * m_indxChunkSize  --------------->
    std::unique_ptr<Vector3ui_t[]> m_indxBuffer;

    //std::variant< std::unique_ptr<Vector3us_t[]>,
    //              std::unique_ptr<Vector3ui_t[]> > m_indxBuffer;

    unsigned int m_indxChunkSize;
    unsigned int m_indxChunkFanOffset;

    // When Chunked vertices, Shared vertices, or Chunks are deleted, then
    // they are simply added to a list of deleted elements in IdRegistry to
    // be reused later, and are not drawn.

    // Empty spaces in the buffers are not a problem. For index buffers, use
    // something like glMultiDrawArrays

}; // struct SubdivTriangleMesh

ChunkedTriangleMesh make_subdivtrimesh_general(
        unsigned int chunkMax, unsigned int subdivLevels,
        unsigned int vrtxSize);


}
