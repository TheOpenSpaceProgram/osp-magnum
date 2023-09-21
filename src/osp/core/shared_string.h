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
#ifndef INCLUDED_OSP_SHARED_STRING_H_56F463BF_C8A5_4633_B906_D7250C06E2DB
#define INCLUDED_OSP_SHARED_STRING_H_56F463BF_C8A5_4633_B906_D7250C06E2DB
#pragma once

#include "string_concat.h"  // for string_data(), string_size()

#include <memory>           // for std::shared_ptr
#include <utility>          // for std::forward
#include <stdexcept>        // for std::invalid_argument
#include <functional>       // for std::hash
#include <string_view>      // for std::basic_string_view
#include <type_traits>      // for std::is_nothrow_default_constructible_v, etc

#include <cstddef>          // for std::size_t

namespace osp
{

/**
 * A class representing a read-only string with shared ownership of the underlying storage for the string.
 * The interface is a std::string_view, and the lifetime management is conducted through move construction / assignment
 * of the template variable LIFETIME_T.
 *
 * The intended usage of this class is for strings that are read-only, but not compile-time constants.
 * For example, data read out of a configuration file, or from the network, that needs to have a long lifetime
 * but which is never modified after initial creation.
 *
 * In either of those situations, using BasicSharedString provides a single allocation for the lifetime of the
 * string data, while still providing the full interface of string_view, as well as relatively cheap copy operations
 * (depending on the specific LIFETIME_T parameter, of course).
 *
 * A very nieve implementation is to provide std::any as the lifetime management. This is undesirable because of the additional
 * pointer indirection that std::any incurs because of it's type erasure.
 *
 * The next most obvious implementation is to use std::shared_ptr<const char[]> for lifetime, which results in BasicSharedString
 * having a size of 4*sizeof(void*) (2 for the std::string_view, 2 for the shared_ptr). That's the implementation provided by the
 * using SharedString = ...; line below the template class declaration.
 *
 * A more sophisticated implementation could use intrusive reference counting for the string data, instead of a seperately
 * allocated and tracked management block like std::shared_ptr uses, and this interface also would allow for a choice of
 * atomic reference counting, or non-atomic, depending on the template parameters.
 *
 * Note also that the create() functions provide a mechanism to create a SharedString that
 * does not attempt lifetime management (e.g. default constructs m_lifetime). This is provided for two purposes.
 * 1. compile-time string literals being passed to a function that take "ownership" of the data passed in,
 *    but the string literal requires no ownership because its lifetime is a superset of the lifetime of the program
 *    in such case, the extra cost of copying / moving the m_lifetime variable can be omitted.
 * 2. Situations where the use of SharedString is required due to type system constraints, but the caller of the code
 *    knows for certain that the ownership of the passed in SharedString will never outlive the lifetime of the function call
 *    (e.g. the function never copies the SharedString, only references it). Situations with this usage are rare, but do happen
 *    so the SharedString::create_reference() function provides a way to do that.
 */
template<typename CHAR_T, typename LIFETIME_T>
class BasicSharedString : public std::basic_string_view<CHAR_T>
{
    using ThisType_t = BasicSharedString;
    using ViewBase_t = std::basic_string_view<CHAR_T>;

    static constexpr auto scm_noExceptSubStr = noexcept(std::declval<ViewBase_t>().substr(0u, ViewBase_t::npos));

    static_assert(std::is_nothrow_default_constructible_v<ViewBase_t>);
    static_assert(std::is_nothrow_move_constructible_v<ViewBase_t>);
    static_assert(std::is_nothrow_copy_constructible_v<ViewBase_t>);
    static_assert(std::is_nothrow_move_assignable_v<ViewBase_t>);
    static_assert(std::is_nothrow_copy_assignable_v<ViewBase_t>);
    static_assert(std::is_nothrow_destructible_v<ViewBase_t>);

public:
    using traits_type            = typename ViewBase_t::traits_type;
    using value_type             = typename ViewBase_t::value_type;
    using pointer                = typename ViewBase_t::pointer;
    using const_pointer          = typename ViewBase_t::const_pointer;
    using reference              = typename ViewBase_t::reference;
    using const_reference        = typename ViewBase_t::const_reference;
    using const_iterator         = typename ViewBase_t::const_iterator;
    using iterator               = typename ViewBase_t::iterator;
    using const_reverse_iterator = typename ViewBase_t::const_reverse_iterator;
    using reverse_iterator       = typename ViewBase_t::reverse_iterator;
    using size_type              = typename ViewBase_t::size_type;
    using difference_type        = typename ViewBase_t::difference_type;

