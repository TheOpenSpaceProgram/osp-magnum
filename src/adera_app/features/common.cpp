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


FeatureDef const ftrMainApp = feature_def("Main", [] (FeatureBuilder &rFB, Implement<FIMainApp> mainApp)
{
    rFB.data_emplace< AppContexts >             (mainApp.di.appContexts, AppContexts{.main = rFB.ctx});
    rFB.data_emplace< MainLoopControl >         (mainApp.di.mainLoopCtrl);
    rFB.data_emplace< osp::Resources >          (mainApp.di.resources);
    rFB.data_emplace< FrameworkModify >         (mainApp.di.frameworkModify);

    rFB.pipeline(mainApp.pl.keepOpen).parent(mainApp.loopblks.mainLoop);

    rFB.task(mainApp.tasks.schedule)
        .name       ("Schedule Main Loop")
        .schedules  ({mainApp.loopblks.mainLoop})
        .args       ({         mainApp.di.mainLoopCtrl})
        .ext_finish(true)
        .func       ([] (MainLoopControl &rLoopControl) noexcept
    {
        rLoopControl.mainScheduleWaiting = true;
    });

    rFB.task(mainApp.tasks.keepOpen )
        .name       ("Schedule KeepOpen")
        .schedules  ({mainApp.pl.keepOpen})
        .args       ({         mainApp.di.mainLoopCtrl})
        .ext_finish(true)
        .func       ([] (MainLoopControl &rLoopControl) noexcept
    {
        rLoopControl.keepOpenWaiting = true;
    });
});

FeatureDef const ftrCleanupCtx = feature_def("CleanupCtx", [] (
        FeatureBuilder              &rFB,
        Implement<FICleanupContext> cleanup)
{
    rFB.data_emplace<bool>(cleanup.di.ranOnce, false);
    rFB.pipeline(cleanup.pl.cleanup).parent(cleanup.loopblks.cleanup);

    rFB.task(cleanup.tasks.blockSchedule)
        .name        ("Schedule Clean-Up Loopblock")
        .schedules   (cleanup.loopblks.cleanup)
        .ext_finish  (true);

    rFB.task(cleanup.tasks.pipelineSchedule)
        .name       ("Schedule Clean-Up Pipeline")
        .schedules  (cleanup.pl.cleanup)
        .args       ({                     cleanup.di.ranOnce})
        .func       ([] (bool &rRanOnce) noexcept -> osp::TaskActions
    {
        bool const ranOncePrev = rRanOnce;
        rRanOnce = true;
        return {.cancel = ranOncePrev};
    });
}); // ftrCleanupCtx

FeatureDef const ftrScene = feature_def("Scene", [] (
        FeatureBuilder          &rFB,
        Implement<FIScene>      scn,
        DependOn<FIMainApp>     mainApp)
{
    rFB.data_emplace<float>(scn.di.deltaTimeIn, 1.0f / 60.0f);
    rFB.data_emplace<SceneLoopControl>(scn.di.loopControl);
    //rFB.pipeline(scn.pl.loopControl).parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scn.pl.update)     .parent(mainApp.loopblks.mainLoop);

//    rFB.task()
//        .name       ("Schedule Scene update")
//        .schedules  (scn.pl.update)
//        //.sync_with  ({scn.pl.loopControl(Ready)})
//        .args       ({                     scn.di.loopControl})
//        .func       ([] (SceneLoopControl const &rLoopControl) noexcept -> osp::TaskActions
//    {
//        return {.cancel = false};//! rLoopControl.doSceneUpdate}; SYNCEXEC
//    });
}); // ftrScene


