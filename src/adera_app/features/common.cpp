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

#include "../feature_interfaces.h"
#include "../application.h"

#include <adera/drawing/CameraController.h>
#include <osp/activescene/basic_fn.h>
#include <osp/core/Resources.h>
#include <osp/core/unpack.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/util/UserInputHandler.h>

using namespace adera;
using namespace ftr_inter;
using namespace ftr_inter::stages;
using namespace osp;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::fw;

namespace adera
{

FeatureDef const ftrMain = feature_def("Main", [] (FeatureBuilder& rFB, Implement<FIMainApp> mainApp, entt::any pkg)
{
    rFB.data_emplace< AppContexts >             (mainApp.di.appContexts);
    rFB.data_emplace< MainLoopControl >         (mainApp.di.mainLoopCtrl);
    rFB.data_emplace< osp::Resources >          (mainApp.di.resources);
    rFB.data_emplace< FrameworkModify >         (mainApp.di.frameworkModify);

    rFB.pipeline(mainApp.pl.mainLoop).loops(true).wait_for_signal(ModifyOrSignal);

    rFB.task()
        .name       ("Schedule Main Loop")
        .schedules  ({mainApp.pl.mainLoop(Schedule)})
        .args       ({         mainApp.di.mainLoopCtrl})
        .func([] (MainLoopControl const& rMainLoopCtrl) noexcept -> osp::TaskActions
    {
        return rMainLoopCtrl.doUpdate ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });
});


FeatureDef const ftrScene = feature_def("Scene", [] (
        FeatureBuilder& rFB, Implement<FIScene> scene, DependOn<FIMainApp> mainApp)
{
    rFB.data_emplace<float>(scene.di.deltaTimeIn, 1.0f / 60.0f);
    rFB.pipeline(scene.pl.update).parent(mainApp.pl.mainLoop).wait_for_signal(ModifyOrSignal);

    rFB.task()
        .name       ("Schedule Scene update")
        .schedules  ({scene.pl.update(Schedule)})
        .args       ({                mainApp.di.mainLoopCtrl})
        .func       ([] (MainLoopControl const& rMainLoopCtrl) noexcept -> osp::TaskActions
    {
        return rMainLoopCtrl.doUpdate ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });
}); // ftrScene


FeatureDef const ftrCommonScene = feature_def("CommonScene", [] (
        FeatureBuilder& rFB, Implement<FICommonScene> commonScene, DependOn<FIScene> scene,
        DependOn<FIMainApp> mainApp, entt::any data)
{

    // TODO: out.m_cleanup = scene.pl.cleanup;

    /* not used here */   rFB.data_emplace< ActiveEntVec_t >(commonScene.di.activeEntDel);
    /* not used here */   rFB.data_emplace< DrawEntVec_t >  (commonScene.di.drawEntDel);
    auto &rBasic        = rFB.data_emplace< ACtxBasic >     (commonScene.di.basic);
    auto &rDrawing      = rFB.data_emplace< ACtxDrawing >   (commonScene.di.drawing);
    auto &rDrawingRes   = rFB.data_emplace< ACtxDrawingRes >(commonScene.di.drawingRes);
    auto &rNamedMeshes  = rFB.data_emplace< NamedMeshes >   (commonScene.di.namedMeshes);

    rFB.pipeline(commonScene.pl.activeEnt)           .parent(scene.pl.update);
    rFB.pipeline(commonScene.pl.activeEntResized)    .parent(scene.pl.update);
    rFB.pipeline(commonScene.pl.activeEntDelete)     .parent(scene.pl.update);
    rFB.pipeline(commonScene.pl.transform)           .parent(scene.pl.update);
    rFB.pipeline(commonScene.pl.hierarchy)           .parent(scene.pl.update);

    rFB.task()
        .name       ("Cancel entity delete tasks stuff if no entities were deleted")
        .run_on     ({commonScene.pl.activeEntDelete(Schedule_)})
        .args       ({commonScene.di.basic,  commonScene.di.activeEntDel })
        .func       ([] (ACtxBasic &rBasic, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        return rActiveEntDel.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rFB.task()
        .name       ("Delete ActiveEnt IDs")
        .run_on     ({commonScene.pl.activeEntDelete(EStgIntr::UseOrRun)})
        .sync_with  ({commonScene.pl.activeEnt(Delete)})
        .args       ({commonScene.di.basic,         commonScene.di.activeEntDel })
        .func       ([] (ACtxBasic &rBasic, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        for (ActiveEnt const ent : rActiveEntDel)
        {
            if (rBasic.m_activeIds.exists(ent))
            {
                rBasic.m_activeIds.remove(ent);
            }
        }
    });

    rFB.task()
        .name       ("Delete basic components")
        .run_on     ({commonScene.pl.activeEntDelete(UseOrRun)})
        .sync_with  ({commonScene.pl.transform(Delete)})
        .args       ({commonScene.di.basic,         commonScene.di.activeEntDel })
        .func([]    (    ACtxBasic &rBasic, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        update_delete_basic(rBasic, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    rFB.task()
        .name       ("Clear ActiveEnt delete vector once we're done with it")
        .run_on     ({commonScene.pl.activeEntDelete(Clear)})
        .args       ({ commonScene.di.activeEntDel })
        .func([]    (ActiveEntVec_t &rActiveEntDel) noexcept
    {
        rActiveEntDel.clear();
    });

    // Clean up tasks

    rFB.task()
        .name       ("Clean up resource owners")
        .run_on     ({scene.pl.cleanup(Run_)})
        .args       ({  commonScene.di.drawing,   commonScene.di.drawingRes,  mainApp.di.resources})
        .func       ([] (ACtxDrawing &rDrawing, ACtxDrawingRes &rDrawingRes, Resources &rResources) noexcept
    {
        SysRender::clear_resource_owners(rDrawingRes, rResources);
    });

    rFB.task()
        .name       ("Clean up NamedMeshes mesh and texture owners")
        .run_on     ({scene.pl.cleanup(Run_)})
        .args       ({        commonScene.di.drawing,             commonScene.di.namedMeshes })
        .func([] (ACtxDrawing &rDrawing, NamedMeshes &rNMesh) noexcept
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

    auto &rResources    = rFB.data_get<Resources>(mainApp.di.resources);


    // Convenient functor to get a reference-counted mesh owner
    auto const quick_add_mesh = SysRender::gen_drawable_mesh_adder(rDrawing, rDrawingRes, rResources, entt::any_cast<PkgId>(data));

    // Acquire mesh resources from Package
    rNamedMeshes.m_shapeToMesh.emplace(EShape::Box,       quick_add_mesh("cube"));
    rNamedMeshes.m_shapeToMesh.emplace(EShape::Cylinder,  quick_add_mesh("cylinder"));
    rNamedMeshes.m_shapeToMesh.emplace(EShape::Sphere,    quick_add_mesh("sphere"));
    rNamedMeshes.m_namedMeshs.emplace("floor", quick_add_mesh("grid64solid"));

}); // ftrCommonScene


FeatureDef const ftrWindowApp = feature_def("WindowApp", [] (
        FeatureBuilder& rFB, Implement<FIWindowApp> windowApp, DependOn<FIMainApp> mainApp)
{
    rFB.pipeline(windowApp.pl.inputs).parent(mainApp.pl.mainLoop);//.wait_for_signal(ModifyOrSignal);
    rFB.pipeline(windowApp.pl.sync)  .parent(mainApp.pl.mainLoop);//.wait_for_signal(ModifyOrSignal);
    rFB.pipeline(windowApp.pl.resync).parent(mainApp.pl.mainLoop);//.wait_for_signal(ModifyOrSignal);

    auto &rUserInput     = rFB.data_emplace<osp::input::UserInputHandler>(windowApp.di.userInput, 12);
    auto &rWindowAppCtrl = rFB.data_emplace<WindowAppLoopControl>        (windowApp.di.windowAppLoopCtrl);

    // TODO: out.m_cleanup = windowApp.pl.cleanup;

    rFB.task()
        .name       ("Schedule Renderer Sync")
        .schedules  ({windowApp.pl.sync(Schedule)})
        .args       ({                   windowApp.di.windowAppLoopCtrl})
        .func       ([] (WindowAppLoopControl const& rWindowAppLoopCtrl) noexcept -> osp::TaskActions
    {
        return rWindowAppLoopCtrl.doSync ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });

    rFB.task()
        .name       ("Schedule Renderer Resync")
        .schedules  ({windowApp.pl.resync(Schedule)})
        .args       ({                   windowApp.di.windowAppLoopCtrl})
        .func       ([] (WindowAppLoopControl const& rWindowAppLoopCtrl) noexcept -> osp::TaskActions
    {
        return rWindowAppLoopCtrl.doResync ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });

}); // ftrWindowApp


#if 0

Session setup_scene_renderer(
        TopTaskBuilder&                 rFB,
        ArrayView<entt::any> const      topData,
        Session const&                  application,
        Session const&                  windowApp,
        Session const&                  commonScene)
{
    OSP_DECLARE_GET_DATA_IDS(windowApp,   TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(commonScene, TESTAPP_DATA_COMMON_SCENE);
    auto const mainApp.pl    = application   .get_pipelines< PlApplication >();
    auto const windowApp.pl    = windowApp     .get_pipelines< PlWindowApp >();
    auto const commonScene.pl     = commonScene   .get_pipelines< PlCommonScene >();


    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SCENE_RENDERER);
    auto const scene.plRdr = out.create_pipelines<PlSceneRenderer>(rFB);

    rFB.pipeline(scene.plRdr.render).parent(mainApp.pl.mainLoop).wait_for_signal(ModifyOrSignal);

    rFB.pipeline(scene.plRdr.drawEnt)         .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.drawEntResized)  .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.drawEntDelete)   .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.entMesh)         .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.entTexture)      .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.entTextureDirty) .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.entMeshDirty)    .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.drawTransforms)  .parent(scene.plRdr.render);
    rFB.pipeline(scene.plRdr.material)        .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.materialDirty)   .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.group)           .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.groupEnts)       .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.entMesh)         .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.entTexture)      .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.mesh)            .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.texture)         .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.meshResDirty)    .parent(windowApp.pl.sync);
    rFB.pipeline(scene.plRdr.textureResDirty) .parent(windowApp.pl.sync);

    auto &rScnRender = osp::rFB.data_emplace<ACtxSceneRender>(topData, idScnRender);
    /* unused */       osp::rFB.data_emplace<DrawTfObservers>(topData, idDrawTfObservers);

    rFB.task()
        .name       ("Resize ACtxSceneRender containers to fit all DrawEnts")
        .run_on     ({scene.plRdr.drawEntResized(Run)})
        .sync_with  ({scene.plRdr.entMesh(New), scene.plRdr.entTexture(New)})
        .push_to    (out.m_tasks)
        .args       ({idScnRender})
        .func       ([] (ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.resize_draw();
    });

    rFB.task()
        .name       ("Resize ACtxSceneRender to fit ActiveEnts")
        .run_on     ({commonScene.pl.activeEntResized(Run)})
        .push_to    (out.m_tasks)
        .args       ({               commonScene.di.basic,                  idScnRender})
        .func([]    (ACtxBasic const &rBasic,  ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.resize_active(rBasic.m_activeIds.capacity());
    });

    // Duplicate task needed for resync to account for existing ActiveEnts when the renderer opens,
    // as activeEntResized doesn't run during resync
    rFB.task()
        .name       ("Resync ACtxSceneRender to fit ActiveEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({commonScene.pl.activeEntResized(Run)})
        .push_to    (out.m_tasks)
        .args       ({               commonScene.di.basic,                  idScnRender})
        .func([]    (ACtxBasic const &rBasic,  ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.resize_active(rBasic.m_activeIds.capacity());
    });

    rFB.task()
        .name       ("Schedule Assign GL textures")
        .schedules  ({scene.plRdr.entTextureDirty(Schedule_)})
        .sync_with  ({scene.plRdr.texture(Ready), scene.plRdr.entTexture(Ready)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender })
        .func([] (ACtxSceneRender& rScnRender) noexcept -> TaskActions
    {
        return rScnRender.m_diffuseDirty.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rFB.task()
        .name       ("Schedule Assign GL meshes")
        .schedules  ({scene.plRdr.entMeshDirty(Schedule_)})
        .sync_with  ({scene.plRdr.mesh(Ready), scene.plRdr.entMesh(Ready)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender })
        .func([] (ACtxSceneRender& rScnRender) noexcept -> TaskActions
    {
        return rScnRender.m_meshDirty.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rFB.task()
        .name       ("Calculate draw transforms")
        .run_on     ({scene.plRdr.render(Run)})
        .sync_with  ({commonScene.pl.hierarchy(Ready), commonScene.pl.transform(Ready), commonScene.pl.activeEnt(Ready), scene.plRdr.drawTransforms(Modify_), scene.plRdr.drawEnt(Ready), scene.plRdr.drawEntResized(Done), commonScene.pl.activeEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({            commonScene.di.basic,                   commonScene.di.drawing,                 idScnRender,                 idDrawTfObservers })
        .func([] (ACtxBasic const& rBasic, ACtxDrawing const& rDrawing, ACtxSceneRender& rScnRender, DrawTfObservers &rDrawTfObservers) noexcept
    {
        auto rootChildren = SysSceneGraph::children(rBasic.m_scnGraph);
        SysRender::update_draw_transforms(
                {
                    .scnGraph     = rBasic    .m_scnGraph,
                    .transforms   = rBasic    .m_transform,
                    .activeToDraw = rScnRender.m_activeToDraw,
                    .needDrawTf   = rScnRender.m_needDrawTf,
                    .rDrawTf      = rScnRender.m_drawTransform
                },
                rootChildren.begin(),
                rootChildren.end(),
                [&rDrawTfObservers, &rScnRender] (Matrix4 const& transform, active::ActiveEnt ent, int depth)
        {
            auto const enableInt  = std::array{rScnRender.drawTfObserverEnable[ent]};
            auto const enableBits = lgrn::bit_view(enableInt);

            for (std::size_t idx : enableBits.ones())
            {
                DrawTfObservers::Observer const &rObserver = rDrawTfObservers.observers[idx];
                rObserver.func(rScnRender, transform, ent, depth, rObserver.data);
            }
        });
    });

    rFB.task()
        .name       ("Delete DrawEntity of deleted ActiveEnts")
        .run_on     ({commonScene.di.activeEntDel(UseOrRun)})
        .sync_with  ({scene.plRdr.drawEntDelete(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,                      commonScene.di.activeEntDel,              idDrawEntDel })
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

    rFB.task()
        .name       ("Delete drawing components")
        .run_on     ({scene.plRdr.drawEntDelete(UseOrRun)})
        .sync_with  ({scene.plRdr.entTexture(Delete), scene.plRdr.entMesh(Delete)})
        .push_to    (out.m_tasks)
        .args       ({        commonScene.di.drawing,                 idScnRender,                    idDrawEntDel })
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, DrawEntVec_t const& rDrawEntDel) noexcept
    {
        SysRender::update_delete_drawing(rScnRender, rDrawing, rDrawEntDel.cbegin(), rDrawEntDel.cend());
    });

    rFB.task()
        .name       ("Delete DrawEntity IDs")
        .run_on     ({scene.plRdr.drawEntDelete(UseOrRun)})
        .sync_with  ({scene.plRdr.drawEnt(Delete)})
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

    rFB.task()
        .name       ("Delete DrawEnt from materials")
        .run_on     ({scene.plRdr.drawEntDelete(UseOrRun)})
        .sync_with  ({scene.plRdr.material(Delete)})
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
                    rMat.m_ents.erase(ent);
                }
            }
        }
    });

    rFB.task()
        .name       ("Clear DrawEnt delete vector once we're done with it")
        .run_on     ({scene.plRdr.drawEntDelete(Clear)})
        .push_to    (out.m_tasks)
        .args       ({         idDrawEntDel })
        .func([] (DrawEntVec_t& rDrawEntDel) noexcept
    {
        rDrawEntDel.clear();
    });

    rFB.task()
        .name       ("Clear dirty DrawEnt's textures once we're done with it")
        .run_on     ({scene.plRdr.entMeshDirty(Clear)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender})
        .func([] (ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.m_meshDirty.clear();
    });

    rFB.task()
        .name       ("Clear dirty DrawEnt's textures once we're done with it")
        .run_on     ({scene.plRdr.entTextureDirty(Clear)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender})
        .func([] (ACtxSceneRender& rScnRender) noexcept
    {
        rScnRender.m_diffuseDirty.clear();
    });

    rFB.task()
        .name       ("Clear dirty materials once we're done with it")
        .run_on     ({scene.plRdr.materialDirty(Clear)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender})
        .func([] (ACtxSceneRender& rScnRender) noexcept
    {
        for (Material &rMat : rScnRender.m_materials)
        {
            rMat.m_dirty.clear();
        }
    });

    rFB.task()
        .name       ("Clean up scene owners")
        .run_on     ({windowApp.pl.cleanup(Run_)})
        .push_to    (out.m_tasks)
        .args       ({        commonScene.di.drawing,                 idScnRender})
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender) noexcept
    {
        SysRender::clear_owners(rScnRender, rDrawing);
    });

    return out;
} // setup_scene_renderer

#endif

} // namespace adera
