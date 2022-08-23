/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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
#include "scene_misc.h"
#include "scene_physics.h"
#include "scenarios.h"
#include "identifiers.h"

#include "CameraController.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>

#include <osp/Active/SysHierarchy.h>
#include <osp/Active/SysRender.h>

#include <osp/Resource/resources.h>

#include <osp/unpack.h>

using namespace osp;
using namespace osp::active;

using osp::phys::EShape;
using osp::input::EButtonControlIndex;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

namespace testapp::scenes
{

void add_floor(ArrayView<entt::any> const topData, Session const& scnCommon, Session const& material, Session const& shapeSpawn, TopDataId const idResources, PkgId const pkg)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(material,   TESTAPP_MATERIAL);
    OSP_SESSION_UNPACK_DATA(shapeSpawn, TESTAPP_SHAPE_SPAWN);

    auto &rResources    = top_get< Resources >      (topData, idResources);
    auto &rActiveIds    = top_get< ActiveReg_t >    (topData, idActiveIds);
    auto &rBasic        = top_get< ACtxBasic >      (topData, idBasic);
    auto &rDrawing      = top_get< ACtxDrawing >    (topData, idDrawing);
    auto &rDrawingRes   = top_get< ACtxDrawingRes > (topData, idDrawingRes);
    auto &rMatEnts      = top_get< EntSet_t >       (topData, idMatEnts);
    auto &rMatDirty     = top_get< EntVector_t >    (topData, idMatDirty);
    auto &rSpawner      = top_get< SpawnerVec_t >   (topData, idSpawner);

    // Convenient function to get a reference-counted mesh owner
    auto const quick_add_mesh = [&rResources, &rDrawing, &rDrawingRes, pkg] (std::string_view const name) -> MeshIdOwner_t
    {
        osp::ResId const res = rResources.find(osp::restypes::gc_mesh, pkg, name);
        assert(res != lgrn::id_null<osp::ResId>());
        MeshId const meshId = SysRender::own_mesh_resource(rDrawing, rDrawingRes, rResources, res);
        return rDrawing.m_meshRefCounts.ref_add(meshId);
    };

    // Create hierarchy root entity
    rBasic.m_hierRoot = rActiveIds.create();
    rBasic.m_hierarchy.emplace(rBasic.m_hierRoot);

    // Create camera entity
    ActiveEnt const camEnt = rActiveIds.create();

    // Create camera transform and draw transform
    ACompTransform &rCamTf = rBasic.m_transform.emplace(camEnt);
    rCamTf.m_transform.translation().z() = 25;

    // Create camera component
    ACompCamera &rCamComp = rBasic.m_camera.emplace(camEnt);
    rCamComp.m_far = 1u << 24;
    rCamComp.m_near = 1.0f;
    rCamComp.m_fov = 45.0_degf;

    // Add camera to hierarchy
    SysHierarchy::add_child(
            rBasic.m_hierarchy, rBasic.m_hierRoot, camEnt);

    // start making floor

    static constexpr Vector3 const sc_floorSize{64.0f, 64.0f, 1.0f};
    static constexpr Vector3 const sc_floorPos{0.0f, 0.0f, -1.005f};

    // Create floor root entity
    ActiveEnt const floorRootEnt = rActiveIds.create();

    // Add transform and draw transform to root
    rBasic.m_transform.emplace(floorRootEnt);

    // Create floor mesh entity
    ActiveEnt const floorMeshEnt = rActiveIds.create();

    // Add mesh to floor mesh entity
    rDrawing.m_mesh.emplace(floorMeshEnt, quick_add_mesh("grid64solid"));
    rDrawing.m_meshDirty.push_back(floorMeshEnt);

    // Add mesh visualizer material to floor mesh entity
    rMatEnts.ints().resize(rActiveIds.vec().capacity());
    rMatEnts.set(std::size_t(floorMeshEnt));
    rMatDirty.push_back(floorMeshEnt);

    // Add transform, draw transform, opaque, and visible entity
    rBasic.m_transform.emplace(
            floorMeshEnt, ACompTransform{Matrix4::scaling(sc_floorSize)});
    rDrawing.m_opaque.emplace(floorMeshEnt);
    rDrawing.m_visible.emplace(floorMeshEnt);

