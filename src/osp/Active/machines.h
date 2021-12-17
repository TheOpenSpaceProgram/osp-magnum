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

#include "activetypes.h"
#include "../Resource/machines.h"

#include <cstdint>
#include <vector>

namespace osp::active
{

enum class MachineEnt : entt::id_type { };

/**
 * This component is added to a part, and stores a vector that keeps track of
 * all the Machines it uses. Machines are stored in multiple entities, so the
 * vector keeps track of these.
 */
struct ACompMachines
{
    std::vector<ActiveEnt> m_machines;
};

}

// Specialize entt::storage_traits to disable signals for storage that uses
// MachineEnt as entities
template<typename Type>
struct entt::storage_traits<osp::active::MachineEnt, Type>
{
    using storage_type = basic_storage<osp::active::MachineEnt, Type>;
};

namespace osp::active
{

template<typename COMP_T>
using mcomp_storage_t = typename entt::storage_traits<MachineEnt, COMP_T>::storage_type;

template<typename... COMP_T>
using mcomp_view_t = entt::basic_view<MachineEnt, entt::exclude_t<>, COMP_T...>;

} // namespace osp::active
