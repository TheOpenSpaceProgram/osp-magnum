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

#include <osp/Universe.h>
#include <osp/Active/SysAreaAssociate.h>
#include <osp/Active/SysForceFields.h>
#include <osp/Active/activetypes.h>

#include <osp/Active/universe_sync.h>

#include <Magnum/Shaders/MeshVisualizer.h>
#include <Magnum/GL/Mesh.h>

namespace planeta::active
{

using Vector3ui = Magnum::Math::Vector3<Magnum::UnsignedInt>;

enum class CustomMeshId : uint32_t {};

class ACtxCustomMeshs
{



private:
    osp::UniqueIdRegistry<CustomMeshId> m_meshIds;
    //IdRefCount<CustomMeshId> m_meshIdRefs;
    std::vector< std::optional<Magnum::Trade::MeshData> > m_meshs;
};

struct ACompGLMesh
{
    Magnum::GL::Buffer m_vrtxBufGL{};
    Magnum::GL::Buffer m_indxBufGL{};
};

struct ACompCustomMesh
{
    CustomMeshId m_id;
};

struct ACtxSyncPlanets
{
    MapSatToEnt_t m_inArea;
};

/**
 * @brief Stores data to store a very large terrain mesh with LOD
 */
struct ACompTriTerrain
{
    //SubdivTriangleSkeleton m_skeleton;

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

    static osp::active::ActiveEnt activate(
            osp::active::ActiveScene &rScene, osp::universe::Universe &rUni,
            osp::universe::Satellite areaSat, osp::universe::Satellite tgtSat);


    static void planet_update_geometry(
            osp::active::ActiveEnt planetEnt,
            osp::active::ActiveScene& rScene);

    static void update_activate(osp::active::ActiveScene& rScene);

    static void update_geometry(osp::active::ActiveScene& rScene);

    static void update_geometry_gl(osp::active::ActiveScene& rScene);

    static void update_physics(osp::active::ActiveScene& rScene);
};

}
