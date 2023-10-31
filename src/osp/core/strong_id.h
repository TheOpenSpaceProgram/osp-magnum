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

#include <longeron/id_management/null.hpp>

#include <concepts>
#include <typeindex> // light-ish header that happens to include std::hash

#include <cstdint>

namespace osp
{

/**
 * @brief Integer wrapper intended for identifiers usable as container indices/keys
 *
 * Default-initialized to null (max int value)
 *
 * @tparam INT_T    Wrapped integer type, usually unsigned
 * @tparam DUMMY_T  Dummy used to make separate unique types, avoids needing inheritance
 */
template <std::integral INT_T, typename DUMMY_T>
struct StrongId
{
    using entity_type = INT_T; // Name used for entt compatibility

    constexpr StrongId() noexcept = default;
    constexpr StrongId(StrongId const& copy) noexcept = default;
    constexpr StrongId& operator=(StrongId const& copy) noexcept = default;

    static StrongId from_index(std::size_t const index)
    {
        return StrongId{static_cast<INT_T>(index)};
    }

    constexpr explicit StrongId(INT_T const value) noexcept
     : value{value}
    { };

    constexpr explicit operator std::size_t() const noexcept
    {
        return value;
    }

    constexpr explicit operator INT_T() const noexcept
    {
        return value;
    }

    constexpr auto operator<=>(StrongId const&) const = default;

    INT_T value{ lgrn::id_null<StrongId>() };
};

} // namespace osp

// std::hash support for unordered containers
template <std::integral INT_T, typename DUMMY_T>
struct std::hash<osp::StrongId<INT_T, DUMMY_T>> {
    constexpr auto operator() (osp::StrongId<INT_T, DUMMY_T> const& key) const
    {
        return key.value;
    }
};

// Longeron++ underlying_int_type
template<typename TYPE_T>
  requires std::is_integral_v<typename TYPE_T::entity_type>
struct lgrn::underlying_int_type< TYPE_T >
{
    using type = typename TYPE_T::entity_type;
};

