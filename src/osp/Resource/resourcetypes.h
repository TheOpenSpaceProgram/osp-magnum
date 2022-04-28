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

#include <longeron/id_management/storage.hpp>

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace osp
{

enum class ResTypeId : uint32_t { };
enum class ResId : uint32_t { };
enum class PkgId : uint32_t { };

template <typename T>
class ResourceContainer
{
public:
    // Required for std::is_copy_assignable to work properly inside of entt::any
    ResourceContainer() = default;
    ResourceContainer(ResourceContainer const& copy) = delete;
    ResourceContainer(ResourceContainer&& move) = default;

    template <typename ... ARGS_T>
    T& emplace(ResId id, ARGS_T&& ... args)
    {
        m_vec.resize(std::max(m_vec.size(), std::size_t(id) + 1));
        std::unique_ptr<T> &rPtr = m_vec[std::size_t(id)];
        assert(!rPtr); // Ensure pointer is empty
        rPtr = std::make_unique<T>(std::forward<ARGS_T>(args)...);
        return *rPtr;
    }

    T const* get(ResId id) const
    {
        if(m_vec.size() <= std::size_t(id))
        {
            return nullptr;
        }
        return m_vec[std::size_t(id)].get();
    }

    T* get(ResId id)
    {
        return const_cast<T*>(const_cast<const ResourceContainer*>(this)->get(id));
    }

    void remove(ResId id)
    {
        assert(m_vec.size() > std::size_t(id));
        m_vec[std::size_t(id)].reset();
    }

private:

    std::vector< std::unique_ptr<T> > m_vec;
};

template <typename T>
struct res_container;

template <typename T>
struct res_container
{
    using type = ResourceContainer<T>;
};

template <typename T>
using res_container_t = typename res_container<T>::type;

// Resource Owner

class Resources;

using ResIdOwner_t = lgrn::IdStorage<ResId, Resources>;


// Resource Type Ids

struct GenResTypeId
{
    friend inline ResTypeId resource_type_next() noexcept;
    friend inline int resource_type_count() noexcept;
private:
    static inline int count{0};
};

inline ResTypeId resource_type_next() noexcept
{
    return ResTypeId(GenResTypeId::count++);
}

inline int resource_type_count() noexcept
{
    return GenResTypeId::count;
}

namespace restypes
{

inline ResTypeId const gc_image         = resource_type_next();
inline ResTypeId const gc_texture       = resource_type_next();
inline ResTypeId const gc_mesh          = resource_type_next();
inline ResTypeId const gc_importer      = resource_type_next();

} // namespace restypes

struct PrefabPair
{
    ResIdOwner_t m_importer;
    int m_prefabId;
};

} // namespace osp
