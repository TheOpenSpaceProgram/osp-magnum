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

#include "skeleton.h"
#include "geometry.h"

#include "chunk_utils.h"

#include <osp/core/bitvector.h>
#include <osp/core/math_int64.h>

namespace planeta
{

struct ChunkScratchpad
{
    ChunkVrtxSubdivLUT lut;

    osp::KeyedVec<ChunkId, ChunkStitch> stitchCmds;

    /// Newly added shared vertices, position needs to be copied from skeleton
    osp::BitVector_t    sharedAdded;

    osp::BitVector_t    sharedRemoved;

    std::vector< osp::MaybeNewId<SkVrtxId> > edgeVertices;

    /// Shared vertices that need to recalculate normals
    osp::BitVector_t sharedNormalsDirty;
};

void restitch_check(
        ChunkId                         chunkId,
        SkTriId                         sktriId,
        ChunkSkeleton&                  rSkCh,
        SubdivTriangleSkeleton          &rSkel,
        SkeletonVertexData              &rSkTrn,
        ChunkScratchpad                 &rChSP);

void update_faces(
        ChunkId                         chunkId,
        SkTriId                         sktriId,
        bool                            newlyAdded,
        SubdivTriangleSkeleton          &rSkel,
        SkeletonVertexData        const &rSkData,
        BasicTerrainGeometry            &rGeom,
        ChunkMeshBufferInfo       const &rChInfo,
        ChunkScratchpad                 &rChSP,
        ChunkSkeleton                   &rSkCh);

void subtract_normal_contrib(
        ChunkId                         chunkId,
        bool                            subtractFill,
        bool                            subtractFan,
        BasicTerrainGeometry            &rGeom,
        ChunkMeshBufferInfo       const &rChInfo,
        ChunkScratchpad                 &rChSP,
        ChunkSkeleton             const &rSkCh);

void debug_check_invariants(
        BasicTerrainGeometry      const &rGeom,
        ChunkMeshBufferInfo       const &rChInfo,
        ChunkSkeleton             const &rSkCh);

void write_obj(
        std::ostream                    &rStream,
        BasicTerrainGeometry      const &rGeom,
        ChunkMeshBufferInfo       const &rChInfo,
        ChunkSkeleton             const &rSkCh);

} // namespace planeta