FeatureDef const ftrCommonScene = feature_def("CommonScene", [] (
        FeatureBuilder              &rFB,
        Implement<FICommonScene>    comScn,
        DependOn<FICleanupContext>  cleanup,
        DependOn<FIScene>           scn,
        DependOn<FIMainApp>         mainApp,
        entt::any                   userData)
{
    /* not used here */   rFB.data_emplace< ActiveEntVec_t >(comScn.di.activeEntDel);
    auto &rBasic        = rFB.data_emplace< ACtxBasic >     (comScn.di.basic);
    auto &rDrawing      = rFB.data_emplace< ACtxDrawing >   (comScn.di.drawing);
    auto &rDrawingRes   = rFB.data_emplace< ACtxDrawingRes >(comScn.di.drawingRes);
    auto &rNamedMeshes  = rFB.data_emplace< NamedMeshes >   (comScn.di.namedMeshes);

    rFB.pipeline(comScn.pl.activeEnt)           .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(comScn.pl.activeEntDelete)     .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(comScn.pl.transform)           .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(comScn.pl.hierarchy)           .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(comScn.pl.meshIds)             .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(comScn.pl.texIds)              .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(comScn.pl.texToRes)            .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(comScn.pl.meshToRes)           .parent(mainApp.loopblks.mainLoop);

    rFB.task().schedules(comScn.pl.activeEntDelete).args({comScn.di.activeEntDel}).name("Schedule activeEntDelete")
              .func(  [] (ActiveEntVec_t const &rActiveEntDel) noexcept -> TaskActions
                      { return {.cancel = rActiveEntDel.empty()}; });

    rFB.task()
        .name       ("Delete ActiveEnt IDs of deleted ActiveEnts")
        .sync_with  ({comScn.pl.activeEntDelete(UseOrRun), comScn.pl.activeEnt(Delete)})
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
        .name       ("Delete transforms of deleted ActiveEnts")
        .sync_with  ({comScn.pl.activeEntDelete(UseOrRun), comScn.pl.transform(Delete)})
        .args       ({     comScn.di.basic,              comScn.di.activeEntDel })
        .func       ([] (ACtxBasic &rBasic, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        for (ActiveEnt const ent : rActiveEntDel)
        {
            if (rBasic.m_transform.contains(ent))
            {
                rBasic.m_transform.remove(ent);
            }
        }
    });

    rFB.task()
        .name       ("Clear ActiveEnt delete vector once we're done with it")
        .sync_with  ({comScn.pl.activeEntDelete(Clear)})
        .args       ({      comScn.di.activeEntDel })
        .func([]    (ActiveEntVec_t &rActiveEntDel) noexcept
    {
        rActiveEntDel.clear();
    });

    // ------

    rFB.task().schedules(comScn.pl.activeEntDelete).args({comScn.di.activeEntDel}).name("activeEntDelete")
              .func(  [] (ActiveEntVec_t const &rActiveEntDel) noexcept -> TaskActions
                      { return {.cancel = rActiveEntDel.empty()}; });


    // Clean up tasks

    rFB.task()
        .name       ("Clean up resource owners")
        .sync_with  ({cleanup.pl.cleanup(Run_)})
        .args       ({       comScn.di.drawing,        comScn.di.drawingRes,  mainApp.di.resources})
        .func       ([] (ACtxDrawing &rDrawing, ACtxDrawingRes &rDrawingRes, Resources &rResources) noexcept
    {
        SysRender::clear_resource_owners(rDrawingRes, rResources);
    });

    rFB.task()
        .name       ("Clean up NamedMeshes mesh and texture owners")
        .sync_with  ({cleanup.pl.cleanup(Run_)})
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
        DependOn<FIMainApp>         mainApp)
{
    rFB.pipeline(windowApp.pl.inputs).parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(windowApp.pl.sync)  .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(windowApp.pl.resync).parent(mainApp.loopblks.mainLoop);

    auto &rUserInput     = rFB.data_emplace<osp::input::UserInputHandler>(windowApp.di.userInput, 12);
    auto &rWindowAppCtrl = rFB.data_emplace<WindowAppLoopControl>        (windowApp.di.windowAppLoopCtrl);

    rFB.task()
        .name       ("Schedule Renderer Sync")
        .schedules  (windowApp.pl.sync)
        .args       ({                   windowApp.di.windowAppLoopCtrl})
        .func       ([] (WindowAppLoopControl const &rWindowAppLoopCtrl) noexcept -> osp::TaskActions
    {
        return {.cancel = ! rWindowAppLoopCtrl.doSync};
    });

    rFB.task()
        .name       ("Schedule Renderer Resync")
        .schedules  ({windowApp.pl.resync})
        .args       ({             windowApp.di.windowAppLoopCtrl})
        .func       ([] (WindowAppLoopControl &rWindowAppLoopCtrl) noexcept -> osp::TaskActions
    {
        bool const doResyncPrev = rWindowAppLoopCtrl.doResync;
        rWindowAppLoopCtrl.doResync = false;
        return {.cancel = ! doResyncPrev};
    });

}); // ftrWindowApp


