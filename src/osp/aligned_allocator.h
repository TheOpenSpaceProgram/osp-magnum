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
#include <cstdlib>

namespace osp
{

/**
 * Allocates memory that is aligned on the specified boundary
 * 
 * MSVC's memory allocation scheme is incompatible with std::aligned_alloc
 * because it cannot free aligned memory with std::free. _aligned_malloc() is
 * used instead and is selected here based on conditional compilation for
 * windows platforms.
 * 
 * The allocated size must be a multiple of the alignment, and the alignment
 * must be a positive integer power of two
 * 
 * See https://en.cppreference.com/w/cpp/memory/c/aligned_alloc for details
 */
template <typename T, size_t ALIGNMENT>
struct AlignedAllocator
{
    typedef T value_type;

    static T* allocate(size_t size);
    static void deallocate(T* ptr, size_t size) noexcept;
};

template <typename T, size_t ALIGNMENT>
T* AlignedAllocator<T, ALIGNMENT>::allocate(size_t size)
{
    T* ptr = nullptr;
#ifdef _WIN64
    ptr = static_cast<T*>(_aligned_malloc(size, ALIGNMENT));
#else
    ptr = static_cast<T*>(std::aligned_alloc(ALIGNMENT, size));
#endif
    return ptr;
}

template <typename T, size_t ALIGNMENT>
void AlignedAllocator<T, ALIGNMENT>::deallocate(T* ptr, size_t size) noexcept
{
#ifdef _WIN64
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif // _WIN64
}

} // namespace osp