    constexpr BasicSharedString(void) noexcept( std::is_nothrow_default_constructible_v<LIFETIME_T> ) = default;
    constexpr BasicSharedString(BasicSharedString &&) noexcept( std::is_nothrow_move_constructible_v<LIFETIME_T> ) = default;
    constexpr BasicSharedString(BasicSharedString const&) noexcept( std::is_nothrow_copy_constructible_v<LIFETIME_T> ) = default;
    constexpr BasicSharedString& operator=(BasicSharedString &&) noexcept( std::is_nothrow_move_assignable_v<LIFETIME_T> ) = default;
    constexpr BasicSharedString& operator=(BasicSharedString const&) noexcept( std::is_nothrow_copy_assignable_v<LIFETIME_T> ) = default;
    ~BasicSharedString() noexcept( std::is_nothrow_destructible_v<LIFETIME_T> ) = default;

    /**
     * @brief substr -- see std::string_view::substr
     * @param pos
     * @param count
     * @return a new BasicSharedString that has a copy of the m_lifetime variable to preserve the lifetime semantics of the object.
     */
    constexpr BasicSharedString substr(size_type pos = 0, size_type count = ViewBase_t::npos) const&
        noexcept(scm_noExceptSubStr && std::is_nothrow_copy_constructible_v<LIFETIME_T>);

    /**
     * @brief substr -- see std::string_view::substr
     * @param pos
     * @param count
     * @return a new BasicSharedString that has the m_lifetime variable of this BasicSharedString std::moved() into it.
     * This allows for a BasicSharedString that is an rvalue reference to have substr called on it, and avoid copying the lifetime.
     */
    constexpr BasicSharedString substr(size_type pos = 0, size_type count = ViewBase_t::npos) &&
        noexcept(scm_noExceptSubStr && std::is_nothrow_move_constructible_v<LIFETIME_T>);

    /**
     * @brief operator std::basic_string<CHAR_T>
     * Convenience function for getting an std::basic_string<CHAR_T> from this BasicSharedString
     */
    explicit constexpr operator std::basic_string<CHAR_T>()
        noexcept( noexcept( std::basic_string<CHAR_T>(ViewBase_t::data(), ViewBase_t::size()) ) );

    /**
     * @brief Constructs a new SharedString with newly allocated storage initialized with the provided data.
     */
    template<typename IT_T>
    static BasicSharedString create(IT_T&&, IT_T&&) noexcept(false); // allocates

    /**
     * @brief Constructs a new SharedString with newly allocated storage initialized with the provided data.
     */
    static BasicSharedString create(const_pointer data, size_type len) noexcept(false); // allocates

    /**
     * @brief Constructs a new SharedString with newly allocated storage initialized with the provided data.
     */
    static BasicSharedString create(ViewBase_t) noexcept(false); // allocates

    /**
     * @brief Constructs a SharedString with lifetime managed by the provided LIFETIME_T object.
     * The data pointed to by the first parameter is not copied, and must live until the end of the provided LIFETIME_T object.
     */
    static constexpr BasicSharedString create(ViewBase_t, LIFETIME_T)
        noexcept( std::is_nothrow_move_constructible_v<LIFETIME_T> );

    /**
     * @brief create_from_parts
     * @param strs
     * @brief Constructs a new SharedString with newly allocated storage initialized with the provided data.
     */
    template<typename ... STRS_T>
    static BasicSharedString create_from_parts(STRS_T && ... strs) noexcept(false); // allocates

    /**
     * @brief create_reference
     * @param view
     * @return a SharedString that does not attempt lifetime management at all.
     */
    static constexpr BasicSharedString create_reference(ViewBase_t)
        noexcept( std::is_nothrow_default_constructible_v<LIFETIME_T> );

#if defined(__cpp_impl_three_way_comparison)
    /**
     * @brief threeway comparison operator for shared strings. Only compares string data, not lifetime.
     */
    constexpr decltype(auto) operator<=>(BasicSharedString const& rhs)
    {
        return ViewBase_t(*this) <=> ViewBase_t(rhs);
    }

