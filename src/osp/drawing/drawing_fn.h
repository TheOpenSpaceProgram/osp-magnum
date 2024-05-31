/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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
#pragma once

#include "drawing.h"

#include "../activescene/basic.h"
#include "../activescene/basic_fn.h"

namespace osp::draw
{

/**
 * @brief Function pointers called when new draw transforms are calculated
 *
 * Draw transforms (Matrix4) are calculated by traversing the Scene graph (tree of ActiveEnts).
 * These matrices are not always stored in memory since they're slightly expensive. By default,
 * they are only saved for DrawEnts associated with an ActiveEnt in ACtxSceneRender::activeToDraw.
 *
 * Draw transforms can be calculated by SysRender::update_draw_transforms, or potentially by a
 * future system that takes physics engine interpolation or animations into account.
 * DrawTfObservers provides a way to tap into this procedure to call custom functions for other
 * systems.
 *
 * To use, write into DrawTfObservers::observers[i]
 * Enable per-DrawEnt by setting ACtxSceneRender::drawTfObserverEnable[drawEnt] bit [i]
 *
 */
struct DrawTfObservers
{
    using UserData_t = std::array<void*, 7>;
    using Func_t = void(*)(ACtxSceneRender& rCtxScnRdr, Matrix4 const&, active::ActiveEnt, int, UserData_t) noexcept;

    struct Observer
    {
        Func_t      func{nullptr};
        UserData_t  data{};
    };

    std::array<Observer, 16> observers;
};

/**
 * @brief View and Projection matrix
 */
struct ViewProjMatrix
{
    ViewProjMatrix(Matrix4 const& view, Matrix4 const& proj)
     : m_viewProj{proj * view}
     , m_view{view}
     , m_proj{proj}
    { }

    Matrix4 m_viewProj;
    Matrix4 m_view;
    Matrix4 m_proj;
};

/**
 * @brief Stores a draw function and user data needed to draw a single entity
 */
struct EntityToDraw
{
    using UserData_t = std::array<void*, 4>;

    /**
     * @brief A function pointer to a Shader's draw() function
     *
     * @param ActiveEnt         [in] The entity being drawn
     * @param ViewProjMatrix    [in] View and projection matrix
     * @param UserData_t        [in] Non-owning user data
     */
    using ShaderDrawFnc_t = void (*)(
            DrawEnt, ViewProjMatrix const&, UserData_t) noexcept;

    ShaderDrawFnc_t draw;

    // Non-owning user data passed to draw function, such as the shader
    UserData_t data;

}; // struct EntityToDraw

/**
 * @brief Tracks a set of entities and their assigned drawing functions
 *
 * RenderGroups are intended to be associated with certain rendering techniques
 * like Forward, Deferred, and Shadow mapping.
 *
 * This also works with game-specific modes like Thermal Imaging.
 */
struct RenderGroup
{
    using DrawEnts_t = Storage_t<DrawEnt, EntityToDraw>;

    DrawEnts_t entities;

}; // struct RenderGroup

class SysRender
{
    struct UpdDrawTransformNoOp
    {
        constexpr void operator()(Matrix4 const&, active::ActiveEnt, int) const noexcept {}
    };

public:

    /**
     * @brief Attempt to create a scene mesh associated with a resource
     *
     * @param rCtxDrawing       [ref] Drawing data
     * @param rCtxDrawingRes    [ref] Resource drawing data
     * @param rResources        [ref] Application Resources containing meshes
     * @param resId             [in] Mesh Resource Id
     *
     * @return Id of new mesh, existing mesh Id if it already exists.
     */
    static MeshId own_mesh_resource(
            ACtxDrawing& rCtxDrawing,
            ACtxDrawingRes& rCtxDrawingRes,
            Resources& rResources,
            ResId resId);

    static TexId own_texture_resource(
            ACtxDrawing& rCtxDrawing,
            ACtxDrawingRes& rCtxDrawingRes,
            Resources& rResources,
            ResId resId);

    /**
     * @brief Remove all mesh and texture components, aware of refcounts
     */
    static void clear_owners(ACtxSceneRender& rCtxScnRdr, ACtxDrawing& rCtxDrawing);

    /**
     * @brief Dissociate resources from the scene's meshes and textures
     *
     * @param rCtxDrawingRes    [ref] Resource drawing data
     * @param rResources        [ref] Application Resources
     */
    static void clear_resource_owners(
            ACtxDrawingRes&                         rCtxDrawingRes,
            Resources&                              rResources);

    static inline void needs_draw_transforms(
            active::ACtxSceneGraph const&           scnGraph,
            active::ActiveEntSet_t&                 rNeedDrawTf,
            active::ActiveEnt                       ent);

