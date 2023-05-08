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

Session setup_scene(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData)
{
    osp::Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SCENE);

    top_emplace< float >(topData, idDeltaTimeIn, 1.0f / 60.0f);

    out.create_targets<TgtScene>(rBuilder);
    return out;
}

Session setup_common_scene(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        TopDataId const             idResources,
        PkgId const                 pkg)
{
    auto const tgScn    = scene.get_targets<TgtScene>();
    auto &rResources    = top_get< Resources >      (topData, idResources);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_COMMON_SCENE);
    auto const tgCS = out.create_targets<TgtCommonScene>(rBuilder);

    /* unused */          top_emplace< ActiveEntVec_t > (topData, idActiveEntDel);
    /* unused */          top_emplace< DrawEntVec_t >   (topData, idDrawEntDel);
    auto &rBasic        = top_emplace< ACtxBasic >      (topData, idBasic);
    auto &rDrawing      = top_emplace< ACtxDrawing >    (topData, idDrawing);
    auto &rDrawingRes   = top_emplace< ACtxDrawingRes > (topData, idDrawingRes);
    auto &rNMesh        = top_emplace< NamedMeshes >    (topData, idNMesh);

    rBuilder.task()
        .name       ("Set materials, meshes, and textures dirty")
        .trigger_on ({tgScn.resyncAll})
        .fulfills   ({tgCS.texture_mod, tgCS.mesh_mod})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing })
        .func([] (ACtxDrawing& rDrawing) noexcept
    {
        SysRender::set_dirty_all(rDrawing);
    });

    rBuilder.task()
        .name       ("Delete ActiveEnt IDs")
        .trigger_on ({tgCS.delActiveEnt_mod})
        .fulfills   ({tgCS.delActiveEnt_use, tgCS.activeEnt_del, tgCS.activeEnt_mod})
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
        .trigger_on ({tgCS.delActiveEnt_mod})
        .fulfills   ({tgCS.delActiveEnt_use, tgCS.transform_del, tgCS.transform_mod})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                      idActiveEntDel })
        .func([] (ACtxBasic& rBasic, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        update_delete_basic(rBasic, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    rBuilder.task()
        .name       ("Delete DrawEntity of deleted ActiveEnts")
        .trigger_on ({tgCS.delActiveEnt_mod})
        .fulfills   ({tgCS.delActiveEnt_use, tgCS.delDrawEnt_mod})
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
        .trigger_on ({tgCS.delDrawEnt_mod})
        .fulfills   ({tgCS.delDrawEnt_use, tgCS.mesh_del, tgCS.mesh_mod, tgCS.texture_del, tgCS.texture_mod})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                    idDrawEntDel })
        .func([] (ACtxDrawing& rDrawing, DrawEntVec_t const& rDrawEntDel) noexcept
    {
        SysRender::update_delete_drawing(rDrawing, rDrawEntDel.cbegin(), rDrawEntDel.cend());
    });

    rBuilder.task()
        .name       ("Delete DrawEntity IDs")
        .trigger_on ({tgCS.delDrawEnt_mod})
        .fulfills   ({tgCS.delDrawEnt_use, tgCS.drawEnt_del, tgCS.drawEnt_mod})
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
        .trigger_on ({tgCS.delDrawEnt_mod})
        .fulfills   ({tgCS.delDrawEnt_use, tgCS.material_del, tgCS.material_mod})
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
        .name       ("Clear delete vectors once we're done with it")
        .trigger_on ({tgCS.delActiveEnt_use, tgCS.delDrawEnt_use})
        .fulfills   ({tgCS.delActiveEnt_clr, tgCS.delDrawEnt_clr})
        .push_to    (out.m_tasks)
        .args       ({            idActiveEntDel,              idDrawEntDel })
        .func([] (ActiveEntVec_t& idActiveEntDel, DrawEntVec_t& rDrawEntDel) noexcept
    {
        idActiveEntDel.clear();
        rDrawEntDel.clear();
    });

    rBuilder.task()
        .name       ("Clear material dirty vectors once we're done with it")
        .trigger_on ({tgCS.material_use})
        .fulfills   ({tgCS.material_clr})
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
        .trigger_on ({tgScn.cleanup})
        .depends_on ({tgCS.mesh_use, tgCS.texture_use})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                idDrawingRes,           idResources})
        .func([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources) noexcept
    {
        SysRender::clear_owners(rDrawing);
        SysRender::clear_resource_owners(rDrawingRes, rResources);
    });

    rBuilder.task()
        .name       ("Clean up NamedMeshes mesh and texture owners")
        .trigger_on ({tgScn.cleanup})
        .fulfills   ({})
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