    // Add floor root to hierarchy root
    SysHierarchy::add_child(
            rBasic.m_hierarchy, rBasic.m_hierRoot, floorRootEnt);

    // Parent floor mesh entity to floor root entity
    SysHierarchy::add_child(
            rBasic.m_hierarchy, floorRootEnt, floorMeshEnt);

    // Add collider to floor root entity
    rSpawner.emplace_back(SpawnShape{
        .m_position = sc_floorPos,
        .m_velocity = sc_floorSize,
        .m_size     = sc_floorSize,
        .m_mass     = 0.0f,
        .m_shape    = EShape::Box
    });
}


Session setup_thrower(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& magnum, Session const& renderer, Session const& simpleCamera, Session const& shapeSpawn)
{
    OSP_SESSION_UNPACK_DATA(shapeSpawn,     TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(shapeSpawn,     TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(magnum,         TESTAPP_MAGNUMAPP);
    OSP_SESSION_UNPACK_DATA(renderer,       TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_DATA(simpleCamera,   TESTAPP_CAMERA_CTRL);

    auto &rCamCtrl = top_get< ACtxCameraController > (topData, idCamCtrl);

    Session thrower;
    auto const [idBtnThrow] = thrower.acquire_data<1>(topData);

    top_emplace< EButtonControlIndex > (topData, idBtnThrow, rCamCtrl.m_controls.button_subscribe("debug_throw"));

    thrower.task() = rBuilder.task().assign({tgInputEvt, tgSpawnMod}).data(
            "Throw spheres when pressing space",
            TopDataIds_t{                      idCamCtrl,        idCamera,              idSpawner,                   idBtnThrow},
            wrap_args([] (ACtxCameraController& rCamCtrl, Camera& rCamera, SpawnerVec_t& rSpawner, EButtonControlIndex btnThrow) noexcept
    {
        // Throw a sphere when the throw button is pressed
        if (rCamCtrl.m_controls.button_held(btnThrow))
        {
            Matrix4 const &camTf = rCamera.m_transform;
            float const speed = 120;
            float const dist = 8.0f;
            rSpawner.emplace_back(SpawnShape{
                .m_position = camTf.translation() - camTf.backward() * dist,
                .m_velocity = -camTf.backward() * speed,
                .m_size     = Vector3{1.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Sphere
            });
            return osp::TopTaskStatus::Success;
        }
        return osp::TopTaskStatus::Success;
    }));

    return thrower;
}

Session setup_droppers(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& scnCommon, Session const& shapeSpawn)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(shapeSpawn, TESTAPP_SHAPE_SPAWN);

    Session droppers;
    auto const [idSpawnTimerA, idSpawnTimerB] = droppers.acquire_data<2>(topData);

    top_emplace< float > (topData, idSpawnTimerA, 0.0f);
    top_emplace< float > (topData, idSpawnTimerB, 0.0f);

    droppers.task() = rBuilder.task().assign({tgTimeEvt, tgSpawnMod}).data(
            "Spawn blocks every 2 seconds",
            TopDataIds_t{              idSpawner,       idSpawnTimerA,          idDeltaTimeIn },
            wrap_args([] (SpawnerVec_t& rSpawner, float& rSpawnTimer, float const deltaTimeIn) noexcept
    {
        rSpawnTimer += deltaTimeIn;
        if (rSpawnTimer >= 2.0f)
        {
            rSpawnTimer -= 2.0f;

            rSpawner.emplace_back(SpawnShape{
                .m_position = {10.0f, 0.0f, 30.0f},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = {2.0f, 2.0f, 1.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Box
            });
        }
    }));

    droppers.task() = rBuilder.task().assign({tgTimeEvt, tgSpawnMod}).data(
            "Spawn cylinders every 1 seconds",
            TopDataIds_t{              idSpawner,       idSpawnTimerB,          idDeltaTimeIn },
            wrap_args([] (SpawnerVec_t& rSpawner, float& rSpawnTimer, float const deltaTimeIn) noexcept
    {
        rSpawnTimer += deltaTimeIn;
        if (rSpawnTimer >= 1.0f)
        {
            rSpawnTimer -= 1.0f;

            rSpawner.emplace_back(SpawnShape{
                .m_position = {-10.0f, 0.0, 30.0f},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = {2.0f, 1.0f, 2.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Cylinder
            });
        }
    }));

    return droppers;
}

}