FeatureDef const ftrSceneRenderer = feature_def("SceneRenderer", [] (
        FeatureBuilder              &rFB,
        Implement<FISceneRenderer>  scnRender,
        DependOn<FICleanupContext>  cleanup,
        DependOn<FICommonScene>     comScn,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIWindowApp>       windowApp)
{
    rFB.pipeline(scnRender.pl.render)           .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.drawEnt)          .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.misc)             .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.drawTransforms)   .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.activeDrawTfs)    .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.diffuseTex)       .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.diffuseTexDirty)  .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.mesh)             .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.meshDirty)        .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.material)         .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.materialDirty)    .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(scnRender.pl.drawEntDelete)    .parent(mainApp.loopblks.mainLoop);

    auto &rScnRender = rFB.data_emplace<ACtxSceneRender>(scnRender.di.scnRender);
    /* unused */       rFB.data_emplace<DrawTfObservers>(scnRender.di.drawTfObservers);
    /* unused */       rFB.data_emplace<DrawEntVec_t>   (scnRender.di.drawEntDel);


    rFB.task()
        .name       ("Resize ACtxSceneRender misc containers to fit all DrawEnts")
        .sync_with  ({scnRender.pl.drawEnt(Ready), scnRender.pl.misc(Resize_)})
        .args       ({        scnRender.di.scnRender})
        .func       ([] (ACtxSceneRender &rScnRender) noexcept
    {
        auto const capacity = rScnRender.m_drawIds.capacity();
        rScnRender.m_opaque     .resize(capacity);
        rScnRender.m_transparent.resize(capacity);
        rScnRender.m_visible    .resize(capacity);
        rScnRender.m_color      .resize(capacity, {1.0f, 1.0f, 1.0f, 1.0f}); // Default to white
    });

    rFB.task()
        .name       ("Resize ACtxSceneRender::m_drawTransform to fit all DrawEnts")
        .sync_with  ({scnRender.pl.drawEnt(Ready), scnRender.pl.drawTransforms(Resize_)})
        .args       ({        scnRender.di.scnRender})
        .func       ([] (ACtxSceneRender &rScnRender) noexcept
    {
        rScnRender.m_drawTransform.resize(rScnRender.m_drawIds.capacity());
    });

    rFB.task()
        .name       ("Resize ACtxSceneRender::m_diffuseTex to fit all DrawEnts")
        .sync_with  ({scnRender.pl.drawEnt(Ready), scnRender.pl.diffuseTex(Resize_)})
        .args       ({        scnRender.di.scnRender})
        .func       ([] (ACtxSceneRender &rScnRender) noexcept
    {
        rScnRender.m_diffuseTex.resize(rScnRender.m_drawIds.capacity());
    });

    rFB.task()
        .name       ("Resize ACtxSceneRender::m_mesh to fit all DrawEnts")
        .sync_with  ({scnRender.pl.drawEnt(Ready), scnRender.pl.mesh(Resize_)})
        .args       ({        scnRender.di.scnRender})
        .func       ([] (ACtxSceneRender &rScnRender) noexcept
    {
        rScnRender.m_mesh.resize(rScnRender.m_drawIds.capacity());
    });


    rFB.task()
        .name       ("Resize ACtxSceneRender::m_material[#].m_ents to fit all DrawEnts")
        .sync_with  ({scnRender.pl.drawEnt(Ready), scnRender.pl.material(Resize_)})
        .args       ({        scnRender.di.scnRender})
        .func       ([] (ACtxSceneRender &rScnRender) noexcept
    {
        auto const capacity = rScnRender.m_drawIds.capacity();
        for (MaterialId matId : rScnRender.m_materialIds)
        {
            rScnRender.m_materials[matId].m_ents.resize(capacity);
        }
    });

    rFB.task()
        .name       ("Resize ACtxSceneRender::{m_needDrawTf,m_activeToDraw,drawTfObserverEnable} to fit all ActiveEnts")
        .sync_with  ({comScn.pl.activeEnt(Ready), scnRender.pl.activeDrawTfs(Resize_)})
        .args       ({           comScn.di.basic,       scnRender.di.scnRender})
        .func       ([] (ACtxBasic const& rBasic,  ACtxSceneRender &rScnRender) noexcept
    {
        auto const capacity = rBasic.m_activeIds.capacity();
        rScnRender.m_needDrawTf        .resize(capacity);
        rScnRender.m_activeToDraw      .resize(capacity, lgrn::id_null<DrawEnt>());
        rScnRender.drawTfObserverEnable.resize(capacity, 0);
    });

    rFB.task()
        .name       ("Calculate draw transforms")
        .sync_with  ({scnRender.pl.render(Run), comScn.pl.hierarchy(Ready), comScn.pl.transform(Ready), comScn.pl.activeEnt(Ready), scnRender.pl.drawTransforms(Modify), scnRender.pl.drawEnt(Ready), scnRender.pl.activeDrawTfs(Ready)})
        .args       ({           comScn.di.basic,           comScn.di.drawing,      scnRender.di.scnRender,                 scnRender.di.drawTfObservers })
        .func       ([] (ACtxBasic const &rBasic, ACtxDrawing const &rDrawing, ACtxSceneRender &rScnRender, DrawTfObservers &rDrawTfObservers) noexcept
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
        .sync_with  ({comScn.pl.activeEntDelete(UseOrRun), scnRender.pl.drawEntDelete(Modify_)})
        .args       ({        scnRender.di.scnRender,              comScn.di.activeEntDel,   scnRender.di.drawEntDel })
        .func       ([] (ACtxSceneRender &rScnRender, ActiveEntVec_t const &rActiveEntDel, DrawEntVec_t &rDrawEntDel) noexcept
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
        .name       ("Clear DrawEnt delete vector once we're done with it")
        .schedules  (scnRender.pl.drawEntDelete)
        .args       ({     scnRender.di.drawEntDel })
        .func       ([] (DrawEntVec_t &rDrawEntDel) noexcept -> osp::TaskActions
    {
        return {.cancel = rDrawEntDel.empty()};
    });

    rFB.task()
        .name       ("Delete drawing components")
        .sync_with  ({scnRender.pl.drawEntDelete(UseOrRun), scnRender.pl.diffuseTex(Delete), scnRender.pl.mesh(Delete)})
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender,         scnRender.di.drawEntDel })
        .func       ([] (ACtxDrawing &rDrawing, ACtxSceneRender &rScnRender, DrawEntVec_t const &rDrawEntDel) noexcept
    {
        SysRender::update_delete_drawing(rScnRender, rDrawing, rDrawEntDel.cbegin(), rDrawEntDel.cend());
    });

    rFB.task()
        .name       ("Delete DrawEntity IDs")
        .sync_with  ({scnRender.pl.drawEntDelete(UseOrRun), scnRender.pl.drawEnt(Delete)})
        .args       ({        scnRender.di.scnRender,            scnRender.di.drawEntDel })
        .func       ([] (ACtxSceneRender &rScnRender, DrawEntVec_t const &rDrawEntDel) noexcept
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
        .sync_with  ({scnRender.pl.drawEntDelete(UseOrRun), scnRender.pl.material(Delete)})
        .args       ({        scnRender.di.scnRender,         scnRender.di.drawEntDel })
        .func       ([] (ACtxSceneRender &rScnRender, DrawEntVec_t const &rDrawEntDel) noexcept
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

    // -------------


    rFB.task().schedules(scnRender.pl.drawEntDelete).args({scnRender.di.drawEntDel}).name("Schedule drawEntDelete")
              .func(  [] (DrawEntVec_t const &rDrawEntDel) noexcept -> TaskActions
                      { return {.cancel = rDrawEntDel.empty()}; });
    rFB.task().schedules(scnRender.pl.diffuseTexDirty).args({scnRender.di.scnRender}).name("Schedule diffuseTexDirty")
              .func(  [] (ACtxSceneRender const &rScnRender) noexcept -> TaskActions
                      { return {.cancel = rScnRender.m_diffuseTexDirty.empty()}; });
    rFB.task().schedules(scnRender.pl.meshDirty).args({scnRender.di.scnRender}).name("Schedule meshDirty")
              .func(  [] (ACtxSceneRender const &rScnRender) noexcept -> TaskActions
                      { return {.cancel = rScnRender.m_meshDirty.empty()}; });

    rFB.task()
        .name       ("Schedule materialDirty")
        .schedules  ({scnRender.pl.materialDirty})
        .args       ({              scnRender.di.scnRender })
        .func       ([] (ACtxSceneRender const &rScnRender) noexcept -> TaskActions
    {
        for (MaterialId const matId : rScnRender.m_materialIds)
        {
            if ( ! rScnRender.m_materials[matId].m_dirty.empty() )
            {
                return {.cancel = false };
            }
        }
        return {.cancel = true };
    });

    // -------------

    rFB.task()
        .name       ("Clear DrawEnt delete vector once we're done with it")
        .sync_with  ({scnRender.pl.drawEntDelete(Clear)})
        .args       ({     scnRender.di.drawEntDel })
        .func       ([] (DrawEntVec_t &rDrawEntDel) noexcept
    {
        rDrawEntDel.clear();
    });

    rFB.task()
        .name       ("Clear dirty DrawEnt's textures once we're done with it")
        .sync_with  ({scnRender.pl.meshDirty(Clear)})
        .args       ({        scnRender.di.scnRender})
        .func       ([] (ACtxSceneRender &rScnRender) noexcept
    {
        rScnRender.m_meshDirty.clear();
    });

    rFB.task()
        .name       ("Clear dirty DrawEnt's textures once we're done with it")
        .sync_with  ({scnRender.pl.diffuseTexDirty(Clear)})
        .args       ({        scnRender.di.scnRender})
        .func       ([] (ACtxSceneRender &rScnRender) noexcept
    {
        rScnRender.m_diffuseTexDirty.clear();
    });

    rFB.task()
        .name       ("Clear dirty materials once we're done with it")
        .sync_with  ({scnRender.pl.materialDirty(Clear)})
        .args       ({        scnRender.di.scnRender})
        .func       ([] (ACtxSceneRender &rScnRender) noexcept
    {
        for (Material &rMat : rScnRender.m_materials)
        {
            rMat.m_dirty.clear();
        }
    });

    rFB.task()
        .name       ("Clean up scene owners")
        .sync_with  ({cleanup.pl.cleanup(Run_)})
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender})
        .func       ([] (ACtxDrawing &rDrawing, ACtxSceneRender &rScnRender) noexcept
    {
        SysRender::clear_owners(rScnRender, rDrawing);
    });
}); // setup_scene_renderer

} // namespace adera
