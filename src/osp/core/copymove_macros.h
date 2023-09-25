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

#define OSP_MOVE_COPY_CTOR_ASSIGN(type)             \
    Type            (Type const& copy)  = default;  \
    Type            (Type&& move)       = default;  \
    Type& operator= (Type const& copy)  = default;  \
    Type& operator= (Type&& move)       = default;

#define OSP_MOVE_ONLY_CTOR_ASSIGN(Type)             \
    Type            (Type const& copy)  = delete;   \
    Type            (Type&& move)       = default;  \
    Type& operator= (Type const& copy)  = delete;   \
    Type& operator= (Type&& move)       = default;

#define OSP_MOVE_ONLY_CTOR(Type)             \
    Type            (Type const& copy)  = delete;   \
    Type            (Type&& move)       = default;

#define OSP_MOVE_COPY_CTOR_ASSIGN_CONSTEXPR_NOEXCEPT(Type)              \
    constexpr Type            (Type const& copy) noexcept = default;    \
    constexpr Type            (Type&& move)      noexcept = default;    \
    constexpr Type& operator= (Type const& copy) noexcept = default;    \
    constexpr Type& operator= (Type&& move)      noexcept = default;

#define OSP_MOVE_ONLY_CTOR_ASSIGN_CONSTEXPR_NOEXCEPT(Type)              \
    constexpr Type            (Type const& copy) noexcept = delete;     \
    constexpr Type            (Type&& move)      noexcept = default;    \
    constexpr Type& operator= (Type const& copy) noexcept = delete;     \
    constexpr Type& operator= (Type&& move)      noexcept = default;

#define OSP_MOVE_ONLY_CTOR_CONSTEXPR_NOEXCEPT(Type)                     \
    constexpr Type            (Type const& copy) noexcept = delete;     \
    constexpr Type            (Type&& move)      noexcept = default;
