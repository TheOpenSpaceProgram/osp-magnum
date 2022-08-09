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

constexpr float gc_physTimestep = 1.0 / 60.0f;
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

void PhysicsTest::setup_scene(MainView mainView, osp::PkgId const pkg, Session const& sceneOut)
{
    auto &idResources = mainView.m_idResources;
    auto &rResources = top_get<Resources>(mainView.m_topData, idResources);
    auto &rTopData = mainView.m_topData;

    // Add required scene data. This populates rTopData

    auto const [idActiveIds, idBasic, idDrawing, idDrawingRes, idComMats,
                idDelEnts, idDelTotal, idTPhys, idNMesh, idNwt, idTest,
                idPhong, idPhongDirty, idVisual, idVisualDirty,
                idSpawner, idSpawnerEnts, idGravity, idBounds]
               = osp::unpack<19>(sceneOut.m_dataIds);

    auto &rActiveIds    = top_emplace< ActiveReg_t >    (rTopData, idActiveIds);
    auto &rBasic        = top_emplace< ACtxBasic >      (rTopData, idBasic);
    auto &rDrawing      = top_emplace< ACtxDrawing >    (rTopData, idDrawing);
    auto &rDrawingRes   = top_emplace< ACtxDrawingRes > (rTopData, idDrawingRes);
    auto &rDelEnts      = top_emplace< EntVector_t >    (rTopData, idDelEnts);
    auto &rDelTotal     = top_emplace< EntVector_t >    (rTopData, idDelTotal);
    auto &rTPhys        = top_emplace< ACtxTestPhys >   (rTopData, idTPhys);
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

    auto builder = osp::TaskBuilder{mainView.m_rTasks, mainView.m_rTaskData};

    // Each tag is just 1 bit in a 64-bit int
    auto const [tgUpdScene, tgUpdTime, tgUpdSync, tgUpdResync, tgUpdCleanup, tgUpdSpawn,
                tgPrereqActiveIds, tgFactorActiveIds, tgNeedActiveIds, tgUsesActiveIds,
                tgPrereqHier, tgFactorHier, tgUsesHier, tgFinishHier, tgNeedHier,
                tgPrereqTransform, tgFactorTransform, tgNeedTransform,
                tgPrereqMesh, tgFactorMesh, tgNeedMesh,
                tgPrereqTex, tgFactorTex, tgNeedTex,
                tgStartMeshDirty, tgFactorMeshDirty, tgNeedMeshDirty,
                tgStartVisualDirty, tgFactorVisualDirty, tgNeedVisualDirty,
                tgStartCollideDirty, tgFactorCollideDirty, tgNeedCollideDirty,
                tgFactorPhysForces, tgNeedPhysForces,
                tgApplyGravity, tgFactorGravity,
                tgPrereqPhysMod, tgFactorPhysMod, tgNeedPhysMod,
                tgProvideCollide, tgNeedCollide,
                tgStartSpawn, tgFactorSpawn, tgNeedSpawn,
                tgProvideSpawnEnt, tgNeedSpawnEnt,
                tgStartDelEnts, tgFactorDelEnts, tgNeedDelEnts,
                tgProvideDelTotal, tgNeedDelTotal]
               = osp::unpack<52>(sceneOut.m_tags);

    builder.tag(tgFactorActiveIds)      .depend_on({tgPrereqActiveIds});
    builder.tag(tgNeedActiveIds)        .depend_on({tgPrereqActiveIds, tgFactorActiveIds});

    builder.tag(tgFinishHier)           .depend_on({tgPrereqHier, tgFactorHier});
    builder.tag(tgNeedHier)             .depend_on({tgPrereqHier, tgFinishHier, tgFactorHier});

    builder.tag(tgFactorTransform)      .depend_on({tgPrereqTransform});
    builder.tag(tgNeedTransform)        .depend_on({tgPrereqTransform, tgFactorTransform});

    builder.tag(tgFactorMesh)           .depend_on({tgPrereqMesh});
    builder.tag(tgNeedMesh)             .depend_on({tgPrereqMesh, tgFactorMesh});

    builder.tag(tgFactorTex)            .depend_on({tgPrereqTex});
    builder.tag(tgNeedTex)              .depend_on({tgPrereqTex, tgFactorTex});

    builder.tag(tgFactorMeshDirty)      .depend_on({tgStartMeshDirty});
    builder.tag(tgNeedMeshDirty)        .depend_on({tgStartMeshDirty, tgFactorMeshDirty});

    builder.tag(tgFactorVisualDirty)    .depend_on({tgStartVisualDirty});
    builder.tag(tgNeedVisualDirty)      .depend_on({tgStartVisualDirty, tgFactorVisualDirty});

    builder.tag(tgFactorCollideDirty)   .depend_on({tgStartCollideDirty});
    builder.tag(tgNeedCollideDirty)     .depend_on({tgStartCollideDirty, tgFactorCollideDirty});
    builder.tag(tgNeedCollide)          .depend_on({tgProvideCollide});

    builder.tag(tgFactorGravity)        .depend_on({tgApplyGravity});

    builder.tag(tgNeedPhysForces)       .depend_on({tgFactorPhysForces});

    builder.tag(tgFactorPhysMod)        .depend_on({tgPrereqPhysMod});
    builder.tag(tgNeedPhysMod)          .depend_on({tgPrereqPhysMod, tgFactorPhysMod});

    builder.tag(tgFactorSpawn)          .depend_on({tgStartSpawn}).enqueues(tgUpdSpawn);
    builder.tag(tgNeedSpawn)            .depend_on({tgStartSpawn, tgFactorSpawn});
    builder.tag(tgNeedSpawnEnt)         .depend_on({tgProvideSpawnEnt, tgStartSpawn});

    builder.tag(tgFactorDelEnts)        .depend_on({tgStartDelEnts});
    builder.tag(tgNeedDelEnts)          .depend_on({tgStartDelEnts, tgFactorDelEnts});

    builder.tag(tgNeedDelTotal)         .depend_on({tgProvideDelTotal});


    builder.task().assign({tgUpdScene, tgStartVisualDirty}).data(
            "Clear dirty vectors for MeshVisualizer shader",
            TopDataIds_t{            idVisualDirty},
            wrap_args([] (EntVector_t& rDirty) noexcept
    {
        rDirty.clear();
    }));

    builder.task().assign({tgUpdResync}).data(
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

    builder.task().assign({tgUpdResync}).data(
            "Set all Phong material entities as dirty",
            TopDataIds_t{idPhong, idPhongDirty}, resync_material);

    builder.task().assign({tgUpdResync}).data(
            "Set all MeshVisualizer material entities as dirty",
            TopDataIds_t{idVisual, idVisualDirty}, resync_material);

    builder.task().assign({tgUpdCleanup}).data(
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

    builder.task().assign({tgUpdCleanup}).data(
            "Clear scene and resource owners on cleanup",
            TopDataIds_t{             idDrawing,                idDrawingRes,           idResources},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources) noexcept
    {
        SysRender::clear_owners(rDrawing);
        SysRender::clear_resource_owners(rDrawingRes, rResources);
    }));


    builder.task().assign({tgUpdScene, tgFinishHier}).data(
            "Sort hierarchy (needed by renderer) after possible modifications",
            TopDataIds_t{           idBasic},
            wrap_args([] (ACtxBasic& rBasic) noexcept
    {
        SysHierarchy::sort(rBasic.m_hierarchy);
    }));

    builder.task().assign({tgUpdScene, tgStartDelEnts}).data(
            "Clear delete vector to start deleting new entities this frame",
            TopDataIds_t{             idDelEnts},
            wrap_args([] (EntVector_t& rDelEnts) noexcept
    {
        rDelEnts.clear();
    }));

    builder.task().assign({tgUpdScene, tgNeedDelEnts, tgProvideDelTotal}).data(
            "Create DeleteTotal vector, which includes descendents of deleted hierarchy entities",
            TopDataIds_t{           idBasic,                   idDelEnts,             idDelTotal},
            wrap_args([] (ACtxBasic& rBasic, EntVector_t const& rDelEnts, EntVector_t& rDelTotal) noexcept
    {
        auto const &delFirst    = std::cbegin(rDelEnts);
        auto const &delLast     = std::cend(rDelEnts);

        // Cut deleted entities out of the hierarchy
        SysHierarchy::update_delete_cut(rBasic.m_hierarchy, delFirst, delLast);

        rDelTotal.assign(delFirst, delLast);

        SysHierarchy::update_delete_descendents(
                rBasic.m_hierarchy, delFirst, delLast,
                [&rDelTotal] (ActiveEnt const ent)
        {
            rDelTotal.push_back(ent);
        });
    }));

    builder.task().assign({tgUpdScene, tgNeedDelTotal, tgPrereqTransform, tgPrereqHier}).data(
            "Delete basic components",
            TopDataIds_t{           idBasic,                   idDelTotal},
            wrap_args([] (ACtxBasic& rBasic, EntVector_t const& rDelTotal) noexcept
    {
        update_delete_basic(rBasic, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    builder.task().assign({tgUpdScene, tgNeedDelTotal, tgPrereqMesh, tgPrereqTex}).data(
            "Delete drawing components",
            TopDataIds_t{              idDrawing,                  idDelTotal},
            wrap_args([] (ACtxDrawing& rDrawing, EntVector_t const& rDelTotal) noexcept
    {
        SysRender::update_delete_drawing(rDrawing, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    builder.task().assign({tgUpdScene, tgNeedDelTotal, tgPrereqActiveIds}).data(
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

    builder.task().assign({tgUpdScene, tgNeedDelTotal, tgPrereqPhysMod}).data(
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

    static auto const delete_ent_set = wrap_args([] (EntSet_t& set, EntVector_t const& rDelTotal) noexcept
    {
        for (ActiveEnt const ent : rDelTotal)
        {
            set.reset(std::size_t(ent));
        }
    });

    builder.task().assign({tgUpdScene, tgNeedDelTotal}).data(
            "Delete gravity components",
            TopDataIds_t{idGravity, idDelTotal}, delete_ent_set);

    builder.task().assign({tgUpdScene, tgNeedDelTotal}).data(
            "Delete bounds components",
            TopDataIds_t{idBounds, idDelTotal}, delete_ent_set);

    builder.task().assign({tgUpdScene, tgApplyGravity}).data(
            "Apply gravity (-Y 9.81N force) to entities in the rGravity set",
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

            rEntNetForce.y() -= 9.81f * rTPhys.m_physics.m_physDynamic.get(ent).m_totalMass;
        }
    }));

    builder.task().assign({tgUpdScene, tgNeedCollideDirty, tgNeedPhysMod, tgProvideCollide}).data(
            "Update Entities with Newton colliders",
            TopDataIds_t{              idTPhys,               idNwt },
            wrap_args([] (ACtxTestPhys& rTPhys, ACtxNwtWorld& rNwt) noexcept
    {
        ospnewton::SysNewton::update_colliders(
                rTPhys.m_physics, rNwt,
                std::exchange(rTPhys.m_physIn.m_colliderDirty, {}));
    }));

    builder.task().assign({tgUpdScene, tgUpdTime, tgNeedCollide, tgNeedPhysForces, tgFactorTransform}).data(
            "Update Newton world",
            TopDataIds_t{           idBasic,              idTPhys,              idNwt },
            wrap_args([] (ACtxBasic& rBasic, ACtxTestPhys& rTPhys, ACtxNwtWorld& rNwt) noexcept
    {
        constexpr float delta = 1.0f / 60.0f;
        auto const physIn = osp::ArrayView<ACtxPhysInputs>(&rTPhys.m_physIn, 1);
        ospnewton::SysNewton::update_world(
                rTPhys.m_physics, rNwt, delta, physIn,
                rBasic.m_hierarchy,
                rBasic.m_transform, rBasic.m_transformControlled,
                rBasic.m_transformMutable);
    }));

    // Shape spawning
    builder.task().assign({tgUpdScene, tgStartSpawn}).data(
            "Clear Shape Spawning vector to start adding new elements",
            TopDataIds_t{              idSpawner,             idSpawnerEnts },
            wrap_args([] (SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts) noexcept
    {
        rSpawner.clear();
        //rSpawnerEnts.clear();
    }));

    builder.task().assign({tgUpdSpawn, tgNeedSpawn, tgFactorActiveIds, tgProvideSpawnEnt}).data(
            "Create entities for requested shapes to spawn",
            TopDataIds_t{             idActiveIds,              idSpawner,             idSpawnerEnts },
            wrap_args([] (ActiveReg_t& rActiveIds, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts) noexcept
    {
        rSpawnerEnts.resize(rSpawner.size() * 2);
        rActiveIds.create(std::begin(rSpawnerEnts), std::end(rSpawnerEnts));
    }));

    builder.task().assign({tgUpdSpawn, tgNeedSpawnEnt, tgFactorTransform, tgFactorHier}).data(
            "Add hierarchy and transform to spawned shapes",
            TopDataIds_t{           idBasic,              idSpawner,             idSpawnerEnts },
            wrap_args([] (ACtxBasic& rBasic, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts) noexcept
    {
        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i * 2];
            ActiveEnt const root    = rSpawnerEnts[i * 2];
            ActiveEnt const child   = rSpawnerEnts[i * 2 + 1];

            rBasic.m_transform.emplace(root, ACompTransform{osp::Matrix4::translation(spawn.m_position)});
            rBasic.m_transform.emplace(child, ACompTransform{Matrix4::scaling(spawn.m_size)});
            SysHierarchy::add_child(rBasic.m_hierarchy, rBasic.m_hierRoot, root);
            SysHierarchy::add_child(rBasic.m_hierarchy, root, child);
        }
    }));

    builder.task().assign({tgUpdSpawn, tgNeedSpawnEnt, tgFactorVisualDirty, tgFactorMeshDirty, tgFactorMesh}).data(
            "Add mesh and material to spawned shapes",
            TopDataIds_t{             idDrawing,              idSpawner,             idSpawnerEnts,             idNMesh,          idVisual,          idVisualDirty,                idActiveIds},
            wrap_args([] (ACtxDrawing& rDrawing, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts, NamedMeshes& rNMesh, EntSet_t& rMat, EntVector_t& rMatDirty, ActiveReg_t const& rActiveIds ) noexcept
    {
        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i * 2];
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

    builder.task().assign({tgUpdSpawn, tgNeedSpawnEnt, tgFactorTransform, tgFactorHier, tgFactorPhysMod}).data(
            "Add physics to spawned shapes",
            TopDataIds_t{              idSpawner,             idSpawnerEnts,              idTPhys },
            wrap_args([] (SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts, ACtxTestPhys& rTPhys) noexcept
    {
        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i * 2];
            ActiveEnt const root    = rSpawnerEnts[i * 2];
            ActiveEnt const child   = rSpawnerEnts[i * 2 + 1];

            rTPhys.m_physics.m_hasColliders.emplace(root);
            rTPhys.m_physics.m_physBody.emplace(root);
            rTPhys.m_physics.m_physLinearVel.emplace(root);
            rTPhys.m_physics.m_physAngularVel.emplace(root);
            ACompPhysDynamic &rDyn = rTPhys.m_physics.m_physDynamic.emplace(root);
            rDyn.m_totalMass = 1.0f;

            rTPhys.m_physIn.m_setVelocity.emplace_back(root, spawn.m_velocity);

            rTPhys.m_physics.m_shape.emplace(child, spawn.m_shape);
            rTPhys.m_physics.m_solid.emplace(child);
            osp::Vector3 const inertia
                    = osp::phys::collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);
            rTPhys.m_hierBody.m_ownDyn.emplace( child, ACompSubBody{ inertia, spawn.m_mass } );

            rTPhys.m_physIn.m_colliderDirty.push_back(child);
        }
    }));


    builder.task().assign({tgUpdSpawn, tgNeedSpawnEnt, tgFactorGravity}).data(
            "Add gravity to spawned shapes",
            TopDataIds_t{                    idSpawner,                   idSpawnerEnts,          idGravity,                   idActiveIds },
            wrap_args([] (SpawnerVec_t const& rSpawner, EntVector_t const& rSpawnerEnts, EntSet_t& rGravity, ActiveReg_t const& rActiveIds) noexcept
    {
        rGravity.ints().resize(rActiveIds.vec().capacity());

        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            ActiveEnt const root    = rSpawnerEnts[i * 2];

            rGravity.set(std::size_t(root));
        }
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
    rBasic.m_transform.emplace(
            floorRootEnt, ACompTransform{Matrix4::rotationX(-90.0_degf)});

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
    Matrix4 const floorTf = Matrix4::scaling(sc_floorSize)
                          * Matrix4::translation(sc_floorPos);
    add_solid_quick({rActiveIds, rBasic, rTPhys, rNMesh, rDrawing}, floorRootEnt, EShape::Box, floorTf,
                    0, 0.0f);

    // Make floor entity a (non-dynamic) rigid body
    rTPhys.m_physics.m_hasColliders.emplace(floorRootEnt);
    rTPhys.m_physics.m_physBody.emplace(floorRootEnt);

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

    auto const [idActiveApp, idRenderGl, idUserInput] = osp::unpack<3>(magnumIn.m_dataIds);
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
                idDelEnts, idDelTotal, idTPhys, idNMesh, idNwt, idTest,
                idPhong, idPhongDirty, idVisual, idVisualDirty,
                idSpawner, idSpawnerEnts, idGravity, idBounds]
               = osp::unpack<19>(sceneIn.m_dataIds);

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

    auto const [tgUpdRender, tgUpdInputs, tgUsesGL] = osp::unpack<3>(magnumIn.m_tags);

    auto const [tgUpdScene, tgUpdTime, tgUpdSync, tgUpdResync, tgUpdCleanup, tgUpdSpawn,
                tgPrereqActiveIds, tgFactorActiveIds, tgNeedActiveIds, tgUsesActiveIds,
                tgPrereqHier, tgFactorHier, tgUsesHier, tgFinishHier, tgNeedHier,
                tgPrereqTransform, tgFactorTransform, tgNeedTransform,
                tgPrereqMesh, tgFactorMesh, tgNeedMesh,
                tgPrereqTex, tgFactorTex, tgNeedTex,
                tgStartMeshDirty, tgFactorMeshDirty, tgNeedMeshDirty,
                tgStartVisualDirty, tgFactorVisualDirty, tgNeedVisualDirty,
                tgStartCollideDirty, tgFactorCollideDirty, tgNeedCollideDirty,
                tgFactorPhysForces, tgNeedPhysForces,
                tgApplyGravity, tgFactorGravity,
                tgPrereqPhysMod, tgFactorPhysMod, tgNeedPhysMod,
                tgProvideCollide, tgNeedCollide,
                tgStartSpawn, tgFactorSpawn, tgNeedSpawn,
                tgProvideSpawnEnt, tgNeedSpawnEnt,
                tgStartDelEnts, tgFactorDelEnts, tgNeedDelEnts,
                tgProvideDelTotal, tgNeedDelTotal]
               = osp::unpack<52>(sceneIn.m_tags);

    auto const [tgCompileMeshGl,        tgNeedMeshGl,
                tgCompileTexGl,         tgNeedTexGl,
                tgFactorEntTex,         tgNeedEntTex,
                tgFactorEntMesh,        tgNeedEntMesh,
                tgFactorGroupFwd,       tgNeedGroupFwd,
                tgFactorDrawTransform,  tgNeedDrawTransform]
                = osp::unpack<12>(sceneRenderOut.m_tags);

    builder.tag(tgNeedMeshGl)           .depend_on({tgCompileMeshGl});
    builder.tag(tgNeedTexGl)            .depend_on({tgCompileTexGl});
    builder.tag(tgNeedEntTex)           .depend_on({tgFactorEntTex});
    builder.tag(tgNeedEntMesh)          .depend_on({tgFactorEntMesh});
    builder.tag(tgNeedGroupFwd)         .depend_on({tgFactorGroupFwd});
    builder.tag(tgNeedDrawTransform)    .depend_on({tgFactorDrawTransform});


    builder.task().assign({tgUpdInputs}).data(
            TopDataIds_t{           idBasic,                      idCamCtrl,                idCamEnt},
            wrap_args([] (ACtxBasic& rBasic, ACtxCameraController& rCamCtrl, ActiveEnt const camEnt) noexcept
    {
                    float delta = 1.0f/60.0f;
        SysCameraController::update_view(rCamCtrl,
                rBasic.m_transform.get(camEnt), delta);
        SysCameraController::update_move(
                rCamCtrl,
                rBasic.m_transform.get(camEnt),
                delta, true);
    }));

    builder.task().assign({tgUpdSync, tgUsesGL, tgCompileTexGl, tgCompileTexGl}).data(
            TopDataIds_t{                      idDrawingRes,                idResources,          idRenderGl},
            wrap_args([] (ACtxDrawingRes const& rDrawingRes, osp::Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::sync_scene_resources(rDrawingRes, rResources, rRenderGl);
    }));

    builder.task().assign({tgUpdSync, tgNeedTexGl, tgFactorEntTex}).data(
            TopDataIds_t{             idDrawing,                idDrawingRes,                   idScnRender,          idRenderGl},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::assign_textures(rDrawing.m_diffuseTex, rDrawingRes.m_texToRes, rDrawing.m_diffuseDirty, rScnRender.m_diffuseTexId, rRenderGl);
    }));

    builder.task().assign({tgUpdSync, tgNeedTexGl, tgFactorEntMesh, tgNeedMeshDirty}).data(
            TopDataIds_t{             idDrawing,                idDrawingRes,                   idScnRender,          idRenderGl},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::assign_meshes(rDrawing.m_mesh, rDrawingRes.m_meshToRes, rDrawing.m_meshDirty, rScnRender.m_meshId, rRenderGl);
    }));

    builder.task().assign({tgUpdSync, tgNeedVisualDirty, tgFactorGroupFwd}).data(
            TopDataIds_t{                   idVisualDirty,          idVisual,               idGroupFwd,                     idDrawVisual},
            wrap_args([] (EntVector_t const& rDirty, EntSet_t const& rMaterial, RenderGroup& rGroup, ACtxDrawMeshVisualizer& rVisualizer) noexcept
    {
        sync_visualizer(std::cbegin(rDirty), std::cend(rDirty), rMaterial, rGroup.m_entities, rVisualizer);
    }));

    // TODO: phong shader

    builder.task().assign({tgUpdSync, tgNeedVisualDirty, tgNeedHier, tgNeedTransform, tgFactorDrawTransform}).data(
            TopDataIds_t{                 idBasic,                   idVisualDirty,                   idScnRender},
            wrap_args([] (ACtxBasic const& rBasic, EntVector_t const& rVisualDirty, ACtxSceneRenderGL& rScnRender) noexcept
    {
        SysRender::assure_draw_transforms(rBasic.m_hierarchy, rScnRender.m_drawTransform, std::cbegin(rVisualDirty), std::cend(rVisualDirty));
        SysRender::update_draw_transforms(rBasic.m_hierarchy, rBasic.m_transform, rScnRender.m_drawTransform);
    }));

    builder.task().assign({tgUpdRender, tgUsesGL, tgFactorEntTex, tgFactorEntMesh, tgNeedDrawTransform, tgNeedGroupFwd}).data(
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

    // space to throw
    builder.task().assign({tgUpdInputs, tgFactorSpawn}).data(
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
                .m_mass     = 700.0f,
                .m_shape    = EShape::Sphere
            });
            return osp::TopTaskStatus::Success;
        }
        return osp::TopTaskStatus::Success;
    }));
}

} // namespace testapp::physicstest
