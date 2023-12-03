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

//https://github.com/Capital-Asterisk/osp-magnum/blob/planets-despaghetti/src/planet-a/Active/SysPlanetA.cpp
//https://github.com/Capital-Asterisk/osp-magnum/blob/cdf94fe7942d12bbc6e90fd586957640dc088d78/src/planet-a/Active/SysPlanetA.cpp

struct ACtxTerrain
{
    osp::Vector3l position; ///< Position relative to planet's center
    osp::Quaterniond rotation;

    std::vector<osp::Vector3l> skPositions;
    std::vector<osp::Vector3> skNormals;
};

struct ACtxTerrainIco
{
    std::array<planeta::SkVrtxId, 12> icoVrtx;
    std::array<planeta::SkTriId, 20> icoTri;
};


struct ACtxTerrainChunks
{

};

struct ACtxPlanetTerrainDraw
{

};

osp::Session setup_terrain(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData);


} // namespace testapp::scenes
