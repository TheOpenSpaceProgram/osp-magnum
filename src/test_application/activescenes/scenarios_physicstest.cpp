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
#include "scenarios.h"
#include "scene_physics.h"
#include "CameraController.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>

#include <osp/Active/SysHierarchy.h>

#include <osp/tasks/builder.h>
#include <osp/tasks/top_utils.h>

#include <osp/Resource/resources.h>

#include <osp/unpack.h>
#include <osp/logging.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

// TODO: move to renderer

#include <Magnum/GL/DefaultFramebuffer.h>
#include <osp/Active/SysRender.h>
#include <osp/Active/opengl/SysRenderGL.h>
#include <osp/Shaders/MeshVisualizer.h>
#include <osp/Shaders/Flat.h>
#include <osp/Shaders/Phong.h>


using namespace osp::active;
using ospnewton::ACtxNwtWorld;
using osp::input::UserInputHandler;
using osp::phys::EShape;
using osp::Resources;
using osp::TopDataIds_t;
using osp::TopTaskStatus;
using osp::Vector3;
using osp::Matrix3;
using osp::Matrix4;

using osp::top_emplace;
using osp::top_get;
using osp::wrap_args;

using Corrade::Containers::arrayView;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

namespace testapp::scenes
{

constexpr int gc_threadCount = 4; // note: not yet passed to Newton

struct SpawnShape
{
    Vector3 m_position;
    Vector3 m_velocity;
    Vector3 m_size;
    float m_mass;
    EShape m_shape;
};

using SpawnerVec_t = std::vector<SpawnShape>;

/**
 * @brief Data used specifically by the physics test scene
 */
struct PhysicsTestData
{
    active_sparse_set_t             m_hasGravity;
    active_sparse_set_t             m_removeOutOfBounds;

