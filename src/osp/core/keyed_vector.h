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

#include <vector>

#include <longeron/utility/enum_traits.hpp>

namespace osp
{

/**
 * @brief Wraps an std::vector intended to be accessed using a (strong typedef) enum class ID
 */
template <typename ID_T, typename DATA_T, typename ALLOC_T = std::allocator<DATA_T>>
class KeyedVec : public std::vector<DATA_T, ALLOC_T>
{
    using vector_t  = std::vector<DATA_T, ALLOC_T>;
    using int_t     = lgrn::underlying_int_type_t<ID_T>;

public:

    using value_type                = typename vector_t::value_type;
    using allocator_type            = typename vector_t::allocator_type;
    using reference                 = typename vector_t::reference;
    using const_reference           = typename vector_t::const_reference;
    using pointer                   = typename vector_t::pointer;
    using const_pointer             = typename vector_t::const_pointer;
    using iterator                  = typename vector_t::iterator;
    using const_iterator            = typename vector_t::const_iterator;
    using reverse_iterator          = typename vector_t::reverse_iterator;
    using const_reverse_iterator    = typename vector_t::const_reverse_iterator;
    using difference_type           = typename vector_t::difference_type;
    using size_type                 = typename vector_t::size_type;

    constexpr vector_t& base() noexcept { return *this; }
    constexpr vector_t const& base() const noexcept { return *this; }

    reference at(ID_T const id)
    {
        return vector_t::at(std::size_t(id));
    }

    const_reference at(ID_T const id) const
    {
        return vector_t::at(std::size_t(id));
    }

    reference operator[] (ID_T const id)
    {
        return vector_t::operator[](std::size_t(id));
    }

    const_reference operator[] (ID_T const id) const
    {
        return vector_t::operator[](std::size_t(id));
    }

}; // class KeyedVec



} // namespace osp
