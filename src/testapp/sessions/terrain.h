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
#pragma once

#include "../scenarios.h"
#include "planet-a/SubdivTriangleMesh.h"

#include <osp/core/math_types.h>
#include <osp/drawing/draw_ent.h>

#include <planet-a/SubdivSkeleton.h>

namespace testapp::scenes
{

struct ACtxSurfaceFrame
{
    osp::Vector3l       position; ///< Position relative to planet's center
    osp::Quaterniond    rotation;
    bool                active      {false};
};

struct SkeletonAABB
{
    osp::Vector3l min;
    osp::Vector3l max;
};

struct PerSubdivLevel
{
    /// Subdivided triangles that neighbor a non-subdivided one
    osp::BitVector_t hasNonSubdivedNeighbor;

    /// Non-subdivided triangles that neighbor a subdivided one
    osp::BitVector_t hasSubdivedNeighbor;

    std::vector<planeta::SkTriId>       distanceTestProcessing;
    std::vector<planeta::SkTriId>       distanceTestNext;

    //std::vector<planeta::SkTriId> mustSubdiv;
};

struct ACtxTerrain
{
    planeta::SubdivTriangleSkeleton skeleton;

    osp::KeyedVec<planeta::SkVrtxId, osp::Vector3l> skPositions;
    osp::KeyedVec<planeta::SkVrtxId, osp::Vector3>  skNormals;

    osp::KeyedVec<planeta::SkTriId, osp::Vector3l> sktriCenter;

    std::array<PerSubdivLevel, 10> levels;
    int levelNeedProcess = 10;
    //int levelMax{0};

    int scale{};
};

struct ACtxTerrainIco
{
    float   radius{};
    float   height{};

    std::array<planeta::SkVrtxId, 12>    icoVrtx;
    std::array<planeta::SkTriGroupId, 5> icoGroups;
    std::array<planeta::SkTriId, 20>     icoTri;
};

struct ACtxPlanetTerrainDraw
{
    planeta::ChunkVrtxSubdivLUT         chunkVrtxLut;
    planeta::SkeletonChunks             skChunks;
    planeta::ChunkedTriangleMeshInfo    info;
};

osp::Session setup_terrain(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene);

osp::Session setup_terrain_debug_draw(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         sceneRenderer,
        osp::Session const&         cameraCtrl,
        osp::Session const&         commonScene,
        osp::Session const&         terrain,
        osp::draw::MaterialId       mat);


} // namespace testapp::scenes
