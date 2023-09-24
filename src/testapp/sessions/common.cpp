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
#include "common.h"
#include "../scenarios.h"

#include <adera/drawing/CameraController.h>
#include <osp/activescene/basic_fn.h>
#include <osp/core/Resources.h>
#include <osp/core/unpack.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/util/UserInputHandler.h>

using namespace adera;
using namespace osp;
using namespace osp::active;
using namespace osp::draw;

namespace testapp::scenes
{

Session setup_scene(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              application)
{
    OSP_DECLARE_GET_DATA_IDS(application, TESTAPP_DATA_APPLICATION);
    auto const tgApp = application.get_pipelines< PlApplication >();

    osp::Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SCENE);

    top_emplace< float >(topData, idDeltaTimeIn, 1.0f / 60.0f);

    auto const plScn = out.create_pipelines<PlScene>(rBuilder);

    rBuilder.pipeline(plScn.update).parent(tgApp.mainLoop).wait_for_signal(ModifyOrSignal);

    rBuilder.task()
        .name       ("Schedule Scene update")
        .schedules  ({plScn.update(Schedule)})
        .push_to    (out.m_tasks)
        .args       ({                  idMainLoopCtrl})
        .func([] (MainLoopControl const& rMainLoopCtrl) noexcept -> osp::TaskActions
    {
        return rMainLoopCtrl.doUpdate ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });

    return out;
} // setup_scene




