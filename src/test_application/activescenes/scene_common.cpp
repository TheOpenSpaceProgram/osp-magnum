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
#include "scene_common.h"
#include "scenarios.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>

#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/SysRender.h>

#include <osp/Resource/resources.h>

#include <osp/unpack.h>

using namespace osp;
using namespace osp::active;

namespace testapp::scenes
{

Session setup_scene(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData)
{
    osp::Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SCENE);

    top_emplace< float >(topData, idDeltaTimeIn, 1.0f / 60.0f);

    out.create_pipelines<PlScene>(rBuilder);
    return out;
}

Session setup_common_scene(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              application,
        PkgId const                 pkg)
{
    OSP_DECLARE_GET_DATA_IDS(application,  TESTAPP_DATA_APPLICATION);

    auto const tgScn    = scene.get_pipelines<PlScene>();
    auto &rResources    = top_get< Resources >      (topData, idResources);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_COMMON_SCENE);
    auto const tgCS = out.create_pipelines<PlCommonScene>(rBuilder);

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
    rBuilder.pipeline(tgCS.drawEnt)             .parent(tgScn.update);
    rBuilder.pipeline(tgCS.drawEntResized)      .parent(tgScn.update);
    rBuilder.pipeline(tgCS.drawEntDelete)       .parent(tgScn.update);
    rBuilder.pipeline(tgCS.mesh)                .parent(tgScn.update);
    rBuilder.pipeline(tgCS.texture)             .parent(tgScn.update);
    rBuilder.pipeline(tgCS.entTextureDirty)     .parent(tgScn.update);
    rBuilder.pipeline(tgCS.entMeshDirty)        .parent(tgScn.update);
    rBuilder.pipeline(tgCS.meshResDirty)        .parent(tgScn.update);
    rBuilder.pipeline(tgCS.textureResDirty)     .parent(tgScn.update);
    rBuilder.pipeline(tgCS.material)            .parent(tgScn.update);
    rBuilder.pipeline(tgCS.materialDirty)       .parent(tgScn.update);

    rBuilder.task()
        .name       ("Delete ActiveEnt IDs")
        .run_on     ({tgCS.activeEnt(Delete)})
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
        .name       ("Cancel entity delete tasks stuff if no entities were deleted")
        .run_on     ({tgCS.activeEntDelete(Schedule_)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                      idActiveEntDel })
        .func([] (ACtxBasic& rBasic, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        return rActiveEntDel.empty() ? TaskAction::Cancel : TaskActions{};
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
        .name       ("Delete DrawEntity of deleted ActiveEnts")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgCS.drawEntDelete(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                      idActiveEntDel,              idDrawEntDel })
        .func([] (ACtxDrawing& rDrawing, ActiveEntVec_t const& rActiveEntDel, DrawEntVec_t& rDrawEntDel) noexcept
    {
        for (ActiveEnt const ent : rActiveEntDel)
        {
            DrawEnt const drawEnt = std::exchange(rDrawing.m_activeToDraw[ent], lgrn::id_null<DrawEnt>());
            if (drawEnt != lgrn::id_null<DrawEnt>())
            {
                rDrawEntDel.push_back(drawEnt);
            }
        }
    });

    rBuilder.task()
        .name       ("Delete drawing components")
        .run_on     ({tgCS.drawEntDelete(UseOrRun)})
        .sync_with  ({tgCS.mesh(Delete), tgCS.texture(Delete)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                    idDrawEntDel })
        .func([] (ACtxDrawing& rDrawing, DrawEntVec_t const& rDrawEntDel) noexcept
    {
        SysRender::update_delete_drawing(rDrawing, rDrawEntDel.cbegin(), rDrawEntDel.cend());
    });

    rBuilder.task()
        .name       ("Delete DrawEntity IDs")
        .run_on     ({tgCS.drawEntDelete(UseOrRun)})
        .sync_with  ({tgCS.drawEnt(Delete)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                    idDrawEntDel })
        .func([] (ACtxDrawing& rDrawing, DrawEntVec_t const& rDrawEntDel) noexcept
    {
        for (DrawEnt const drawEnt : rDrawEntDel)
        {
            if (rDrawing.m_drawIds.exists(drawEnt))
            {
                rDrawing.m_drawIds.remove(drawEnt);
            }
        }
    });

    rBuilder.task()
        .name       ("Delete DrawEnt from materials")
        .run_on     ({tgCS.drawEntDelete(UseOrRun)})
        .sync_with  ({tgCS.material(Delete)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                    idDrawEntDel })
        .func([] (ACtxDrawing& rDrawing, DrawEntVec_t const& rDrawEntDel) noexcept
    {
        for (DrawEnt const ent : rDrawEntDel)
        {
            for (Material &rMat : rDrawing.m_materials)
            {
                rMat.m_ents.reset(std::size_t(ent));
            }
        }
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

    rBuilder.task()
        .name       ("Clear DrawEnt delete vector once we're done with it")
        .run_on     ({tgCS.drawEntDelete(Clear)})
        .push_to    (out.m_tasks)
        .args       ({         idDrawEntDel })
        .func([] (DrawEntVec_t& rDrawEntDel) noexcept
    {
        rDrawEntDel.clear();
    });

    rBuilder.task()
        .name       ("Clear material dirty vectors once we're done with it")
        .run_on     ({tgCS.materialDirty(Clear)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing })
        .func([] (ACtxDrawing& rDrawing) noexcept
    {
        for (std::size_t const materialInt : rDrawing.m_materialIds.bitview().zeros())
        {
            rDrawing.m_materials[MaterialId(materialInt)].m_dirty.clear();
        }
    });

    rBuilder.task()
        .name       ("Clean up scene and resource owners")
        .run_on     ({tgScn.cleanup(Run_)})
        //.sync_with  ({tgCS.mesh(Delete), tgCS.texture(Delete)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                idDrawingRes,           idResources})
        .func([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources) noexcept
    {
        SysRender::clear_owners(rDrawing);
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
    using osp::phys::EShape;
    rNMesh.m_shapeToMesh.emplace(EShape::Box,       quick_add_mesh("cube"));
    rNMesh.m_shapeToMesh.emplace(EShape::Cylinder,  quick_add_mesh("cylinder"));
    rNMesh.m_shapeToMesh.emplace(EShape::Sphere,    quick_add_mesh("sphere"));
    rNMesh.m_namedMeshs.emplace("floor", quick_add_mesh("grid64solid"));

    return out;
}

} // namespace testapp::scenes
