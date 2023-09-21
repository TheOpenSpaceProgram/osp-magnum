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

#include "../bitvector.h"   // for osp::BitVector_t

// IWYU pragma: begin_exports

#include <entt/core/fwd.hpp>          // for entt::id_type
#include <entt/entity/view.hpp>       // for basic_view
#include <entt/entity/storage.hpp>    // for entt::basic_storage
#include <entt/entity/sparse_set.hpp> // for entt::sparse_set

// IWYU pragma: end_exports

#include <longeron/id_management/registry_stl.hpp>      // for lgrn::IdRegistryStl
#include <longeron/containers/bit_view.hpp>             // for lgrn::BitView

#include <optional>
#include <vector>

namespace osp::active
{

enum class ActiveEnt: uint32_t { };

using ActiveEntVec_t = std::vector<ActiveEnt>;
using ActiveEntSet_t = BitVector_t;

using active_sparse_set_t = entt::basic_sparse_set<ActiveEnt>;

template<typename COMP_T>
using acomp_storage_t = typename entt::basic_storage<COMP_T, ActiveEnt>;

//-----------------------------------------------------------------------------

enum class DrawEnt : uint32_t { };

using DrawEntVec_t  = std::vector<DrawEnt>;
using DrawEntSet_t  = BitVector_t;


enum class MaterialId : uint32_t { };


/**
 * @brief Emplace, Reassign, or Remove a value from an entt::basic_storage
 */
template <typename COMP_T, typename ENT_T>
void storage_assign(entt::basic_storage<COMP_T, ENT_T> &rStorage, ENT_T const ent, std::optional<COMP_T> value)
{
    if (value.has_value())
    {
        if (rStorage.contains(ent))
        {
            rStorage.get(ent) = std::move(*value);
        }
        else
        {
            rStorage.emplace(ent, std::move(*value));
        }
    }
    else
    {
        rStorage.remove(ent); // checks contains(ent) internally
    }
}


} // namespace osp::active

