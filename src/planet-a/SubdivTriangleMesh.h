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

namespace planeta
{

constexpr inline Magnum::MeshIndexType const gc_meshIndexType = Magnum::MeshIndexType::UnsignedInt;
using MeshIndx_t = Magnum::UnsignedInt;

struct Chunk
{
    SkTriId m_tri;
    MeshIndx_t m_start;
    MeshIndx_t m_end;
};

struct SubdivTriangleMesh
{
    using Vector3Indx_t = Magnum::Math::Vector3<MeshIndx_t>;

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
    // [chunk vertices] [chunk vertices] [chunk vertices] .... [shared vertices]
    std::vector<osp::Vector3> m_meshVrtxBuffer;

    // Index buffer consists of large fixed-size blocks forming a mesh patched
    // overtop of a SkeletonTriangle, known as a Chunk. Each chunk consists of
    // a fixed number of mesh triangles to form most of the triangular tiling
    // of the chunk, but the triangles along the edges are specially treated as
    // a variable (but limited) number of 'Fan triangles' that may form fan-like
    // structures to smoothly transition to other chunks of a higher detail.
    std::vector<Vector3Indx_t> m_meshIndxBuffer;

    // When Chunked vertices, Shared vertices, or Chunks are deleted, then
    // they are simply added to a list of deleted elements in IdRegistry to
    // be reused later, and are not drawn.

    // Empty spaces in the buffers are not a problem. For index buffers, use
    // something like glMultiDrawArrays
};

}
