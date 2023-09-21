/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include <entt/entity/view.hpp>       // for basic_view
#include <entt/entity/storage.hpp>    // for entt::basic_storage
// IWYU pragma: end_exports

#include <optional>

namespace osp
{

template<typename ENT_T, typename COMP_T>
using Storage_t = typename entt::basic_storage<COMP_T, ENT_T>;

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

} // namespace osp