Session setup_common_scene(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              application,
        PkgId const                 pkg)
{
    OSP_DECLARE_GET_DATA_IDS(application, TESTAPP_DATA_APPLICATION);

    auto const tgScn    = scene.get_pipelines<PlScene>();
    auto &rResources    = top_get< Resources >      (topData, idResources);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_COMMON_SCENE);
    auto const tgCS = out.create_pipelines<PlCommonScene>(rBuilder);

    out.m_cleanup = tgScn.cleanup;

    /* unused */          top_emplace< ActiveEntVec_t > (topData, idActiveEntDel);
    /* unused */          top_emplace< DrawEntVec_t >   (topData, idDrawEntDel);
    auto &rBasic        = top_emplace< ACtxBasic >      (topData, idBasic);
    auto &rDrawing      = top_emplace< ACtxDrawing >    (topData, idDrawing);
    auto &rDrawingRes   = top_emplace< ACtxDrawingRes > (topData, idDrawingRes);
    auto &rNMesh        = top_emplace< NamedMeshes >    (topData, idNMesh);

    rBuilder.pipeline(tgCS.activeEnt)           .parent(tgScn.update);
    rBuilder.pipeline(tgCS.activeEntResized)    .parent(tgScn.update);
    rBuilder.pipeline(tgCS.activeEntDelete)     .parent(tgScn.update);
    rBuilder.pipeline(tgCS.transform)           .parent(tgScn.update);
    rBuilder.pipeline(tgCS.hierarchy)           .parent(tgScn.update);



    rBuilder.task()
        .name       ("Cancel entity delete tasks stuff if no entities were deleted")
        .run_on     ({tgCS.activeEntDelete(Schedule_)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                      idActiveEntDel })
        .func([] (ACtxBasic& rBasic, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        return rActiveEntDel.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rBuilder.task()
        .name       ("Delete ActiveEnt IDs")
        .run_on     ({tgCS.activeEntDelete(EStgIntr::UseOrRun)})
        .sync_with  ({tgCS.activeEnt(Delete)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                      idActiveEntDel })
        .func([] (ACtxBasic& rBasic, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        for (ActiveEnt const ent : rActiveEntDel)
        {
            if (rBasic.m_activeIds.exists(ent))
            {
                rBasic.m_activeIds.remove(ent);
            }
        }
    });

    rBuilder.task()
        .name       ("Delete basic components")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgCS.transform(Delete)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                      idActiveEntDel })
        .func([] (ACtxBasic& rBasic, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        update_delete_basic(rBasic, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    rBuilder.task()
        .name       ("Clear ActiveEnt delete vector once we're done with it")
        .run_on     ({tgCS.activeEntDelete(Clear)})
        .push_to    (out.m_tasks)
        .args       ({            idActiveEntDel })
        .func([] (ActiveEntVec_t& idActiveEntDel) noexcept
    {
        idActiveEntDel.clear();
    });


    // Clean up tasks

    rBuilder.task()
        .name       ("Clean up resource owners")
        .run_on     ({tgScn.cleanup(Run_)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                idDrawingRes,           idResources})
        .func([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources) noexcept
    {
        SysRender::clear_resource_owners(rDrawingRes, rResources);
    });

    rBuilder.task()
        .name       ("Clean up NamedMeshes mesh and texture owners")
        .run_on     ({tgScn.cleanup(Run_)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,             idNMesh })
        .func([] (ACtxDrawing& rDrawing, NamedMeshes& rNMesh) noexcept
    {
        for ([[maybe_unused]] auto && [_, rOwner] : std::exchange(rNMesh.m_shapeToMesh, {}))
        {
            rDrawing.m_meshRefCounts.ref_release(std::move(rOwner));
        }

        for ([[maybe_unused]] auto && [_, rOwner] : std::exchange(rNMesh.m_namedMeshs, {}))
        {
            rDrawing.m_meshRefCounts.ref_release(std::move(rOwner));
        }
    });

    // Convenient functor to get a reference-counted mesh owner
    auto const quick_add_mesh = SysRender::gen_drawable_mesh_adder(rDrawing, rDrawingRes, rResources, pkg);

    // Acquire mesh resources from Package
    rNMesh.m_shapeToMesh.emplace(EShape::Box,       quick_add_mesh("cube"));
    rNMesh.m_shapeToMesh.emplace(EShape::Cylinder,  quick_add_mesh("cylinder"));
    rNMesh.m_shapeToMesh.emplace(EShape::Sphere,    quick_add_mesh("sphere"));
    rNMesh.m_namedMeshs.emplace("floor", quick_add_mesh("grid64solid"));

    return out;
} // setup_common_scene




Session setup_window_app(
        TopTaskBuilder&                 rBuilder,
        ArrayView<entt::any> const      topData,
        Session const&                  application)
{
    OSP_DECLARE_GET_DATA_IDS(application, TESTAPP_DATA_APPLICATION);
    auto const tgApp = application   .get_pipelines< PlApplication >();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_WINDOW_APP);
    auto const tgWin = out.create_pipelines<PlWindowApp>(rBuilder);

    rBuilder.pipeline(tgWin.inputs).parent(tgApp.mainLoop).wait_for_signal(ModifyOrSignal);
    rBuilder.pipeline(tgWin.sync)  .parent(tgApp.mainLoop).wait_for_signal(ModifyOrSignal);
    rBuilder.pipeline(tgWin.resync).parent(tgApp.mainLoop).wait_for_signal(ModifyOrSignal);

    auto &rUserInput = top_emplace<osp::input::UserInputHandler>(topData, idUserInput, 12);

    out.m_cleanup = tgWin.cleanup;

    rBuilder.task()
        .name       ("Schedule GL Resync")
        .schedules  ({tgWin.resync(Schedule)})
        .push_to    (out.m_tasks)
        .args       ({                  idMainLoopCtrl})
        .func([] (MainLoopControl const& rMainLoopCtrl) noexcept -> osp::TaskActions
    {
        return rMainLoopCtrl.doResync ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });

    return out;
} // setup_window_app




Session setup_scene_renderer(
        TopTaskBuilder&                 rBuilder,
        ArrayView<entt::any> const      topData,
        Session const&                  application,
        Session const&                  windowApp,
        Session const&                  commonScene)
{
    OSP_DECLARE_GET_DATA_IDS(windowApp,   TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(commonScene, TESTAPP_DATA_COMMON_SCENE);
    auto const tgApp    = application   .get_pipelines< PlApplication >();
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgCS     = commonScene   .get_pipelines< PlCommonScene >();


    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SCENE_RENDERER);
    auto const tgScnRdr = out.create_pipelines<PlSceneRenderer>(rBuilder);

    rBuilder.pipeline(tgScnRdr.render).parent(tgApp.mainLoop).wait_for_signal(ModifyOrSignal);

    rBuilder.pipeline(tgScnRdr.drawEnt)         .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.drawEntResized)  .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.drawEntDelete)   .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.entMesh)         .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.entTexture)      .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.entTextureDirty) .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.entMeshDirty)    .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.drawTransforms)  .parent(tgScnRdr.render);
    rBuilder.pipeline(tgScnRdr.material)        .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.materialDirty)   .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.group)           .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.groupEnts)       .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.entMesh)         .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.entTexture)      .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.mesh)            .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.texture)         .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.meshResDirty)    .parent(tgWin.sync);
    rBuilder.pipeline(tgScnRdr.textureResDirty) .parent(tgWin.sync);

    auto &rScnRender = osp::top_emplace<ACtxSceneRender>(topData, idScnRender);

    rBuilder.task()
        .name       ("Resize ACtxSceneRender containers to fit all DrawEnts")
        .run_on     ({tgScnRdr.drawEntResized(Run)})
        .sync_with  ({tgScnRdr.entMesh(New), tgScnRdr.entTexture(New)})
        .push_to    (out.m_tasks)
        .args       ({idScnRender})
        .func       ([] (ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.resize_draw();
    });

    rBuilder.task()
        .name       ("Resize ACtxSceneRender to fit ActiveEnts")
        .run_on     ({tgCS.activeEntResized(Run)})
        .push_to    (out.m_tasks)
        .args       ({               idBasic,                  idScnRender})
        .func([]    (ACtxBasic const &rBasic,  ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.resize_active(rBasic.m_activeIds.capacity());
    });

    // Duplicate task needed for resync to account for existing ActiveEnts when the renderer opens,
    // as activeEntResized doesn't run during resync
    rBuilder.task()
        .name       ("Resync ACtxSceneRender to fit ActiveEnts")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgCS.activeEntResized(Run)})
        .push_to    (out.m_tasks)
        .args       ({               idBasic,                  idScnRender})
        .func([]    (ACtxBasic const &rBasic,  ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.resize_active(rBasic.m_activeIds.capacity());
    });

    rBuilder.task()
        .name       ("Schedule Assign GL textures")
        .schedules  ({tgScnRdr.entTextureDirty(Schedule_)})
        .sync_with  ({tgScnRdr.texture(Ready), tgScnRdr.entTexture(Ready)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender })
        .func([] (ACtxSceneRender& rScnRender) noexcept -> TaskActions
    {
        return rScnRender.m_diffuseDirty.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rBuilder.task()
        .name       ("Schedule Assign GL meshes")
        .schedules  ({tgScnRdr.entMeshDirty(Schedule_)})
        .sync_with  ({tgScnRdr.mesh(Ready), tgScnRdr.entMesh(Ready)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender })
        .func([] (ACtxSceneRender& rScnRender) noexcept -> TaskActions
    {
        return rScnRender.m_meshDirty.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rBuilder.task()
        .name       ("Delete DrawEntity of deleted ActiveEnts")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgScnRdr.drawEntDelete(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,                      idActiveEntDel,              idDrawEntDel })
        .func([] (ACtxSceneRender& rScnRender, ActiveEntVec_t const& rActiveEntDel, DrawEntVec_t& rDrawEntDel) noexcept
    {
        for (ActiveEnt const ent : rActiveEntDel)
        {
            if (rScnRender.m_activeToDraw.size() < std::size_t(ent))
            {
                continue;
            }
            DrawEnt const drawEnt = std::exchange(rScnRender.m_activeToDraw[ent], lgrn::id_null<DrawEnt>());
            if (drawEnt != lgrn::id_null<DrawEnt>())
            {
                rDrawEntDel.push_back(drawEnt);
            }
        }
    });

    rBuilder.task()
        .name       ("Delete drawing components")
        .run_on     ({tgScnRdr.drawEntDelete(UseOrRun)})
        .sync_with  ({tgScnRdr.entTexture(Delete), tgScnRdr.entMesh(Delete)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                 idScnRender,                    idDrawEntDel })
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, DrawEntVec_t const& rDrawEntDel) noexcept
    {
        SysRender::update_delete_drawing(rScnRender, rDrawing, rDrawEntDel.cbegin(), rDrawEntDel.cend());
    });

    rBuilder.task()
        .name       ("Delete DrawEntity IDs")
        .run_on     ({tgScnRdr.drawEntDelete(UseOrRun)})
        .sync_with  ({tgScnRdr.drawEnt(Delete)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,                    idDrawEntDel })
        .func([] (ACtxSceneRender& rScnRender, DrawEntVec_t const& rDrawEntDel) noexcept
    {
        for (DrawEnt const drawEnt : rDrawEntDel)
        {
            if (rScnRender.m_drawIds.exists(drawEnt))
            {
                rScnRender.m_drawIds.remove(drawEnt);
            }
        }
    });

    rBuilder.task()
        .name       ("Delete DrawEnt from materials")
        .run_on     ({tgScnRdr.drawEntDelete(UseOrRun)})
        .sync_with  ({tgScnRdr.material(Delete)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,                    idDrawEntDel })
        .func([] (ACtxSceneRender& rScnRender, DrawEntVec_t const& rDrawEntDel) noexcept
    {
        for (DrawEnt const ent : rDrawEntDel)
        {
            for (Material &rMat : rScnRender.m_materials)
            {
                if (std::size_t(ent) < rMat.m_ents.size())
                {
                    rMat.m_ents.reset(std::size_t(ent));
                }
            }
        }
    });

    rBuilder.task()
        .name       ("Clear DrawEnt delete vector once we're done with it")
        .run_on     ({tgScnRdr.drawEntDelete(Clear)})
        .push_to    (out.m_tasks)
        .args       ({         idDrawEntDel })
        .func([] (DrawEntVec_t& rDrawEntDel) noexcept
    {
        rDrawEntDel.clear();
    });

    rBuilder.task()
        .name       ("Clear dirty DrawEnt's textures once we're done with it")
        .run_on     ({tgScnRdr.entMeshDirty(Clear)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender})
        .func([] (ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.m_meshDirty.clear();
    });

    rBuilder.task()
        .name       ("Clear dirty DrawEnt's textures once we're done with it")
        .run_on     ({tgScnRdr.entTextureDirty(Clear)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender})
        .func([] (ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.m_diffuseDirty.clear();
    });

    rBuilder.task()
        .name       ("Clean up scene owners")
        .run_on     ({tgWin.cleanup(Run_)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                 idScnRender})
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender) noexcept
    {
        SysRender::clear_owners(rScnRender, rDrawing);
    });

    return out;
} // setup_scene_renderer

} // namespace testapp::scenes
