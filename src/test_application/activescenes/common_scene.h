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
#pragma once

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>

//#include <entt/core/any.hpp>

#include <array>

namespace testapp
{

/**
 * @brief An array of entt::any intended for storing unique types
 */
struct MultiAny
{
    using Array_t = std::array<entt::any, 8>;

    Array_t m_data;

    template <typename T, typename ... ARGS_T>
    T& emplace(ARGS_T&& ... args);

    template <typename T>
    T& get();
};

template <typename T, typename ... ARGS_T>
T& MultiAny::emplace(ARGS_T&& ... args)
{
    Array_t::iterator found = std::find_if(std::begin(m_data), std::end(m_data),
                              [] (entt::any const &any)
    {
        return ! bool(any);
    });

    assert (found != std::end(m_data));

    found->emplace<T>(std::forward<ARGS_T>(args) ...);

    return entt::any_cast<T&>(*found);
}

template <typename T>
T& MultiAny::get()
{
    Array_t::iterator found = std::find_if(std::begin(m_data), std::end(m_data),
                              [] (entt::any const &any)
    {
        return any.type() == entt::type_id<T>();
    });

    assert (found != std::end(m_data));

    return entt::any_cast<T&>(*found);
}

//-----------------------------------------------------------------------------

struct CommonTestScene : MultiAny
{
    using on_cleanup_t = void(CommonTestScene&);
    using ActiveEnt = osp::active::ActiveEnt;

    CommonTestScene(osp::Resources &rResources)
     : m_pResources{&rResources}
    { }
    ~CommonTestScene();

    osp::Resources *m_pResources;

    std::vector<on_cleanup_t*> m_onCleanup;

    // ID registry generates entity IDs, and keeps track of which ones exist
    lgrn::IdRegistry<osp::active::ActiveEnt> m_activeIds;

    // Basic and Drawing components
    osp::active::ACtxBasic          m_basic;
    osp::active::ACtxDrawing        m_drawing;
    osp::active::ACtxDrawingRes     m_drawingRes;

    // Entity delete list/queue
    std::vector<ActiveEnt>          m_delete;
    std::vector<ActiveEnt>          m_deleteTotal;

    // Hierarchy root, needs to exist so all hierarchy entities are connected
    ActiveEnt                       m_hierRoot{lgrn::id_null<ActiveEnt>()};

    int     m_materialCount     {0};
    int     m_matCommon         {m_materialCount++};
    int     m_matVisualizer     {m_materialCount++};

    /**
     * @brief Delete descendents of entities to delete
     *
     * Reads m_delete, and populates m_deleteTotal with existing m_delete and
     * descendents added
     */
    void update_hierarchy_delete();

    /**
     * @brief Delete components of deleted entities in m_deleteTotal
     */
    void update_delete();

    /**
     * @brief Set all meshes, textures, and materials as dirty
     *
     * Intended to synchronize a newly added renderer
     */
    void set_all_dirty();
};


} // namespace testapp