    struct ArgsForUpdDrawTransform
    {
        active::ACtxSceneGraph const&               scnGraph;
        active::ACompTransformStorage_t const&      transforms;
        KeyedVec<active::ActiveEnt, DrawEnt> const& activeToDraw;
        active::ActiveEntSet_t const&               needDrawTf;
        DrawTransforms_t&                           rDrawTf;
    };

    template<typename IT_T, typename ITB_T, typename FUNC_T = UpdDrawTransformNoOp>
    static void update_draw_transforms(
            ArgsForUpdDrawTransform     args,
            IT_T                        first,
            ITB_T const&                last,
            FUNC_T                      func = {});

    template<typename IT_T>
    static void update_delete_drawing(
            ACtxSceneRender& rCtxScnRdr, ACtxDrawing& rCtxDrawing, IT_T const& first, IT_T const& last);

    static MeshIdOwner_t add_drawable_mesh(ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources, PkgId const pkg, std::string_view const name);

    static constexpr decltype(auto) gen_drawable_mesh_adder(ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources, PkgId const pkg);

private:

    template<typename FUNC_T>
    static void update_draw_transforms_recurse(
            ArgsForUpdDrawTransform     args,
            active::ActiveEnt           ent,
            Matrix4 const&              parentTf,
            int                         depth,
            FUNC_T&                     func);

}; // class SysRender

void SysRender::needs_draw_transforms(
        active::ACtxSceneGraph const&   scnGraph,
        active::ActiveEntSet_t&         rNeedDrawTf,
        active::ActiveEnt               ent)
{
    rNeedDrawTf.insert(ent);

    active::ActiveEnt const parentEnt = scnGraph.m_entParent[ent];

    if (   parentEnt != lgrn::id_null<active::ActiveEnt>()
        && ! rNeedDrawTf.contains(parentEnt) )
    {
        SysRender::needs_draw_transforms(scnGraph, rNeedDrawTf, parentEnt);
    }
}

template<typename IT_T, typename ITB_T, typename FUNC_T>
void SysRender::update_draw_transforms(
        ArgsForUpdDrawTransform     args,
        IT_T                        first,
        ITB_T const&                last,
        FUNC_T                      func)
{
    static constexpr Matrix4 const identity{};

    while (first != last)
    {
        active::ActiveEnt const ent = *first;

        if (args.needDrawTf.contains(ent))
        {
            update_draw_transforms_recurse(args, ent, identity, true, func);
        }

        std::advance(first, 1);
    }
}

template<typename FUNC_T>
void SysRender::update_draw_transforms_recurse(
        ArgsForUpdDrawTransform     args,
        active::ActiveEnt           ent,
        Matrix4 const&              parentTf,
        int                         depth,
        FUNC_T&                     func)
{
    using namespace osp::active;

    Matrix4 const& entTf        = args.transforms.get(ent).m_transform;
    Matrix4 const& entDrawTf    = (depth == 0) ? (entTf) : (parentTf * entTf);

    func(entDrawTf, ent, depth);

    DrawEnt const drawEnt = args.activeToDraw[ent];
    if (drawEnt != lgrn::id_null<DrawEnt>())
    {
        args.rDrawTf[drawEnt] = entDrawTf;
    }

    for (ActiveEnt entChild : SysSceneGraph::children(args.scnGraph, ent))
    {
        if (args.needDrawTf.contains(entChild))
        {
            update_draw_transforms_recurse(args, entChild, entDrawTf, depth + 1, func);
        }
    }
}


template<typename STORAGE_T, typename REFCOUNT_T>
void remove_refcounted(
        DrawEnt const ent, STORAGE_T &rStorage, REFCOUNT_T &rRefcount)
{
    auto &rOwner = rStorage[ent];
    if (rOwner.has_value())
    {
        rRefcount.ref_release(std::move(rOwner));
    }
}

template<typename IT_T>
void SysRender::update_delete_drawing(
        ACtxSceneRender& rCtxScnRdr, ACtxDrawing& rCtxDrawing, IT_T const& first, IT_T const& last)
{
    for (auto it = first; it != last; std::advance(it, 1))
    {
        DrawEnt const drawEnt = *it;

        remove_refcounted(drawEnt, rCtxScnRdr.m_diffuseTex, rCtxDrawing.m_texRefCounts);
        remove_refcounted(drawEnt, rCtxScnRdr.m_mesh,       rCtxDrawing.m_meshRefCounts);
    }
}

constexpr decltype(auto) SysRender::gen_drawable_mesh_adder(ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, Resources& rResources, PkgId const pkg)
{
    return [&rDrawing, &rDrawingRes, &rResources, pkg] (std::string_view const name) -> MeshIdOwner_t
    {
        return add_drawable_mesh(rDrawing, rDrawingRes, rResources, pkg, name);
    };
}

} // namespace osp::draw
