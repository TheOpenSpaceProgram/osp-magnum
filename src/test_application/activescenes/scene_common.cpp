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
#include "identifiers.h"

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

Session setup_common_scene(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        TopDataId const             idResources,
        PkgId const                 pkg)
{
    auto &rResources    = top_get< Resources >      (topData, idResources);

    Session scnCommon;
    OSP_SESSION_ACQUIRE_DATA(scnCommon, topData,    TESTAPP_COMMON_SCENE);
    OSP_SESSION_ACQUIRE_TAGS(scnCommon, rTags,      TESTAPP_COMMON_SCENE);
    scnCommon.m_tgCleanupEvt = tgCleanupEvt;

    top_emplace< float >            (topData, idDeltaTimeIn, 1.0f / 60.0f);
    top_emplace< EntVector_t >      (topData, idDelEnts);
    top_emplace< EntVector_t >      (topData, idDelTotal);

    auto &rBasic        = top_emplace< ACtxBasic >      (topData, idBasic);
    auto &rActiveIds    = top_emplace< ActiveReg_t >    (topData, idActiveIds);
    auto &rDrawing      = top_emplace< ACtxDrawing >    (topData, idDrawing);
    auto &rDrawingRes   = top_emplace< ACtxDrawingRes > (topData, idDrawingRes);
    auto &rNMesh        = top_emplace< NamedMeshes >    (topData, idNMesh);

    rBuilder.tag(tgEntNew)          .depend_on({tgEntDel});
    rBuilder.tag(tgEntReq)          .depend_on({tgEntDel, tgEntNew});
    rBuilder.tag(tgDelEntReq)       .depend_on({tgDelEntMod});
    rBuilder.tag(tgDelEntClr)       .depend_on({tgDelEntMod, tgDelEntReq});
    rBuilder.tag(tgDelTotalReq)     .depend_on({tgDelTotalMod});
    rBuilder.tag(tgDelTotalClr)     .depend_on({tgDelTotalMod, tgDelTotalReq});
    rBuilder.tag(tgTransformDel)    .depend_on({tgTransformMod});
    rBuilder.tag(tgTransformNew)    .depend_on({tgTransformMod, tgTransformDel});
    rBuilder.tag(tgTransformReq)    .depend_on({tgTransformMod, tgTransformDel, tgTransformNew});
    rBuilder.tag(tgHierNew)         .depend_on({tgHierDel});
    rBuilder.tag(tgHierModEnd)      .depend_on({tgHierDel, tgHierNew, tgHierMod});
    rBuilder.tag(tgHierReq)         .depend_on({tgHierMod, tgHierModEnd});
    rBuilder.tag(tgDrawMod)         .depend_on({tgDrawDel});
    rBuilder.tag(tgDrawReq)         .depend_on({tgDrawDel, tgDrawMod});
    rBuilder.tag(tgMeshMod)         .depend_on({tgMeshDel});
    rBuilder.tag(tgMeshReq)         .depend_on({tgMeshDel, tgMeshMod});
    rBuilder.tag(tgMeshClr)         .depend_on({tgMeshDel, tgMeshMod, tgMeshReq});
    rBuilder.tag(tgTexMod)          .depend_on({tgTexDel});
    rBuilder.tag(tgTexReq)          .depend_on({tgTexDel, tgTexMod});
    rBuilder.tag(tgTexClr)          .depend_on({tgTexDel, tgTexMod, tgTexReq});

    scnCommon.task() = rBuilder.task().assign({tgResyncEvt}).data(
            "Set entity meshes and textures dirty",
            TopDataIds_t{             idDrawing},
            wrap_args([] (ACtxDrawing& rDrawing) noexcept
    {
        SysRender::set_dirty_all(rDrawing);
    }));

    scnCommon.task() = rBuilder.task().assign({tgSceneEvt, tgDelEntReq, tgDelTotalMod}).data(
            "Create DeleteTotal vector, which includes descendents of deleted hierarchy entities",
            TopDataIds_t{           idBasic,                   idDelEnts,             idDelTotal},
            wrap_args([] (ACtxBasic& rBasic, EntVector_t const& rDelEnts, EntVector_t& rDelTotal) noexcept
    {
        auto const &delFirst    = std::cbegin(rDelEnts);
        auto const &delLast     = std::cend(rDelEnts);

        rDelTotal.assign(delFirst, delLast);

        for (ActiveEnt root : rDelEnts)
        {
            for (ActiveEnt descendant : SysSceneGraph::descendants(rBasic.m_scnGraph, root))
            {
                rDelTotal.push_back(descendant);
            }
        }
    }));

    scnCommon.task() = rBuilder.task().assign({tgSceneEvt, tgDelEntReq, tgHierMod}).data(
            "Cut deleted entities out of hierarchy",
            TopDataIds_t{           idBasic,                   idDelEnts},
            wrap_args([] (ACtxBasic& rBasic, EntVector_t const& rDelEnts) noexcept
    {
        auto const &delFirst    = std::cbegin(rDelEnts);
        auto const &delLast     = std::cend(rDelEnts);

        SysSceneGraph::cut(rBasic.m_scnGraph, delFirst, delLast);
    }));

    scnCommon.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgEntDel}).data(
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

    scnCommon.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgTransformDel, tgHierDel}).data(
            "Delete basic components",
            TopDataIds_t{           idBasic,                   idDelTotal},
            wrap_args([] (ACtxBasic& rBasic, EntVector_t const& rDelTotal) noexcept
    {
        update_delete_basic(rBasic, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    scnCommon.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgDrawDel}).data(
            "Delete drawing components",
            TopDataIds_t{              idDrawing,                  idDelTotal},
            wrap_args([] (ACtxDrawing& rDrawing, EntVector_t const& rDelTotal) noexcept
    {
        SysRender::update_delete_drawing(rDrawing, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    scnCommon.task() = rBuilder.task().assign({tgSceneEvt, tgDelEntClr}).data(
            "Clear delete vectors once we're done with it",
            TopDataIds_t{             idDelEnts},
            wrap_args([] (EntVector_t& rDelEnts) noexcept
    {
        rDelEnts.clear();
    }));

    scnCommon.task() = rBuilder.task().assign({tgCleanupEvt}).data(
            "Clean up scene and resource owners",
            TopDataIds_t{             idDrawing,                idDrawingRes,           idResources},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources) noexcept
    {
        SysRender::clear_owners(rDrawing);
        SysRender::clear_resource_owners(rDrawingRes, rResources);
    }));


    // Convenient functor to get a reference-counted mesh owner
    auto const quick_add_mesh = SysRender::gen_drawable_mesh_adder(rDrawing, rDrawingRes, rResources, pkg);

    scnCommon.task() = rBuilder.task().assign({tgCleanupEvt}).data(
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

    // Acquire mesh resources from Package
    using osp::phys::EShape;
    rNMesh.m_shapeToMesh.emplace(EShape::Box,       quick_add_mesh("cube"));
    rNMesh.m_shapeToMesh.emplace(EShape::Cylinder,  quick_add_mesh("cylinder"));
    rNMesh.m_shapeToMesh.emplace(EShape::Sphere,    quick_add_mesh("sphere"));
    rNMesh.m_namedMeshs.emplace("floor", quick_add_mesh("grid64solid"));


    return scnCommon;
}

Session setup_material(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon)
{
    OSP_SESSION_UNPACK_DATA(scnCommon, TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon, TESTAPP_COMMON_SCENE);

    Session material;
    OSP_SESSION_ACQUIRE_DATA(material, topData, TESTAPP_MATERIAL);
    OSP_SESSION_ACQUIRE_TAGS(material, rTags, TESTAPP_MATERIAL);

    top_emplace< EntSet_t >     (topData, idMatEnts);
    top_emplace< EntVector_t >  (topData, idMatDirty);

    rBuilder.tag(tgMatMod)      .depend_on({tgMatDel});
    rBuilder.tag(tgMatReq)      .depend_on({tgMatDel, tgMatMod});
    rBuilder.tag(tgMatClr)      .depend_on({tgMatDel, tgMatMod, tgMatReq});

    material.task() = rBuilder.task().assign({tgResyncEvt}).data(
            "Set all X material entities as dirty",
            TopDataIds_t{                idMatEnts,             idMatDirty},
            wrap_args([] (EntSet_t const& rMatEnts, EntVector_t& rMatDirty) noexcept
    {
        for (std::size_t const entInt : rMatEnts.ones())
        {
            rMatDirty.push_back(ActiveEnt(entInt));
        }
    }));

    material.task() = rBuilder.task().assign({tgSceneEvt, tgSyncEvt, tgMatClr}).data(
            "Clear dirty vectors for material",
            TopDataIds_t{             idMatDirty},
            wrap_args([] (EntVector_t& rMatDirty) noexcept
    {
        rMatDirty.clear();
    }));

    material.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgMatDel}).data(
            "Delete material components",
            TopDataIds_t{idMatEnts, idDelTotal}, delete_ent_set);


    return material;
}

}




