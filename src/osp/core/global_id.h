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

#include <longeron/utility/enum_traits.hpp> // for lgrn::underlying_int_type_t

#include <type_traits> // for std::is_integral_v

namespace osp
{

template <typename ID_T, typename DUMMY_T = void>
struct GlobalIdReg
{
    using underlying_t = lgrn::underlying_int_type_t<ID_T>;
    static_assert(std::is_integral_v<underlying_t>);

    [[nodiscard]] static constexpr ID_T create() noexcept
    {
        return ID_T{sm_count++};
    }

    [[nodiscard]] static constexpr std::size_t size() noexcept
    {
      return std::size_t{sm_count};
    }

    [[nodiscard]] static constexpr underlying_t largest() noexcept
    {
      return sm_count;
    }

private:
    static inline constinit underlying_t sm_count{0};
};

} // namespace osp
