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

#include "common.h"

#include "../feature_interfaces.h"
#include "../application.h"

#include <osp/activescene/basic_fn.h>
#include <osp/core/Resources.h>
#include <osp/core/unpack.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/util/UserInputHandler.h>

using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp;

namespace adera
{

FeatureDef const ftrMain = feature_def("Main", [] (FeatureBuilder &rFB, Implement<FIMainApp> mainApp)
{
    rFB.data_emplace< AppContexts >             (mainApp.di.appContexts, AppContexts{.main = rFB.ctx});
    rFB.data_emplace< MainLoopControl >         (mainApp.di.mainLoopCtrl);
    rFB.data_emplace< osp::Resources >          (mainApp.di.resources);
    rFB.data_emplace< FrameworkModify >         (mainApp.di.frameworkModify);

    rFB.pipeline(mainApp.pl.mainLoop).loops(true).wait_for_signal(ModifyOrSignal);
    rFB.pipeline(mainApp.pl.stupidWorkaround).parent(mainApp.pl.mainLoop);

    rFB.task()
        .name       ("Schedule Main Loop")
        .schedules  ({mainApp.pl.mainLoop(Schedule)})
        .args       ({         mainApp.di.mainLoopCtrl})
        .func([] (MainLoopControl const &rMainLoopCtrl) noexcept -> osp::TaskActions
    {
        return rMainLoopCtrl.doUpdate ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });
});


FeatureDef const ftrScene = feature_def("Scene", [] (
        FeatureBuilder          &rFB,
        Implement<FIScene>      scn,
        DependOn<FIMainApp>     mainApp)
{
    rFB.data_emplace<float>(scn.di.deltaTimeIn, 0.0f);
    rFB.data_emplace<SceneLoopControl>(scn.di.loopControl);
    rFB.pipeline(scn.pl.loopControl).parent(mainApp.pl.mainLoop);
    rFB.pipeline(scn.pl.update)     .parent(mainApp.pl.mainLoop);

    rFB.task()
        .name       ("Schedule Scene update")
        .schedules  ({scn.pl.update(Schedule)})
        .sync_with   ({scn.pl.loopControl(Ready)})
        .args       ({                     scn.di.loopControl})
        .func       ([] (SceneLoopControl const &rLoopControl) noexcept -> osp::TaskActions
    {
        return rLoopControl.doSceneUpdate ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });
}); // ftrScene


FeatureDef const ftrCommonScene = feature_def("CommonScene", [] (
        FeatureBuilder              &rFB,
        Implement<FICommonScene>    comScn,
        Implement<FICleanupContext> cleanup,
        DependOn<FIScene>           scn,
        DependOn<FIMainApp>         mainApp,
        entt::any                   userData)
{
    /* not used here */   rFB.data_emplace< ActiveEntVec_t >(comScn.di.activeEntDel);
    /* not used here */   rFB.data_emplace< DrawEntVec_t >  (comScn.di.drawEntDel);
    auto &rBasic        = rFB.data_emplace< ACtxBasic >     (comScn.di.basic);
    auto &rDrawing      = rFB.data_emplace< ACtxDrawing >   (comScn.di.drawing);
    auto &rDrawingRes   = rFB.data_emplace< ACtxDrawingRes >(comScn.di.drawingRes);
    auto &rNamedMeshes  = rFB.data_emplace< NamedMeshes >   (comScn.di.namedMeshes);

    rFB.pipeline(comScn.pl.activeEnt)           .parent(scn.pl.update);
    rFB.pipeline(comScn.pl.activeEntResized)    .parent(scn.pl.update);
    rFB.pipeline(comScn.pl.activeEntDelete)     .parent(scn.pl.update);
    rFB.pipeline(comScn.pl.transform)           .parent(scn.pl.update);
    rFB.pipeline(comScn.pl.hierarchy)           .parent(scn.pl.update);
    rFB.pipeline(cleanup.pl.cleanupWorkaround).parent(cleanup.pl.cleanup);

    rFB.task()
        .name       ("Cancel entity delete tasks stuff if no entities were deleted")
        .run_on     ({comScn.pl.activeEntDelete(Schedule_)})
        .args       ({     comScn.di.basic,  comScn.di.activeEntDel })
        .func       ([] (ACtxBasic &rBasic, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        return rActiveEntDel.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rFB.task()
        .name       ("Delete ActiveEnt IDs")
        .run_on     ({comScn.pl.activeEntDelete(EStgIntr::UseOrRun)})
        .sync_with  ({comScn.pl.activeEnt(Delete)})
        .args       ({     comScn.di.basic,              comScn.di.activeEntDel })
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
        .run_on     ({comScn.pl.activeEntDelete(UseOrRun)})
        .sync_with  ({comScn.pl.transform(Delete)})
        .args       ({     comScn.di.basic,              comScn.di.activeEntDel })
        .func       ([] (ACtxBasic &rBasic, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        update_delete_basic(rBasic, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    rFB.task()
        .name       ("Clear ActiveEnt delete vector once we're done with it")
        .run_on     ({comScn.pl.activeEntDelete(Clear)})
        .args       ({      comScn.di.activeEntDel })
        .func([]    (ActiveEntVec_t &rActiveEntDel) noexcept
    {
        rActiveEntDel.clear();
    });

    // Clean up tasks

    rFB.task()
        .name       ("Clean up resource owners")
        .run_on     ({cleanup.pl.cleanup(Run_)})
        .args       ({       comScn.di.drawing,        comScn.di.drawingRes,  mainApp.di.resources})
        .func       ([] (ACtxDrawing &rDrawing, ACtxDrawingRes &rDrawingRes, Resources &rResources) noexcept
    {
        SysRender::clear_resource_owners(rDrawingRes, rResources);
    });

    rFB.task()
        .name       ("Clean up NamedMeshes mesh and texture owners")
        .run_on     ({cleanup.pl.cleanup(Run_)})
        .args       ({       comScn.di.drawing,     comScn.di.namedMeshes })
        .func       ([] (ACtxDrawing &rDrawing, NamedMeshes &rNamedMeshes) noexcept
    {
        for ([[maybe_unused]] auto &&[_, rOwner] : std::exchange(rNamedMeshes.m_shapeToMesh, {}))
        {
            rDrawing.m_meshRefCounts.ref_release(std::move(rOwner));
        }

        for ([[maybe_unused]] auto &&[_, rOwner] : std::exchange(rNamedMeshes.m_namedMeshs, {}))
        {
            rDrawing.m_meshRefCounts.ref_release(std::move(rOwner));
        }
    });

    auto &rResources = rFB.data_get<Resources>(mainApp.di.resources);

    // Convenient functor to get a reference-counted mesh owner
    auto const quick_add_mesh = SysRender::gen_drawable_mesh_adder(rDrawing, rDrawingRes, rResources, entt::any_cast<PkgId>(userData));

    // Acquire mesh resources from Package
    rNamedMeshes.m_shapeToMesh.emplace(EShape::Box,       quick_add_mesh("cube"));
    rNamedMeshes.m_shapeToMesh.emplace(EShape::Cylinder,  quick_add_mesh("cylinder"));
    rNamedMeshes.m_shapeToMesh.emplace(EShape::Sphere,    quick_add_mesh("sphere"));
    rNamedMeshes.m_namedMeshs.emplace("floor", quick_add_mesh("grid64solid"));

}); // ftrCommonScene


FeatureDef const ftrWindowApp = feature_def("WindowApp", [] (
        FeatureBuilder              &rFB,
        Implement<FIWindowApp>      windowApp,
        Implement<FICleanupContext> cleanup,
        DependOn<FIMainApp>         mainApp)
{
    rFB.pipeline(windowApp.pl.inputs).parent(mainApp.pl.mainLoop).wait_for_signal(ModifyOrSignal);
    rFB.pipeline(windowApp.pl.sync)  .parent(mainApp.pl.mainLoop).wait_for_signal(ModifyOrSignal);
    rFB.pipeline(windowApp.pl.resync).parent(mainApp.pl.mainLoop).wait_for_signal(ModifyOrSignal);
    rFB.pipeline(cleanup.pl.cleanupWorkaround).parent(cleanup.pl.cleanup);

    auto &rUserInput     = rFB.data_emplace<osp::input::UserInputHandler>(windowApp.di.userInput, 12);
    auto &rWindowAppCtrl = rFB.data_emplace<WindowAppLoopControl>        (windowApp.di.windowAppLoopCtrl);

    rFB.task()
        .name       ("Schedule Renderer Sync")
        .schedules  ({windowApp.pl.sync(Schedule)})
        .args       ({                   windowApp.di.windowAppLoopCtrl})
        .func       ([] (WindowAppLoopControl const &rWindowAppLoopCtrl) noexcept -> osp::TaskActions
    {
        return rWindowAppLoopCtrl.doSync ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });

    rFB.task()
        .name       ("Schedule Renderer Resync")
        .schedules  ({windowApp.pl.resync(Schedule)})
        .args       ({                   windowApp.di.windowAppLoopCtrl})
        .func       ([] (WindowAppLoopControl const &rWindowAppLoopCtrl) noexcept -> osp::TaskActions
    {
        return rWindowAppLoopCtrl.doResync ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });

}); // ftrWindowApp


FeatureDef const ftrSceneRenderer = feature_def("SceneRenderer", [] (
        FeatureBuilder              &rFB,
        Implement<FISceneRenderer>  scnRender,
        Implement<FICleanupContext> cleanup,
        DependOn<FICommonScene>     comScn,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIWindowApp>       windowApp)
{
    rFB.pipeline(scnRender.pl.render).parent(mainApp.pl.mainLoop);

    rFB.pipeline(scnRender.pl.drawEnt)         .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.drawEntResized)  .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.drawEntDelete)   .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.entMesh)         .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.entTexture)      .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.entTextureDirty) .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.entMeshDirty)    .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.drawTransforms)  .parent(scnRender.pl.render);
    rFB.pipeline(scnRender.pl.material)        .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.materialDirty)   .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.group)           .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.groupEnts)       .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.entMesh)         .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.entTexture)      .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.mesh)            .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.texture)         .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.meshResDirty)    .parent(windowApp.pl.sync);
    rFB.pipeline(scnRender.pl.textureResDirty) .parent(windowApp.pl.sync);

    auto &rScnRender = rFB.data_emplace<ACtxSceneRender>(scnRender.di.scnRender);
    /* unused */       rFB.data_emplace<DrawTfObservers>(scnRender.di.drawTfObservers);

    // TODO: format after framework changes

    rFB.task()
        .name       ("Schedule Scene Render")
        .schedules  ({scnRender.pl.render(Schedule)})
        .args       ({                   windowApp.di.windowAppLoopCtrl})
        .func       ([] (WindowAppLoopControl const &rWindowAppLoopCtrl) noexcept -> osp::TaskActions
    {
        return rWindowAppLoopCtrl.doRender ? osp::TaskActions{} : osp::TaskAction::Cancel;
    });

    rFB.task()
        .name       ("Resize ACtxSceneRender containers to fit all DrawEnts")
        .run_on     ({scnRender.pl.drawEntResized(Run)})
        .sync_with  ({scnRender.pl.entMesh(New), scnRender.pl.entTexture(New)})
        .args       ({scnRender.di.scnRender})
        .func       ([] (ACtxSceneRender &rScnRender) noexcept
    {
        rScnRender.resize_draw();
    });

    rFB.task()
        .name       ("Resize ACtxSceneRender to fit ActiveEnts")
        .run_on     ({comScn.pl.activeEntResized(Run)})
        .args       ({               comScn.di.basic,                  scnRender.di.scnRender})
        .func([]    (ACtxBasic const &rBasic,  ACtxSceneRender &rScnRender) noexcept
    {
        rScnRender.resize_active(rBasic.m_activeIds.capacity());
    });

    // Duplicate task needed for resync to account for existing ActiveEnts when the renderer opens,
    // as activeEntResized doesn't run during resync
    rFB.task()
        .name       ("Resync ACtxSceneRender to fit ActiveEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({comScn.pl.activeEntResized(Run)})
        .args       ({               comScn.di.basic,                  scnRender.di.scnRender})
        .func([]    (ACtxBasic const &rBasic,  ACtxSceneRender &rScnRender) noexcept
    {
        rScnRender.resize_active(rBasic.m_activeIds.capacity());
    });

    rFB.task()
        .name       ("Schedule Assign GL textures")
        .schedules  ({scnRender.pl.entTextureDirty(Schedule_)})
        .sync_with  ({scnRender.pl.texture(Ready), scnRender.pl.entTexture(Ready)})
        .args       ({            scnRender.di.scnRender })
        .func([] (ACtxSceneRender &rScnRender) noexcept -> TaskActions
    {
        return rScnRender.m_diffuseDirty.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rFB.task()
        .name       ("Schedule Assign GL meshes")
        .schedules  ({scnRender.pl.entMeshDirty(Schedule_)})
        .sync_with  ({scnRender.pl.mesh(Ready), scnRender.pl.entMesh(Ready)})
        .args       ({            scnRender.di.scnRender })
        .func([] (ACtxSceneRender &rScnRender) noexcept -> TaskActions
    {
        return rScnRender.m_meshDirty.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rFB.task()
        .name       ("Calculate draw transforms")
        .run_on     ({scnRender.pl.render(Run)})
        .sync_with  ({comScn.pl.hierarchy(Ready), comScn.pl.transform(Ready), comScn.pl.activeEnt(Ready), scnRender.pl.drawTransforms(Modify_), scnRender.pl.drawEnt(Ready), scnRender.pl.drawEntResized(Done), comScn.pl.activeEntResized(Done)})
        .args       ({            comScn.di.basic,                   comScn.di.drawing,                 scnRender.di.scnRender,                 scnRender.di.drawTfObservers })
        .func([] (ACtxBasic const &rBasic, ACtxDrawing const &rDrawing, ACtxSceneRender &rScnRender, DrawTfObservers &rDrawTfObservers) noexcept
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
                [&rDrawTfObservers, &rScnRender] (Matrix4 const &transform, active::ActiveEnt ent, int depth)
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
        .run_on     ({comScn.pl.activeEntDelete(UseOrRun)})
        .sync_with  ({scnRender.pl.drawEntDelete(Modify_)})
        .args       ({            scnRender.di.scnRender,                      comScn.di.activeEntDel,              comScn.di.drawEntDel })
        .func([] (ACtxSceneRender &rScnRender, ActiveEntVec_t const &rActiveEntDel, DrawEntVec_t &rDrawEntDel) noexcept
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
        .run_on     ({scnRender.pl.drawEntDelete(UseOrRun)})
        .sync_with  ({scnRender.pl.entTexture(Delete), scnRender.pl.entMesh(Delete)})
        .args       ({        comScn.di.drawing,                 scnRender.di.scnRender,                    comScn.di.drawEntDel })
        .func([] (ACtxDrawing &rDrawing, ACtxSceneRender &rScnRender, DrawEntVec_t const &rDrawEntDel) noexcept
    {
        SysRender::update_delete_drawing(rScnRender, rDrawing, rDrawEntDel.cbegin(), rDrawEntDel.cend());
    });

    rFB.task()
        .name       ("Delete DrawEntity IDs")
        .run_on     ({scnRender.pl.drawEntDelete(UseOrRun)})
        .sync_with  ({scnRender.pl.drawEnt(Delete)})
        .args       ({            scnRender.di.scnRender,                    comScn.di.drawEntDel })
        .func([] (ACtxSceneRender &rScnRender, DrawEntVec_t const &rDrawEntDel) noexcept
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
        .run_on     ({scnRender.pl.drawEntDelete(UseOrRun)})
        .sync_with  ({scnRender.pl.material(Delete)})
        .args       ({            scnRender.di.scnRender,                    comScn.di.drawEntDel })
        .func([] (ACtxSceneRender &rScnRender, DrawEntVec_t const &rDrawEntDel) noexcept
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
        .run_on     ({scnRender.pl.drawEntDelete(Clear)})
        .args       ({         comScn.di.drawEntDel })
        .func([] (DrawEntVec_t &rDrawEntDel) noexcept
    {
        rDrawEntDel.clear();
    });

    rFB.task()
        .name       ("Clear dirty DrawEnt's textures once we're done with it")
        .run_on     ({scnRender.pl.entMeshDirty(Clear)})
        .args       ({            scnRender.di.scnRender})
        .func([] (ACtxSceneRender &rScnRender) noexcept
    {
        rScnRender.m_meshDirty.clear();
    });

    rFB.task()
        .name       ("Clear dirty DrawEnt's textures once we're done with it")
        .run_on     ({scnRender.pl.entTextureDirty(Clear)})
        .args       ({            scnRender.di.scnRender})
        .func([] (ACtxSceneRender &rScnRender) noexcept
    {
        rScnRender.m_diffuseDirty.clear();
    });

    rFB.task()
        .name       ("Clear dirty materials once we're done with it")
        .run_on     ({scnRender.pl.materialDirty(Clear)})
        .args       ({            scnRender.di.scnRender})
        .func([] (ACtxSceneRender &rScnRender) noexcept
    {
        for (Material &rMat : rScnRender.m_materials)
        {
            rMat.m_dirty.clear();
        }
    });

    rFB.task()
        .name       ("Clean up scene owners")
        .run_on     ({cleanup.pl.cleanup(Run_)})
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender})
        .func       ([] (ACtxDrawing &rDrawing, ACtxSceneRender &rScnRender) noexcept
    {
        SysRender::clear_owners(rScnRender, rDrawing);
    });
}); // setup_scene_renderer

} // namespace adera
