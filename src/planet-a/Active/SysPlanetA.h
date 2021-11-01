/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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

#include "../SubdivTriangleMesh.h"

#include <osp/Active/activetypes.h>

namespace planeta::active
{

/**
 * @brief Stores data to store a very large terrain mesh with LOD
 */
struct ACompTriTerrain
{
    SubdivTriangleSkeleton m_skeleton;

    std::vector<osp::Vector3l> m_positions;
    std::vector<osp::Vector3> m_normals;
};

/**
 * @brief Indicates that a ACompTriangleTerrain is a round planet
 */
struct ACompPlanetSurface
{
    double m_radius;
};

/**
 * @brief Forms a CustomMesh out of a ACompTriTerrain intended for rendering
 */
struct ACompTriTerrainMesh
{
    ChunkedTriangleMeshInfo m_chunks;
    ChunkVrtxSubdivLUT m_chunkVrtxLut;

    // Triangles that are actively being distance-checked
    std::vector<SkTriId> m_trisWatching;

    osp::Vector3l m_translation;
    int m_pow2scale;

    float surfaceMaxLength;
    float screenMaxLength;
};


class SysPlanetA
{
public:

    static void planet_update_geometry(
            osp::active::ActiveEnt planetEnt,
            osp::active::ActiveScene& rScene);

    static void update_activate(osp::active::ActiveScene& rScene);

    static void update_geometry(osp::active::ActiveScene& rScene);

    static void update_geometry_gl(osp::active::ActiveScene& rScene);

    static void update_physics(osp::active::ActiveScene& rScene);
};

}