    /**
     * @brief threeway comparison operator for shared strings. Only compares string data, not lifetime.
     */
    constexpr decltype(auto) operator<=>(ViewBase_t const& rhs)
    {
        return ViewBase_t(*this) <=> rhs;
    }
#endif // defined(__cpp_impl_three_way_comparison)

    /**
     * @brief Equality operator for shared strings. Only compares string data, not lifetime.
     */
    constexpr bool operator==(BasicSharedString const& rhs)
    {
        return ViewBase_t(*this) == ViewBase_t(rhs);
    }

    /**
     * @brief Equality operator for shared strings. Only compares string data, not lifetime.
     */
    constexpr bool operator==(ViewBase_t const& rhs)
    {
        return ViewBase_t(*this) == rhs;
    }

protected:
    constexpr BasicSharedString(ViewBase_t view) noexcept( std::is_nothrow_default_constructible_v<LIFETIME_T> )
     : ViewBase_t{ view }
    { }

    constexpr BasicSharedString(ViewBase_t view, LIFETIME_T lifetime) noexcept( std::is_nothrow_move_constructible_v<LIFETIME_T> )
     : ViewBase_t{ view }
     , m_lifetime{ std::move(lifetime) }
    { }

private:
    [[no_unique_address]] LIFETIME_T m_lifetime;
}; // class BasicSharedString

//-----------------------------------------------------------------------------

#if defined(__cpp_lib_shared_ptr_arrays) && 201707 <= __cpp_lib_shared_ptr_arrays
using SharedStringLifetime_t = std::shared_ptr<const char[]>;
#else
// missing std::shared_ptr<T[]>...
using SharedStringLifetime_t = std::shared_ptr<const char>;
#endif

/**
 * The concrete implementation.
 */
using SharedString = BasicSharedString<char, SharedStringLifetime_t>;

//-----------------------------------------------------------------------------
// Inlines for BasicSharedString<>
//-----------------------------------------------------------------------------

template<typename CHAR_T, typename LIFETIME_T>
constexpr auto BasicSharedString<CHAR_T, LIFETIME_T>::substr(size_type const pos, size_type const count) const&
    noexcept(scm_noExceptSubStr && std::is_nothrow_copy_constructible_v<LIFETIME_T>)
    -> BasicSharedString
{
    return { ViewBase_t::substr(pos, count), m_lifetime };
}

template<typename CHAR_T, typename LIFETIME_T>
constexpr auto BasicSharedString<CHAR_T, LIFETIME_T>::substr(size_type const pos, size_type const count) &&
    noexcept(scm_noExceptSubStr && std::is_nothrow_move_constructible_v<LIFETIME_T>)
    -> BasicSharedString
{
    return { ViewBase_t::substr(pos, count), std::move(m_lifetime) };
}

template<typename CHAR_T, typename LIFETIME_T>
constexpr BasicSharedString<CHAR_T, LIFETIME_T>::operator std::basic_string<CHAR_T>()
    noexcept( noexcept( std::basic_string<CHAR_T>(ViewBase_t::data(), ViewBase_t::size()) ) )
{
    return { ViewBase_t::data(), ViewBase_t::size() };
}

// TODO: This isn't super generic. Using std::shared_ptr regardless of the LIFETIME_T parameter
// means that alternative LIFETIME_T params will probably encounter compile failures.
template<typename CHAR_T, typename LIFETIME_T>
  template<typename IT_T>
inline auto BasicSharedString<CHAR_T, LIFETIME_T>::create(IT_T && begin, IT_T && end) noexcept(false) // allocates
    -> BasicSharedString
{
    // Determine size
    using distance_t = typename std::iterator_traits<IT_T>::difference_type;
    distance_t const len = std::distance(begin, end);
    if constexpr(std::is_signed_v<distance_t>)
    {
        // One could argue that it's not necessary to detect this and throw
        // as the std::copy() below requires end to be reachable from begin,
        // and thus passing in invalid iterators is a precondition violation
        // but we don't skip the check, so as to provide meaningful errors.
        if(len < 0)
        {
            throw std::invalid_argument("End must be reachable from begin.");
        }
    }

    size_type const size = static_cast<size_type>(len);

    if(size == 0u)
    {
        // No data to copy, return empty SharedString
        return {};
    }

    // Make space to copy the string
#if defined(__cpp_lib_smart_ptr_for_overwrite)
    auto buf = std::make_shared_for_overwrite<CHAR_T[]>(size);
#elif defined(__cpp_lib_shared_ptr_arrays) && 201707 <= __cpp_lib_shared_ptr_arrays
    // avoid initilization of allocated array...
    std::shared_ptr<CHAR_T[]> buf{new CHAR_T[size]};
#else
    std::shared_ptr<CHAR_T> buf(new CHAR_T[size]);
#endif

    // Do the copy
    std::copy_n(std::forward<IT_T>(begin), size, buf.get());

    // Construct prior to SharedString constructor to avoid
    // ABI specific parameter passing order problems.
    ViewBase_t view{ buf.get(), size };

    // Make the SharedString
    return create(std::move(view), std::move(buf));
}

template<typename CHAR_T, typename LIFETIME_T>
  template<typename ... STRS_T>
inline auto BasicSharedString<CHAR_T, LIFETIME_T>::create_from_parts(STRS_T && ... strs) noexcept(false) // allocates
    -> BasicSharedString
{
    size_type const size = static_cast<size_type>( ( 0u + ... + string_size(strs) ) );

    if(size == 0u)
    {
        // No data to copy, return empty SharedString
        return {};
    }

    // Make space to copy the string
#if defined(__cpp_lib_smart_ptr_for_overwrite)
    auto buf = std::make_shared_for_overwrite<CHAR_T[]>(size);
#elif defined(__cpp_lib_shared_ptr_arrays) && 201707 <= __cpp_lib_shared_ptr_arrays
    // avoid initilization of allocated array...
    std::shared_ptr<CHAR_T[]> buf{new CHAR_T[size]};
#else
    std::shared_ptr<CHAR_T> buf(new CHAR_T[size]);
#endif

    {
        auto * p = buf.get();

        // Do the copy
        // C++17 fold for function calls.
        ( (p = std::copy_n(string_data(strs), string_size(strs), p)), ... );
    }

    // Construct prior to SharedString constructor to avoid
    // ABI specific parameter passing order problems.
    ViewBase_t view{ buf.get(), size };

    // Make the SharedString
    return create(std::move(view), std::move(buf));
}

template<typename CHAR_T, typename LIFETIME_T>
inline auto BasicSharedString<CHAR_T, LIFETIME_T>::create(const_pointer const data, size_type const len) noexcept(false) // allocates
    -> BasicSharedString
{
    // Reuse iterator factory function.
    return create(data, data+len);
}

template<typename CHAR_T, typename LIFETIME_T>
inline auto BasicSharedString<CHAR_T, LIFETIME_T>::create(ViewBase_t const view) noexcept(false) // allocates
    -> BasicSharedString
{
    // Reuse iterator factory function.
    return create(view.begin(), view.end());
}

template<typename CHAR_T, typename LIFETIME_T>
constexpr auto BasicSharedString<CHAR_T, LIFETIME_T>::create(ViewBase_t const view, LIFETIME_T buf)
    noexcept( std::is_nothrow_move_constructible_v<LIFETIME_T> )
    -> BasicSharedString
{
    return { view, std::move(buf) };
}

template<typename CHAR_T, typename LIFETIME_T>
constexpr auto BasicSharedString<CHAR_T, LIFETIME_T>::create_reference(ViewBase_t const view)
    noexcept( std::is_nothrow_default_constructible_v<LIFETIME_T> )
    -> BasicSharedString
{
    return { view };
}

} // namespace osp

// custom specialization of std::hash can be injected in namespace std
// Inherit from the std::basic_string_view std::hash implementation.
// This is needed to support using osp::BasicSharedString as the
// key type in a hashmap.
template<typename CHAR_T, typename LIFETIME_T>
struct std::hash<osp::BasicSharedString<CHAR_T, LIFETIME_T>>
{
    using ViewType_t = std::basic_string_view<CHAR_T>;

    using is_transparent = std::true_type;
    decltype(auto) operator()(ViewType_t const s) const noexcept
    {
        return std::hash<ViewType_t>{}(s);
    }
};

#endif // defined INCLUDED_OSP_SHARED_STRING_H_56F463BF_C8A5_4633_B906_D7250C06E2DB
