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

// IWYU pragma: begin_exports

#include <entt/core/fwd.hpp>          // for entt::id_type
#include <entt/entity/view.hpp>       // for basic_view
#include <entt/entity/storage.hpp>    // for entt::basic_storage
#include <entt/entity/sparse_set.hpp> // for entt::sparse_set

// IWYU pragma: end_exports

#include <longeron/id_management/registry_stl.hpp>      // for lgrn::IdRegistryStl
#include <longeron/containers/bit_view.hpp>             // for lgrn::BitView

#include <vector>

namespace osp::active
{

enum class ActiveEnt: entt::id_type {};

} // namespace osp::active


// Specialize entt::storage_traits to disable signals for storage that uses
// ActiveEnt as entities
template<typename Type>
struct entt::storage_traits<osp::active::ActiveEnt, Type>
{
    using storage_type = basic_storage<osp::active::ActiveEnt, Type>;
};


namespace osp::active
{


inline constexpr unsigned gc_heir_physics_level = 1;

using ActiveReg_t = lgrn::IdRegistryStl<osp::active::ActiveEnt>;

using EntVector_t = std::vector<ActiveEnt>;
using EntSet_t = lgrn::BitView< std::vector<uint64_t> >;

struct EntSetPair
{
    EntSet_t    &m_rEnts;
    EntVector_t &m_rDirty;
};

using active_sparse_set_t = entt::basic_sparse_set<ActiveEnt>;

template<typename COMP_T>
using acomp_storage_t = typename entt::storage_traits<ActiveEnt, COMP_T>::storage_type;

template<typename... COMP_T>
using acomp_view_t = entt::basic_view<ActiveEnt, entt::get_t<COMP_T...>, entt::exclude_t<>>;

} // namespace osp::active