    // Timers for when to create boxes and cylinders
    float m_boxTimer{0.0f};
    float m_cylinderTimer{0.0f};

};

void PhysicsTest::setup_scene(MainView mainView, osp::PkgId const pkg, Session& sceneOut)
{
    auto &idResources = mainView.m_idResources;
    auto &rResources = top_get<Resources>(mainView.m_topData, idResources);
    auto &rTopData = mainView.m_topData;

    // Add required scene data. This populates rTopData

    auto const [idActiveIds, idBasic, idDrawing, idDrawingRes, idComMats,
                idDelEnts, idDelTotal, idTPhys, idDeltaTimeIn, idNMesh, idNwt,
                idTest, idPhong, idPhongDirty, idVisual, idVisualDirty,
                idSpawner, idSpawnerEnts, idGravity, idBounds,
                idSpawnTimerA, idSpawnTimerB]
               = osp::unpack<22>(sceneOut.m_dataIds);

    auto &rActiveIds    = top_emplace< ActiveReg_t >    (rTopData, idActiveIds);
    auto &rBasic        = top_emplace< ACtxBasic >      (rTopData, idBasic);
    auto &rDrawing      = top_emplace< ACtxDrawing >    (rTopData, idDrawing);
    auto &rDrawingRes   = top_emplace< ACtxDrawingRes > (rTopData, idDrawingRes);
    auto &rDelEnts      = top_emplace< EntVector_t >    (rTopData, idDelEnts);
    auto &rDelTotal     = top_emplace< EntVector_t >    (rTopData, idDelTotal);
    auto &rTPhys        = top_emplace< ACtxTestPhys >   (rTopData, idTPhys);
    auto &rDeltaTimeIn  = top_emplace< float >          (rTopData, idDeltaTimeIn, 1.0f/60.0f);
    auto &rNMesh        = top_emplace< NamedMeshes >    (rTopData, idNMesh);
    auto &rNwt          = top_emplace< ACtxNwtWorld >   (rTopData, idNwt, gc_threadCount);
    auto &rTest         = top_emplace< PhysicsTestData >(rTopData, idTest);
    auto &rPhong        = top_emplace< EntSet_t >       (rTopData, idPhong);
    auto &rPhongDirty   = top_emplace< EntVector_t >    (rTopData, idPhongDirty);
    auto &rVisual       = top_emplace< EntSet_t >       (rTopData, idVisual);
    auto &rVisualDirty  = top_emplace< EntVector_t >    (rTopData, idVisualDirty);
    auto &rSpawner      = top_emplace< SpawnerVec_t >   (rTopData, idSpawner);
    auto &rSpawnerEnts  = top_emplace< EntVector_t >    (rTopData, idSpawnerEnts);
    auto &rGravity      = top_emplace< EntSet_t >       (rTopData, idGravity);
    auto &rBounds       = top_emplace< EntSet_t >       (rTopData, idBounds);
    auto &rSpawnTimerA  = top_emplace< float >          (rTopData, idSpawnTimerA, 0.0f);
    auto &rSpawnTimerB  = top_emplace< float >          (rTopData, idSpawnTimerB, 0.0f);

    auto builder = osp::TaskBuilder{mainView.m_rTasks, mainView.m_rTaskData};

    // Each corresponds to a bit in a 64-bit int
    auto const
    [
        tgCleanupEvt, tgResyncEvt, tgSyncEvt, tgSceneEvt, tgTimeEvt,
        tgEntDel,           tgEntNew,           tgEntReq,
        tgDelEntMod,        tgDelEntReq,        tgDelEntClr,
        tgDelTotalMod,      tgDelTotalReq,      tgDelTotalClr,
        tgTransformMod,     tgTransformDel,     tgTransformNew,     tgTransformReq,
        tgHierMod,          tgHierModEnd,       tgHierDel,          tgHierNew,          tgHierReq,
        tgShVisualDel,      tgShVisualMod,      tgShVisualReq,      tgShVisualClr,
        tgDrawDel,          tgDrawMod,          tgDrawReq,
        tgMeshDel,          tgMeshMod,          tgMeshReq,          tgMeshClr,
        tgTexDel,           tgTexMod,           tgTexReq,           tgTexClr,
        tgPhysBodyDel,      tgPhysBodyMod,      tgPhysBodyReq,
        tgPhysMod,          tgPhysReq,
        tgGravityReq,       tgGravityDel,       tgGravityNew,
        tgBoundsReq,        tgBoundsDel,        tgBoundsNew,
        tgSpawnMod,         tgSpawnReq,         tgSpawnClr
    ] = osp::unpack<52>(sceneOut.m_tags);

    // Set dependencies between tasks
    builder.tag(tgEntNew)           .depend_on({tgEntDel});
    builder.tag(tgEntReq)           .depend_on({tgEntDel, tgEntNew});
    builder.tag(tgDelEntReq)        .depend_on({tgDelEntMod});
    builder.tag(tgDelEntClr)        .depend_on({tgDelEntMod, tgDelEntReq});
    builder.tag(tgDelTotalReq)      .depend_on({tgDelTotalMod});
    builder.tag(tgDelTotalClr)      .depend_on({tgDelTotalMod, tgDelTotalReq});
    builder.tag(tgTransformNew)     .depend_on({tgTransformDel});
    builder.tag(tgTransformReq)     .depend_on({tgTransformDel, tgTransformNew, tgTransformMod});
    builder.tag(tgHierNew)          .depend_on({tgHierDel});
    builder.tag(tgHierModEnd)       .depend_on({tgHierDel, tgHierNew, tgHierMod});
    builder.tag(tgHierReq)          .depend_on({tgHierMod, tgHierModEnd});
    builder.tag(tgShVisualMod)      .depend_on({tgShVisualDel});
    builder.tag(tgShVisualReq)      .depend_on({tgShVisualDel, tgShVisualMod});
    builder.tag(tgShVisualClr)      .depend_on({tgShVisualDel, tgShVisualMod, tgShVisualReq});
    builder.tag(tgDrawMod)          .depend_on({tgDrawDel});
    builder.tag(tgDrawReq)          .depend_on({tgDrawDel, tgDrawMod});
    builder.tag(tgMeshMod)          .depend_on({tgMeshDel});
    builder.tag(tgMeshReq)          .depend_on({tgMeshDel, tgMeshMod});
    builder.tag(tgMeshClr)          .depend_on({tgMeshDel, tgMeshMod, tgMeshReq});
    builder.tag(tgTexMod)           .depend_on({tgTexDel});
    builder.tag(tgTexReq)           .depend_on({tgTexDel, tgTexMod});
    builder.tag(tgTexClr)           .depend_on({tgTexDel, tgTexMod, tgTexReq});
    builder.tag(tgPhysBodyMod)      .depend_on({tgPhysBodyDel});
    builder.tag(tgPhysBodyReq)      .depend_on({tgPhysBodyDel, tgPhysBodyMod, tgPhysMod});
    builder.tag(tgPhysReq)          .depend_on({tgPhysMod});
    builder.tag(tgGravityReq)       .depend_on({tgGravityDel, tgGravityNew});
    builder.tag(tgGravityNew)       .depend_on({tgGravityDel, tgPhysReq});
    builder.tag(tgBoundsDel)        .depend_on({tgBoundsReq});
    builder.tag(tgBoundsNew)        .depend_on({tgBoundsReq, tgBoundsDel});
    builder.tag(tgSpawnReq)         .depend_on({tgSpawnMod});
    builder.tag(tgSpawnClr)         .depend_on({tgSpawnMod, tgSpawnReq});

    builder.task().assign({tgResyncEvt}).data(
            "Set entity meshes and textures dirty",
            TopDataIds_t{             idDrawing},
            wrap_args([] (ACtxDrawing& rDrawing) noexcept
    {
        SysRender::set_dirty_all(rDrawing);
    }));

    static auto const resync_material = wrap_args([] (EntSet_t const& hasMaterial, EntVector_t& rDirty) noexcept
    {
        for (std::size_t const entInt : hasMaterial.ones())
        {
            rDirty.push_back(ActiveEnt(entInt));
        }
    });

    builder.task().assign({tgResyncEvt}).data(
            "Set all Phong material entities as dirty",
            TopDataIds_t{idPhong, idPhongDirty}, resync_material);

    builder.task().assign({tgResyncEvt}).data(
            "Set all MeshVisualizer material entities as dirty",
            TopDataIds_t{idVisual, idVisualDirty}, resync_material);

    builder.task().assign({tgSceneEvt, tgSyncEvt, tgShVisualClr}).data(
            "Clear dirty vectors for MeshVisualizer shader",
            TopDataIds_t{            idVisualDirty},
            wrap_args([] (EntVector_t& rDirty) noexcept
    {
        rDirty.clear();
    }));

    builder.task().assign({tgSceneEvt, tgSyncEvt, tgHierModEnd}).data(
            "Sort hierarchy (needed by renderer) after possible modifications",
            TopDataIds_t{           idBasic},
            wrap_args([] (ACtxBasic& rBasic) noexcept
    {
        SysHierarchy::sort(rBasic.m_hierarchy);
    }));

    // Deleters

    static auto const delete_ent_set = wrap_args([] (EntSet_t& set, EntVector_t const& rDelTotal) noexcept
    {
        for (ActiveEnt const ent : rDelTotal)
        {
            set.reset(std::size_t(ent));
        }
    });

    builder.task().assign({tgSceneEvt, tgDelEntClr}).data(
            "Clear delete vectors once we're done with it",
            TopDataIds_t{             idDelEnts},
            wrap_args([] (EntVector_t& rDelEnts) noexcept
    {
        rDelEnts.clear();
    }));

    builder.task().assign({tgSceneEvt, tgDelEntReq, tgDelTotalMod}).data(
            "Create DeleteTotal vector, which includes descendents of deleted hierarchy entities",
            TopDataIds_t{           idBasic,                   idDelEnts,             idDelTotal},
            wrap_args([] (ACtxBasic& rBasic, EntVector_t const& rDelEnts, EntVector_t& rDelTotal) noexcept
    {
        auto const &delFirst    = std::cbegin(rDelEnts);
        auto const &delLast     = std::cend(rDelEnts);

        rDelTotal.assign(delFirst, delLast);

        SysHierarchy::update_delete_descendents(
                rBasic.m_hierarchy, delFirst, delLast,
                [&rDelTotal] (ActiveEnt const ent)
        {
            rDelTotal.push_back(ent);
        });
    }));

    builder.task().assign({tgSceneEvt, tgDelEntReq, tgHierMod}).data(
            "Cut deleted entities out of hierarchy",
            TopDataIds_t{           idBasic,                   idDelEnts},
            wrap_args([] (ACtxBasic& rBasic, EntVector_t const& rDelEnts) noexcept
    {
        auto const &delFirst    = std::cbegin(rDelEnts);
        auto const &delLast     = std::cend(rDelEnts);

        SysHierarchy::update_delete_cut(rBasic.m_hierarchy, delFirst, delLast);
    }));

    builder.task().assign({tgSceneEvt, tgDelTotalReq, tgTransformDel, tgHierDel}).data(
            "Delete basic components",
            TopDataIds_t{           idBasic,                   idDelTotal},
            wrap_args([] (ACtxBasic& rBasic, EntVector_t const& rDelTotal) noexcept
    {
        update_delete_basic(rBasic, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    builder.task().assign({tgSceneEvt, tgDelTotalReq, tgDrawDel}).data(
            "Delete drawing components",
            TopDataIds_t{              idDrawing,                  idDelTotal},
            wrap_args([] (ACtxDrawing& rDrawing, EntVector_t const& rDelTotal) noexcept
    {
        SysRender::update_delete_drawing(rDrawing, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    builder.task().assign({tgSceneEvt, tgDelTotalReq, tgEntDel}).data(
            "Delete Entity IDs",
            TopDataIds_t{             idActiveIds,                    idDelTotal},
            wrap_args([] (ActiveReg_t& rActiveIds, EntVector_t const& rDelTotal) noexcept
    {
        for (ActiveEnt const ent : rDelTotal)
        {
            if (rActiveIds.exists(ent))
            {
                rActiveIds.remove(ent);
            }
        }
    }));

    builder.task().assign({tgSceneEvt, tgDelTotalReq, tgPhysBodyDel}).data(
            "Delete Physics components",
            TopDataIds_t{              idTPhys,              idNwt,                   idDelTotal},
            wrap_args([] (ACtxTestPhys& rTPhys, ACtxNwtWorld& rNwt, EntVector_t const& rDelTotal) noexcept
    {
        auto const &delFirst    = std::cbegin(rDelTotal);
        auto const &delLast     = std::cend(rDelTotal);

        SysPhysics::update_delete_phys      (rTPhys.m_physics,  delFirst, delLast);
        SysPhysics::update_delete_shapes    (rTPhys.m_physics,  delFirst, delLast);
        SysPhysics::update_delete_hier_body (rTPhys.m_hierBody, delFirst, delLast);
        ospnewton::SysNewton::update_delete (rNwt,              delFirst, delLast);
    }));

    builder.task().assign({tgSceneEvt, tgDelTotalReq, tgGravityDel}).data(
            "Delete gravity components",
            TopDataIds_t{idGravity, idDelTotal}, delete_ent_set);

    builder.task().assign({tgSceneEvt, tgDelTotalReq, tgBoundsDel}).data(
            "Delete bounds components",
            TopDataIds_t{idBounds, idDelTotal}, delete_ent_set);

    builder.task().assign({tgSceneEvt, tgDelTotalReq, tgShVisualDel}).data(
            "Delete MeshVisualizer material components",
            TopDataIds_t{idVisual, idDelTotal}, delete_ent_set);

    // Physics

    builder.task().assign({tgSceneEvt, tgGravityReq}).data(
            "Apply gravity (-Z 9.81N force) to entities in the rGravity set",
            TopDataIds_t{              idTPhys,          idGravity   },
            wrap_args([] (ACtxTestPhys& rTPhys, EntSet_t& rGravity) noexcept
    {
        acomp_storage_t<ACompPhysNetForce> &rNetForce
                = rTPhys.m_physIn.m_physNetForce;
        for (std::size_t const entInt : rGravity.ones())
        {
            ActiveEnt const ent = ActiveEnt(entInt);
            ACompPhysNetForce &rEntNetForce = rNetForce.contains(ent)
                                            ? rNetForce.get(ent)
                                            : rNetForce.emplace(ent);

            rEntNetForce.z() -= 9.81f * rTPhys.m_physics.m_physDynamic.get(ent).m_totalMass;
        }
    }));

    builder.task().assign({tgTimeEvt, tgTransformMod, tgHierMod, tgPhysMod}).data(
            "Update Newton world",
            TopDataIds_t{           idBasic,              idTPhys,              idNwt,           idDeltaTimeIn },
            wrap_args([] (ACtxBasic& rBasic, ACtxTestPhys& rTPhys, ACtxNwtWorld& rNwt, float const deltaTimeIn) noexcept
    {
        auto const physIn = osp::ArrayView<ACtxPhysInputs>(&rTPhys.m_physIn, 1);
        ospnewton::SysNewton::update_world(
                rTPhys.m_physics, rNwt, deltaTimeIn, physIn,
                rBasic.m_hierarchy,
                rBasic.m_transform, rBasic.m_transformControlled,
                rBasic.m_transformMutable);
    }));


    builder.task().assign({tgSceneEvt, tgSyncEvt, tgPhysReq, tgPhysBodyReq}).data(
            "Update Entities with Newton colliders",
            TopDataIds_t{              idTPhys,               idNwt },
            wrap_args([] (ACtxTestPhys& rTPhys, ACtxNwtWorld& rNwt) noexcept
    {
        ospnewton::SysNewton::update_colliders(
                rTPhys.m_physics, rNwt,
                std::exchange(rTPhys.m_physIn.m_colliderDirty, {}));
    }));


    builder.task().assign({tgSceneEvt, tgPhysReq, tgBoundsReq, tgDelEntMod}).data(
            "Bounds check",
            TopDataIds_t{                 idBasic,                idBounds,             idDelEnts   },
            wrap_args([] (ACtxBasic const& rBasic, EntSet_t const& rBounds, EntVector_t& rDelEnts) noexcept
    {
        for (std::size_t const entInt : rBounds.ones())
        {
            auto const ent = ActiveEnt(entInt);
            ACompTransform const &entTf = rBasic.m_transform.get(ent);
            if (entTf.m_transform.translation().z() < -10)
            {
                rDelEnts.push_back(ent);
            }
        }
    }));

    // Shape spawning

    builder.task().assign({tgSceneEvt, tgSpawnReq, tgEntNew}).data(
            "Create entities for requested shapes to spawn",
            TopDataIds_t{             idActiveIds,              idSpawner,             idSpawnerEnts },
            wrap_args([] (ActiveReg_t& rActiveIds, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts) noexcept
    {
        rSpawnerEnts.resize(rSpawner.size() * 2);
        rActiveIds.create(std::begin(rSpawnerEnts), std::end(rSpawnerEnts));
    }));

    builder.task().assign({tgSceneEvt, tgSpawnReq, tgTransformNew, tgHierNew}).data(
            "Add hierarchy and transform to spawned shapes",
            TopDataIds_t{           idBasic,              idSpawner,             idSpawnerEnts },
            wrap_args([] (ACtxBasic& rBasic, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts) noexcept
    {
        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            ActiveEnt const root    = rSpawnerEnts[i * 2];
            ActiveEnt const child   = rSpawnerEnts[i * 2 + 1];

            rBasic.m_transform.emplace(root, ACompTransform{osp::Matrix4::translation(spawn.m_position)});
            rBasic.m_transform.emplace(child, ACompTransform{Matrix4::scaling(spawn.m_size)});
            SysHierarchy::add_child(rBasic.m_hierarchy, rBasic.m_hierRoot, root);
            SysHierarchy::add_child(rBasic.m_hierarchy, root, child);
        }
    }));

    builder.task().assign({tgSceneEvt, tgSpawnReq, tgMeshMod, tgDrawMod, tgShVisualMod}).data(
            "Add mesh and material to spawned shapes",
            TopDataIds_t{             idDrawing,              idSpawner,             idSpawnerEnts,             idNMesh,          idVisual,          idVisualDirty,                idActiveIds},
            wrap_args([] (ACtxDrawing& rDrawing, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts, NamedMeshes& rNMesh, EntSet_t& rMat, EntVector_t& rMatDirty, ActiveReg_t const& rActiveIds ) noexcept
    {
        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            ActiveEnt const child   = rSpawnerEnts[i * 2 + 1];

            rDrawing.m_mesh.emplace( child, rDrawing.m_meshRefCounts.ref_add(rNMesh.m_shapeToMesh.at(spawn.m_shape)) );
            rDrawing.m_meshDirty.push_back(child);

            rMat.ints().resize(rActiveIds.vec().capacity());
            rMat.set(std::size_t(child));
            rMatDirty.push_back(child);

            rDrawing.m_visible.emplace(child);
            rDrawing.m_opaque.emplace(child);
        }
    }));

    builder.task().assign({tgSceneEvt, tgSpawnReq, tgPhysBodyMod}).data(
            "Add physics to spawned shapes",
            TopDataIds_t{              idSpawner,             idSpawnerEnts,              idTPhys },
            wrap_args([] (SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts, ACtxTestPhys& rTPhys) noexcept
    {
        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            ActiveEnt const root    = rSpawnerEnts[i * 2];
            ActiveEnt const child   = rSpawnerEnts[i * 2 + 1];

            rTPhys.m_physics.m_hasColliders.emplace(root);
            rTPhys.m_physics.m_physBody.emplace(root);
            if (spawn.m_mass != 0.0f)
            {
                rTPhys.m_physics.m_physLinearVel.emplace(root);
                rTPhys.m_physics.m_physAngularVel.emplace(root);
                ACompPhysDynamic &rDyn = rTPhys.m_physics.m_physDynamic.emplace(root);
                rDyn.m_totalMass = spawn.m_mass;

                rTPhys.m_physIn.m_setVelocity.emplace_back(root, spawn.m_velocity);
                osp::Vector3 const inertia
                        = osp::phys::collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);
                rTPhys.m_hierBody.m_ownDyn.emplace( child, ACompSubBody{ inertia, spawn.m_mass } );
            }


            rTPhys.m_physics.m_shape.emplace(child, spawn.m_shape);
            rTPhys.m_physics.m_solid.emplace(child);
            rTPhys.m_physIn.m_colliderDirty.push_back(child);
        }
    }));


    builder.task().assign({tgSceneEvt, tgSpawnReq, tgGravityNew}).data(
            "Add gravity to spawned shapes",
            TopDataIds_t{                    idSpawner,                   idSpawnerEnts,          idGravity,                   idActiveIds },
            wrap_args([] (SpawnerVec_t const& rSpawner, EntVector_t const& rSpawnerEnts, EntSet_t& rGravity, ActiveReg_t const& rActiveIds) noexcept
    {
        rGravity.ints().resize(rActiveIds.vec().capacity());

        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            if (spawn.m_mass == 0)
            {
                continue;
            }

            ActiveEnt const root    = rSpawnerEnts[i * 2];

            rGravity.set(std::size_t(root));
        }
    }));

    builder.task().assign({tgSceneEvt, tgSpawnReq, tgBoundsNew}).data(
            "Add bounds to spawned shapes",
            TopDataIds_t{                    idSpawner,                   idSpawnerEnts,          idBounds,                   idActiveIds },
            wrap_args([] (SpawnerVec_t const& rSpawner, EntVector_t const& rSpawnerEnts, EntSet_t& rBounds, ActiveReg_t const& rActiveIds) noexcept
    {
        rBounds.ints().resize(rActiveIds.vec().capacity());

        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            if (spawn.m_mass == 0)
            {
                continue;
            }

            ActiveEnt const root    = rSpawnerEnts[i * 2];

            rBounds.set(std::size_t(root));
        }
    }));

    builder.task().assign({tgSceneEvt, tgSpawnClr}).data(
            "Clear Shape Spawning vector after use",
            TopDataIds_t{              idSpawner},
            wrap_args([] (SpawnerVec_t& rSpawner) noexcept
    {
        rSpawner.clear();
    }));

    builder.task().assign({tgTimeEvt, tgSpawnMod}).data(
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

    builder.task().assign({tgTimeEvt, tgSpawnMod}).data(
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


    // Cleanup

    builder.task().assign({tgCleanupEvt}).data(
            "Clean up NamedMeshes mesh and texture owners",
            TopDataIds_t{             idDrawing,             idNMesh},
            wrap_args([] (ACtxDrawing& rDrawing, NamedMeshes& rNMesh) noexcept
    {
        for ([[maybe_unused]] auto && [_, rOwner] : std::exchange(rNMesh.m_shapeToMesh, {}))
        {
            rDrawing.m_meshRefCounts.ref_release(std::move(rOwner));
        }

        for ([[maybe_unused]] auto && [_, rOwner] : std::exchange(rNMesh.m_namedMeshs, {}))
        {
            rDrawing.m_meshRefCounts.ref_release(std::move(rOwner));
        }
    }));

    builder.task().assign({tgCleanupEvt}).data(
            "Clean up scene and resource owners",
            TopDataIds_t{             idDrawing,                idDrawingRes,           idResources},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources) noexcept
    {
        SysRender::clear_owners(rDrawing);
        SysRender::clear_resource_owners(rDrawingRes, rResources);
    }));

    // Setup the scene

    // Convenient function to get a reference-counted mesh owner
    auto const quick_add_mesh = [&rResources, &rDrawing, &rDrawingRes, pkg] (std::string_view const name) -> MeshIdOwner_t
    {
        osp::ResId const res = rResources.find(osp::restypes::gc_mesh, pkg, name);
        assert(res != lgrn::id_null<osp::ResId>());
        MeshId const meshId = SysRender::own_mesh_resource(rDrawing, rDrawingRes, rResources, res);
        return rDrawing.m_meshRefCounts.ref_add(meshId);
    };

    // Acquire mesh resources from Package
    rNMesh.m_shapeToMesh.emplace(EShape::Box,       quick_add_mesh("cube"));
    rNMesh.m_shapeToMesh.emplace(EShape::Cylinder,  quick_add_mesh("cylinder"));
    rNMesh.m_shapeToMesh.emplace(EShape::Sphere,    quick_add_mesh("sphere"));
    rNMesh.m_namedMeshs.emplace("floor", quick_add_mesh("grid64solid"));

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
    rVisual.ints().resize(rActiveIds.vec().capacity());
    rVisual.set(std::size_t(floorMeshEnt));
    rVisualDirty.push_back(floorMeshEnt);

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

    // Add collider to floor root entity (yeah lol it's a big cube)
    rSpawner.emplace_back(SpawnShape{
        .m_position = sc_floorPos,
        .m_velocity = sc_floorSize,
        .m_size     = sc_floorSize,
        .m_mass     = 0.0f,
        .m_shape    = EShape::Box
    });


}


//-----------------------------------------------------------------------------

struct PhysicsTestControls
{
    PhysicsTestControls(osp::input::UserInputHandler &rInputs)
     :  m_btnThrow(rInputs.button_subscribe("debug_throw"))
    { }

    osp::input::EButtonControlIndex m_btnThrow;
};

void PhysicsTest::setup_renderer_gl(
        MainView        mainView,
        Session const&  appIn,
        Session const&  sceneIn,
        Session const&  magnumIn,
        Session const&  sceneRenderOut) noexcept
{
    using namespace osp::shader;

    auto &rTopData = mainView.m_topData;

    auto const
    [
            idActiveApp, idRenderGl, idUserInput
    ] = osp::unpack<3>(magnumIn.m_dataIds);
    auto &rRenderGl     = osp::top_get<RenderGL>(rTopData, idRenderGl);
    auto &rUserInput    = osp::top_get<UserInputHandler>(rTopData, idUserInput);

    auto const [idScnRender, idGroupFwd, idDrawPhong, idDrawVisual, idCamEnt, idCamCtrl, idControls]
            = osp::unpack<7>(sceneRenderOut.m_dataIds);
    auto &rScnRender    = top_emplace< ACtxSceneRenderGL >     (rTopData, idScnRender);
    auto &rGroupFwd     = top_emplace< RenderGroup >           (rTopData, idGroupFwd);
    auto &rDrawPhong    = top_emplace< ACtxDrawPhong >         (rTopData, idDrawPhong);
    auto &rDrawVisual   = top_emplace< ACtxDrawMeshVisualizer >(rTopData, idDrawVisual);
    auto &rCamEnt       = top_emplace< ActiveEnt >             (rTopData, idCamEnt);
    auto &rCamCtrl      = top_emplace< ACtxCameraController >  (rTopData, idCamCtrl, rUserInput);
    auto &rControls     = top_emplace< PhysicsTestControls  >  (rTopData, idControls, rUserInput);

    // Setup Phong shaders
    auto const texturedFlags        = Phong::Flag::DiffuseTexture | Phong::Flag::AlphaMask | Phong::Flag::AmbientTexture;
    rDrawPhong.m_shaderDiffuse      = Phong{texturedFlags, 2};
    rDrawPhong.m_shaderUntextured   = Phong{{}, 2};
    rDrawPhong.assign_pointers(rScnRender, rRenderGl);

    // Setup Mesh Visualizer shader
    rDrawVisual.m_shader = MeshVisualizer{ MeshVisualizer::Flag::Wireframe };
    rDrawVisual.assign_pointers(rScnRender, rRenderGl);

    [[maybe_unused]]
    auto const [idActiveIds, idBasic, idDrawing, idDrawingRes, idComMats,
                idDelEnts, idDelTotal, idTPhys, idDeltaTimeIn, idNMesh, idNwt,
                idTest, idPhong, idPhongDirty, idVisual, idVisualDirty,
                idSpawner, idSpawnerEnts, idGravity, idBounds]
               = osp::unpack<20>(sceneIn.m_dataIds);

    auto &rBasic = top_get< ACtxBasic >(rTopData, idBasic);

    auto const [idResources] = osp::unpack<1>(appIn.m_dataIds);

    // Select first camera for rendering
    rCamEnt = rBasic.m_camera.at(0);
    rBasic.m_camera.get(rCamEnt).set_aspect_ratio(
            osp::Vector2(Magnum::GL::defaultFramebuffer.viewport().size()));
    SysRender::add_draw_transforms_recurse(
            rBasic.m_hierarchy, rScnRender.m_drawTransform, rCamEnt);

    // Set initial position of camera slightly above the ground
    rCamCtrl.m_target = osp::Vector3{0.0f, 2.0f, 0.0f};

    auto builder = osp::TaskBuilder{mainView.m_rTasks, mainView.m_rTaskData};

    auto const
    [
        tgRenderEvt, tgInputEvt, tgGlUse
    ] = osp::unpack<3>(magnumIn.m_tags);

    auto const
    [
        tgCleanupEvt, tgResyncEvt, tgSyncEvt, tgSceneEvt, tgTimeEvt,
        tgEntDel,           tgEntNew,           tgEntReq,
        tgDelEntMod,        tgDelEntReq,        tgDelEntClr,
        tgDelTotalMod,      tgDelTotalReq,      tgDelTotalClr,
        tgTransformMod,     tgTransformDel,     tgTransformNew,     tgTransformReq,
        tgHierMod,          tgHierModEnd,       tgHierDel,          tgHierNew,          tgHierReq,
        tgShVisualDel,      tgShVisualMod,      tgShVisualReq,      tgShVisualClr,
        tgDrawDel,          tgDrawMod,          tgDrawReq,
        tgMeshDel,          tgMeshMod,          tgMeshReq,          tgMeshClr,
        tgTexDel,           tgTexMod,           tgTexReq,           tgTexClr,
        tgPhysBodyDel,      tgPhysBodyMod,      tgPhysBodyReq,
        tgPhysMod,          tgPhysReq,
        tgGravityReq,       tgGravityDel,       tgGravityNew,
        tgBoundsReq,        tgBoundsDel,        tgBoundsNew,
        tgSpawnMod,         tgSpawnReq,         tgSpawnClr
    ] = osp::unpack<52>(sceneIn.m_tags);

    auto const
    [
        tgDrawGlDel,        tgDrawGlMod,        tgDrawGlReq,
        tgMeshGlMod,        tgMeshGlReq,
        tgTexGlMod,         tgTexGlReq,
        tgEntTexMod,        tgEntTexReq,
        tgEntMeshMod,       tgEntMeshReq,
        tgGroupFwdDel,      tgGroupFwdMod,      tgGroupFwdReq,
        tgDrawTransformDel, tgDrawTransformMod, tgDrawTransformReq
    ] = osp::unpack<17>(sceneRenderOut.m_tags);

    builder.tag(tgDrawGlMod)           .depend_on({tgDrawGlDel});
    builder.tag(tgDrawGlReq)           .depend_on({tgDrawGlDel, tgDrawGlMod});
    builder.tag(tgMeshGlReq)           .depend_on({tgMeshGlMod});
    builder.tag(tgTexGlReq)            .depend_on({tgTexGlMod});
    builder.tag(tgEntTexReq)           .depend_on({tgEntTexMod});
    builder.tag(tgEntMeshReq)          .depend_on({tgEntMeshMod});
    builder.tag(tgGroupFwdMod)         .depend_on({tgGroupFwdDel});
    builder.tag(tgGroupFwdReq)         .depend_on({tgGroupFwdDel, tgGroupFwdMod});
    builder.tag(tgDrawTransformMod)    .depend_on({tgDrawTransformDel});
    builder.tag(tgDrawTransformReq)    .depend_on({tgDrawTransformMod});


    builder.task().assign({tgInputEvt, tgGlUse}).data(
            "Move Camera",
            TopDataIds_t{           idBasic,                      idCamCtrl,                idCamEnt,          idDeltaTimeIn},
            wrap_args([] (ACtxBasic& rBasic, ACtxCameraController& rCamCtrl, ActiveEnt const camEnt, float const deltaTimeIn) noexcept
    {
        SysCameraController::update_view(rCamCtrl,
                rBasic.m_transform.get(camEnt), deltaTimeIn);
        SysCameraController::update_move(
                rCamCtrl,
                rBasic.m_transform.get(camEnt),
                deltaTimeIn, true);
    }));

    builder.task().assign({tgSyncEvt, tgGlUse, tgTexGlMod, tgTexGlMod}).data(
            "Synchronize used mesh and texture Resources with GL",
            TopDataIds_t{                      idDrawingRes,                idResources,          idRenderGl},
            wrap_args([] (ACtxDrawingRes const& rDrawingRes, osp::Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::sync_scene_resources(rDrawingRes, rResources, rRenderGl);
    }));

    builder.task().assign({tgSyncEvt, tgTexGlReq, tgEntTexMod}).data(
            "Assign GL textures to entities with scene textures",
            TopDataIds_t{             idDrawing,                idDrawingRes,                   idScnRender,          idRenderGl},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::assign_textures(rDrawing.m_diffuseTex, rDrawingRes.m_texToRes, rDrawing.m_diffuseDirty, rScnRender.m_diffuseTexId, rRenderGl);
    }));

    builder.task().assign({tgSyncEvt, tgTexGlReq, tgEntMeshMod, tgMeshReq}).data(
            "Assign GL meshes to entities with scene meshes",
            TopDataIds_t{             idDrawing,                idDrawingRes,                   idScnRender,          idRenderGl},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::assign_meshes(rDrawing.m_mesh, rDrawingRes.m_meshToRes, rDrawing.m_meshDirty, rScnRender.m_meshId, rRenderGl);
    }));

    builder.task().assign({tgSyncEvt, tgShVisualReq, tgGroupFwdMod}).data(
            "Sync MeshVisualizer shader entities",
            TopDataIds_t{                   idVisualDirty,          idVisual,               idGroupFwd,                     idDrawVisual},
            wrap_args([] (EntVector_t const& rDirty, EntSet_t const& rMaterial, RenderGroup& rGroup, ACtxDrawMeshVisualizer& rVisualizer) noexcept
    {
        sync_visualizer(std::cbegin(rDirty), std::cend(rDirty), rMaterial, rGroup.m_entities, rVisualizer);
    }));

    // TODO: phong shader

    builder.task().assign({tgSyncEvt, tgDelTotalReq, tgDrawGlDel}).data(
            "Delete GL components",
            TopDataIds_t{                   idScnRender,                   idDelTotal},
            wrap_args([] (ACtxSceneRenderGL& rScnRender, EntVector_t const& rDelTotal) noexcept
    {
        SysRenderGL::update_delete(rScnRender, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    builder.task().assign({tgSyncEvt, tgDelTotalReq, tgGroupFwdDel}).data(
            "Delete entities from render groups",
            TopDataIds_t{             idGroupFwd,                idDelTotal},
            wrap_args([] (RenderGroup& rGroup, EntVector_t const& rDelTotal) noexcept
    {
        rGroup.m_entities.remove(std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    builder.task().assign({tgSyncEvt, tgShVisualReq, tgHierReq, tgTransformReq, tgDrawTransformMod}).data(
            "Add and calculate draw transforms (hackily specific to MeshVisualizer's entities)",
            TopDataIds_t{                 idBasic,                   idVisualDirty,                   idScnRender},
            wrap_args([] (ACtxBasic const& rBasic, EntVector_t const& rVisualDirty, ACtxSceneRenderGL& rScnRender) noexcept
    {
        SysRender::assure_draw_transforms(rBasic.m_hierarchy, rScnRender.m_drawTransform, std::cbegin(rVisualDirty), std::cend(rVisualDirty));
        SysRender::update_draw_transforms(rBasic.m_hierarchy, rBasic.m_transform, rScnRender.m_drawTransform);
    }));

    builder.task().assign({tgRenderEvt, tgGlUse, tgDrawTransformReq, tgGroupFwdReq, tgDrawReq, tgEntTexMod, tgEntMeshMod}).data(
            "Render and display scene",
            TopDataIds_t{                 idBasic,                   idDrawing,                   idScnRender,          idRenderGl,                   idGroupFwd,                idCamEnt},
            wrap_args([] (ACtxBasic const& rBasic, ACtxDrawing const& rDrawing, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl, RenderGroup const& rGroupFwd, ActiveEnt const camEnt) noexcept
    {
        using Magnum::GL::Framebuffer;
        using Magnum::GL::FramebufferClear;
        using Magnum::GL::Texture2D;

        // Bind offscreen FBO
        Framebuffer &rFbo = rRenderGl.m_fbo;
        rFbo.bind();

        // Clear it
        rFbo.clear( FramebufferClear::Color | FramebufferClear::Depth
                    | FramebufferClear::Stencil);

        ACompCamera const &rCamera = rBasic.m_camera.get(camEnt);
        ACompDrawTransform const &cameraDrawTf
                = rScnRender.m_drawTransform.get(camEnt);
        ViewProjMatrix viewProj{
                cameraDrawTf.m_transformWorld.inverted(),
                rCamera.calculate_projection()};

        // Forward Render fwd_opaque group to FBO
        SysRenderGL::render_opaque(rGroupFwd, rDrawing.m_visible, viewProj);

        Texture2D &rFboColor = rRenderGl.m_texGl.get(rRenderGl.m_fboColor);
        SysRenderGL::display_texture(rRenderGl, rFboColor);
    }));

    builder.task().assign({tgInputEvt, tgSpawnMod}).data(
            "Throw spheres when pressing space",
            TopDataIds_t{                 idBasic,              idSpawner,         idCamEnt,                      idCamCtrl,                     idControls},
            wrap_args([] (ACtxBasic const& rBasic, SpawnerVec_t& rSpawner, ActiveEnt camEnt, ACtxCameraController& rCamCtrl, PhysicsTestControls& rControls) noexcept
    {
        // Throw a sphere when the throw button is pressed
        if (rCamCtrl.m_controls.button_held(rControls.m_btnThrow))
        {
            Matrix4 const &camTf = rBasic.m_transform.get(camEnt).m_transform;
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
}

} // namespace testapp::physicstest
