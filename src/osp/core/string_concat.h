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
#ifndef INCLUDED_OSP_STRING_CONCAT_H_C0CC2473_002D_406E_99B6_E233565EAA1E
#define INCLUDED_OSP_STRING_CONCAT_H_C0CC2473_002D_406E_99B6_E233565EAA1E
#pragma once

#include <string>       // for std::string
#include <iterator>     // for std::data(), std::size()
#include <algorithm>    // for std::copy_n()
#include <type_traits>  // for std::is_pointer<>, std::is_array<>

#include <cstddef>      // for std::size_t

namespace osp
{

/**
 * Gets a pointer to the underlying data of a string type object
 *
 * If the parameter is a char *, or a char[], then this
 * function simply returns the parameter.
 *
 * If the parameter is neither a char* or char[], then return
 * the result of calling std::data() on the parameter.
 *
 * The returned pointer provides no guarantee on the string being nul
 * terminated. Always use string_data() with string_size() to ensure
 * no out of bounds data access occurs.
 *
 * Note that it's important that the str parameter not be taken as const&,
 * but instead as a forwarding/universal reference. If the parameter were const&
 * then std::data() would return a const pointer, instead of a pointer that's
 * const conditionally based on whether the passed in object is const.
 */
template<class STRING_T>
constexpr auto string_data(STRING_T && str)
{
    using Type_t = std::remove_reference_t<STRING_T>;
    if constexpr(std::is_pointer_v<Type_t>)
    {
        return std::forward<STRING_T>(str);
    }
    else
    {
        return std::data(std::forward<STRING_T>(str));
    }
} // string_data()


//-----------------------------------------------------------------------------

/**
 * Gets the size of the string provided as the parameter
 *
 * If the parameter is a char *, or a char[], then this
 * function calls the string length function, and therefore
 * the provided string must be nul-terminated.
 *
 * (Future enhancement will allow the size of char[] parameters
 *  to be statically determined at compile time)
 *
 * If the parameter is neither a char* or char[], then return
 * the result of calling std::size().
 */
template<class STRING_T>
constexpr std::size_t string_size(STRING_T && str)
{
    using Type_t = std::remove_reference_t<STRING_T>;
    if constexpr(std::is_pointer_v<Type_t>)
    {
        return std::char_traits<std::remove_pointer_t<STRING_T>>::length(str);
    }
    else if constexpr(std::is_array_v<Type_t>)
    {
        // While we could deduce the length of the string based only on the static size of the array
        // that wouldn't work for arrays that have embedded nul-terminators, which would then be
        // calculated as having a length longer than other string tools like std::string_view would use.
        // TODO: However, we can detect that discrepancy and assert, in debug builds, to catch mistakes.
        return std::char_traits<std::remove_pointer_t<std::decay_t<STRING_T>>>::length(str);
    }
    else
    {
        return std::size(std::forward<STRING_T>(str));
    }
} // string_size()


//-----------------------------------------------------------------------------

/**
 * Effecienctly appends multiple strings together using, at most, a single
 * allocation to reserve the necessary space, via the "resize" function call.
 *
 * We'd be able to get slightly improved perf when P1072 resize_and_overwrite() is available.
 *
 * \arg dest   -- The destination where all of the appended strings will be stored
 * \arg others -- The strings to append into the destination.
 *
 * \returns Nothing.
 */
template<typename DESTINATION_T, typename ... STRS_T>
constexpr void string_append(DESTINATION_T & dest, STRS_T const& ... others)
{
    auto const curSize   = string_size(dest);
    auto const totalSize = ( curSize + ... + string_size(others) );

#if defined(__cpp_lib_string_resize_and_overwrite)

    dest.resize_and_overwrite(
        totalSize,
        [&](char* pData, int n)
        {
			pData += curSize;

            // C++17 fold for function calls.
            ( ( pData = std::copy_n( string_data(others),
                                     string_size(others),
                                     pData ) ), ... );
            return n;
        });

#else

    // C++17 fold for summation
    dest.resize( ( curSize + ... + string_size(others) ) );

    auto * p = string_data(dest) + curSize;

    // C++17 fold for function calls.
    ( (p = std::copy_n(string_data(others), string_size(others), p)), ... );

#endif

} // string_append()


//-----------------------------------------------------------------------------

/**
 * Concatenates all of the provided string-like objects into a single
 * string of the type RESULT_T. RESULT_T must have resize() and append()
 * functions.
 *
 * This operation uses a single allocation to acquire storage, via the "resize" function call.
 *
 * We'd be able to get slightly improved perf when P1072 resize_and_overwrite() is available.
 *
 * \arg string -- the strings to concatinate together.
 *
 * \returns A RESULT_T object containing the strings concatenated together.
 */
template<typename RESULT_T, typename ... STRS_T>
constexpr RESULT_T basic_string_concat(STRS_T const& ... others)
{
    RESULT_T dest;
    string_append(dest, others...);
    return dest;
} // basic_string_concat()


//-----------------------------------------------------------------------------

/**
 * Concatenates all of the provided string-like objects into a single std::string.
 *
 * This operation uses a single allocation to acquire storage, via the "resize" function call.
 *
 * \arg string -- the strings to concatinate together.
 *
 * \returns A std::string object containing the strings concatenated together.
 */
template<typename ... STRS_T>
constexpr auto string_concat(STRS_T const& ... strs)
{
    return basic_string_concat<std::string>(strs...);
} // string_concat()

} // namespace osp

#endif // defined INCLUDED_OSP_STRING_CONCAT_H_C0CC2473_002D_406E_99B6_E233565EAA1E
