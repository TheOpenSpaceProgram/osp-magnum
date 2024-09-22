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

#include "../chunk_generate.h"
#include "../geometry.h"
#include "../skeleton_subdiv.h"
#include "../skeleton.h"

#include <osp/drawing/drawing.h>
#include <osp/core/math_types.h>

namespace planeta
{

/**
 * @brief Scene orientation relative to terrain
 *
 * This is intended to be modified by a system responsible for handling floating origin and
 * the Scene's position within a universe.
 */
struct ACtxTerrainFrame
{
    /// Position of scene's (0, 0, 0) origin point from the terrain's frame of reference.
    osp::Vector3l       position;
    osp::Quaterniond    rotation;
    bool                active      {false};
};

struct ACtxTerrain
{
    // 'Skeleton' used for managing instances and relationships between vertices, triangles, and
    // chunks. These are all integer IDs that can be parented, connected, neighboring, etc...
    planeta::SubdivTriangleSkeleton     skeleton;
    planeta::SkeletonVertexData         skData;
    planeta::ChunkSkeleton              skChunks;

    planeta::ChunkMeshBufferInfo        chunkInfo{};
    planeta::BasicChunkMeshGeometry     chunkGeom;

    planeta::ChunkScratchpad            chunkSP;
    planeta::SkeletonSubdivScratchpad   scratchpad;

    osp::draw::MeshIdOwner_t            terrainMesh;
};

struct ACtxTerrainIco
{
    /// Planet lowest ground level in meters. Lowest valley.
    double  radius{};

    /// Planet max ground height in meters. Highest mountain.
    double  height{};

    std::array<planeta::SkVrtxId,     12>   icoVrtx;
    std::array<planeta::SkTriGroupId, 5>    icoGroups;
    std::array<planeta::SkTriId,      20>   icoTri;
};

} // namespace planeta
