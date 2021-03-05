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

#include <assert.h>
#include <map>
#include <optional>
#include <string>

namespace osp
{

struct construct_tag {};
struct reserve_tag {};

template <class TYPE_T>
struct Resource
{
    Resource(reserve_tag)
     : m_data()
    { }
    Resource(construct_tag)
     : m_data(TYPE_T())
    { }
    Resource(construct_tag, TYPE_T&& data)
     : m_data(std::move(data))
    { }
    Resource(Resource&& move) = default;
    Resource(const Resource& copy) = delete;
    ~Resource()
    {
        assert(m_refCount == 0);
    }

    std::optional<TYPE_T> m_data;

    int m_refCount{0};
};

template <class TYPE_T>
class DependRes
{
public:
    DependRes() : m_it(), m_empty(true) {};

    DependRes(typename std::map<std::string, Resource<TYPE_T>>::iterator it) : m_it(it), m_empty(false)
    {
        m_it->second.m_refCount ++;
    }

    DependRes(DependRes<TYPE_T> const& copy) : m_it(copy.m_it), m_empty(copy.m_empty)
    {
        if (!m_empty)
        {
            m_it->second.m_refCount ++;
        }
    }

    ~DependRes()
    {
        if (!m_empty)
        {
            m_it->second.m_refCount --;
        }
    }

    DependRes<TYPE_T>& operator=(DependRes<TYPE_T>&& move)
    {
        if (!m_empty)
        {
            // Existing iterator is being overwritten, decrease ref count
            m_it->second.m_refCount--;
        }
        m_empty = move.m_empty;
        move.m_empty = true;

        m_it = std::move(move.m_it);

        return *this;
    }

    bool operator==(DependRes<TYPE_T> const& compare) const
    {
        return m_it == compare.m_it;
    }

    bool empty() const { return m_empty; }

    std::string name() const { return m_it->first; }

    constexpr TYPE_T const& operator*() const noexcept { return m_it->second.m_data.value(); }
    constexpr TYPE_T& operator*() noexcept { return m_it->second.m_data.value(); }

    constexpr TYPE_T const* operator->() const noexcept { return &m_it->second.m_data.value(); }
    constexpr TYPE_T* operator->() noexcept { return &m_it->second.m_data.value(); }
private:
    bool m_empty;
    typename std::map<std::string, Resource<TYPE_T>>::iterator m_it;
};


}
