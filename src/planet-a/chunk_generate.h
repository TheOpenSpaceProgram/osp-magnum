/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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
/**
 * @file
 * @brief Functions and data required for generating chunk meshes
 */

#include "skeleton.h"
#include "geometry.h"

namespace planeta
{

struct ChunkScratchpad
{
    void resize(ChunkSkeleton const& rChSk);

    /// Lookup table to help calculate 'Fill' vertices for chunks
    ChunkFillSubdivLUT lut;

    /// Temporary vector for storing sections of shared vertices
    std::vector< osp::MaybeNewId<SkVrtxId> > edgeVertices;

    /// New stitches to apply to currently existing chunks
    osp::KeyedVec<ChunkId, ChunkStitch> stitchCmds;

    lgrn::IdSetStl<ChunkId> chunksAdded;   ///< Recently added chunks
    lgrn::IdSetStl<ChunkId> chunksRemoved; ///< Recently removed chunks

    lgrn::IdSetStl<SharedVrtxId> sharedAdded;   ///< Recently added shared vertices
    lgrn::IdSetStl<SharedVrtxId> sharedRemoved; ///< Recently removed shared vertices

    /// Shared vertices that need to recalculate normals
    lgrn::IdSetStl<SharedVrtxId> sharedNormalsDirty;
};

/**
 * @brief Check a chunk and its neighbors if their stitches (fan triangles) need to be updated.
 *
 * Populates ChunkScratchpad::stitchCmds
 */
void restitch_check(
        ChunkId                   const chunkId,
        SkTriId                   const sktriId,
        ChunkSkeleton             const &rSkCh,
        SubdivTriangleSkeleton    const &rSkel,
        SkeletonVertexData        const &rSkData,
        ChunkScratchpad                 &rChSP);

/**
 * @brief Write chunk fan and fill triangles to the index buffer.
 *
 * Fan triangles will be generated for newly added chunks. Fan triangles will be added or replaced
 * if a chunk command is enabled.
 */
void update_faces(
        ChunkId                         chunkId,
        SkTriId                         sktriId,
        bool                            newlyAdded,
        SubdivTriangleSkeleton          &rSkel,
        SkeletonVertexData        const &rSkData,
        BasicChunkMeshGeometry          &rGeom,
        ChunkMeshBufferInfo       const &rChInfo,
        ChunkScratchpad                 &rChSP,
        ChunkSkeleton                   &rSkCh);

/**
 * @brief Subtract normals from connected shared vertices when removing a chunk, or fan triangles
 *        only if fans are being redone.
 *
 * \c see BasicChunkMeshGeometry::sharedNormalSum
 */
void subtract_normal_contrib(
        ChunkId                         chunkId,
        bool                            onlySubtractFans,
        BasicChunkMeshGeometry          &rGeom,
        ChunkMeshBufferInfo       const &rChInfo,
        ChunkScratchpad                 &rChSP,
        ChunkSkeleton             const &rSkCh);

/**
 * @brief Does asserts, checks if chunk normals are normalized
 */
void debug_check_invariants(
        BasicChunkMeshGeometry    const &rGeom,
        ChunkMeshBufferInfo       const &rChInfo,
        ChunkSkeleton             const &rSkCh);

/**
 * @brief Write chunk mesh in wavefront .obj format
 */
void write_obj(
        std::ostream                    &rStream,
        BasicChunkMeshGeometry    const &rGeom,
        ChunkMeshBufferInfo       const &rChInfo,
        ChunkSkeleton             const &rSkCh);

} // namespace planeta
