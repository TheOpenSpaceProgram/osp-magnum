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
#pragma once

#include "active_ent.h"

#include "../core/keyed_vector.h"
#include "../core/math_types.h"
#include "../core/storage.h"

#include <longeron/id_management/null.hpp>
#include <longeron/id_management/registry_stl.hpp>
#include <longeron/id_management/id_set_stl.hpp> 

#include <string>

namespace osp::active
{

using ActiveEntVec_t = std::vector<ActiveEnt>;
using ActiveEntSet_t = lgrn::IdSetStl<ActiveEnt>;

/**
 * @brief Component for transformation (in meters)
 */
struct ACompTransform
{
    osp::Matrix4 m_transform;
};

/**
 * @brief Simple name component
 */
struct ACompName
{
    std::string m_name;
};

using TreePos_t = uint32_t;

struct ACtxSceneGraph
{
    // N-ary tree structure represented as an array of descendant counts. Each node's subtree of
    // descendants is positioned directly after it within the array.
    // Example for tree structure "A(  B(C(D)), E(F(G(H,I)))  )"
    // * Descendant Count array: [A:8, B:2, C:1, D:0, E:4, F:3, G:2, H:0, I:0]
    osp::KeyedVec<TreePos_t, ActiveEnt>  m_treeToEnt{{lgrn::id_null<ActiveEnt>()}};
    osp::KeyedVec<TreePos_t, uint32_t>   m_treeDescendants{std::initializer_list<uint32_t>{0}};

    osp::KeyedVec<ActiveEnt, ActiveEnt>  m_entParent;
    osp::KeyedVec<ActiveEnt, TreePos_t>  m_entToTreePos;

    std::vector<TreePos_t>  m_delete;

    void resize(std::size_t ents)
    {
        m_treeToEnt         .reserve(ents);
        m_treeDescendants   .reserve(ents);
        m_entParent         .resize(ents);
        m_entToTreePos      .resize(ents, lgrn::id_null<TreePos_t>());
    }
};

using ACompTransformStorage_t = Storage_t<ActiveEnt, ACompTransform>;

/**
 * @brief Storage for basic components
 */
struct ACtxBasic
{
    lgrn::IdRegistryStl<ActiveEnt>      m_activeIds;

    ACtxSceneGraph                      m_scnGraph;
    ACompTransformStorage_t             m_transform;
};

template<typename IT_T>
void update_delete_basic(ACtxBasic &rCtxBasic, IT_T first, IT_T const& last)
{
    while (first != last)
    {
        ActiveEnt const ent = *first;

        if (rCtxBasic.m_transform.contains(ent))
        {
            rCtxBasic.m_transform           .remove(ent);

        }

        std::advance(first, 1);
    }
}

} // namespace osp::active
