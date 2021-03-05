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

#include "Resource.h"

#include <entt/core/type_info.hpp>
#include <entt/core/family.hpp>
#include <entt/core/any.hpp>


#include <iostream>
#include <cstdint>
#include <map>
#include <memory>
#include <string>


namespace osp
{

// Just a string, but typedef'd to indicate that it represents a prefix
using ResPrefix_t = std::string;

// Stores a split path as <prefix, rest of path>
struct Path
{
    std::string_view prefix;
    std::string_view identifier;
};

using StrViewPair_t = std::pair<std::string_view, std::string_view>;

/**
 * Split a string_view at the first instance of a delimiter
 *
 */
StrViewPair_t decompose_str(std::string_view path, const char delim);

/**
 * Split a resource path into a prefix and identifier
 *
 * A path of the format "prefix:identifier" is divided into the prefix (any
 * text preceeding the first instance of the ':' character), and the following
 * identifier.
 *
 * @param path [in] The path to split
 * @return The path divided into a prefix and identifier
 */
Path decompose_path(std::string_view path);

/**
 * Stores maps of string to any type. A map is created for each new type added.
 * Stored  are referred to as Resources, and are reference counted.
 */
class Package
{
    // Used to generate sequential IDs at runtime
    using resource_id = entt::family<struct resource_type>;

public:

    template<class TYPE_T>
    using group_t =  std::map<std::string, osp::Resource<TYPE_T>, std::less<>>;

    Package(std::string prefix, std::string packageName);
    Package(Package&& move) = default;
    Package(const Package& copy) = delete;
    Package& operator=(Package const& copy) = delete;

    /**
     * Initialize and add a resource to store in this package.
     *
     * @tparam TYPE_T   Type of resource to store
     *
     * @param path [in] String used to identify this resource
     * @param args [in] Arguments to pass to TYPE_T constructor
     * @return Reference counted dependency to new resource
     */
    template<class TYPE_T, typename ... ARGS_T>
    DependRes<TYPE_T> add(std::string_view path, ARGS_T&& ... args);

    /**
     * Get a resource by path identifier
     *
     * @tparam TYPE_T   Type of resource to get
     *
     * @param path [in] String used to identify the target resource
     * @return Reference counted dependency to the found resource; empty if not
     *         found.
     */
    template<class TYPE_T>
    DependRes<TYPE_T> get(std::string_view path) noexcept;

    /**
     * Get a resource by path identifier. If it isn't found, then reserve the
     * path to be loaded later.
     *
     * @tparam TYPE_T Type of resource to get
     *
     * @param path [in] String used to identify the target resource
     * @return Reference counted dependency to the found or unloaded resource
     */
    template<class TYPE_T>
    DependRes<TYPE_T> get_or_reserve(std::string_view path) noexcept;

    template<class TYPE_T>
    group_t<TYPE_T> const& group_get();

    // TODO: function for removing specific resources

    /**
     * Remove all stored resource of the specified type
     *
     * @tparam TYPE_T Type of resource to clear
     */
    template<class TYPE_T>
    void clear() noexcept;

    // Stores maps of resoures of a specified type
    template<class TYPE_T>
    struct GroupType
    {
        GroupType() noexcept = default;
        GroupType(GroupType&& move) noexcept = default;
        GroupType& operator=(GroupType&& move) noexcept = default;

        // Needs to be copyable to store in entt::any, but copy is not allowed
        // entt experimental branch allows move-only types.
        GroupType(GroupType const& copy) { assert(0); };

        group_t<TYPE_T> m_resources;
    };

    constexpr ResPrefix_t const& get_prefix() const { return m_prefix; }

private:

    std::vector<entt::any> m_groups;

    std::string m_packageName;

    ResPrefix_t m_prefix;

    std::string m_displayName;
};

template<class TYPE_T, typename ... ARGS_T>
DependRes<TYPE_T> Package::add(std::string_view path,
                               ARGS_T&& ... args)
{
    // Runtime generated (global) sequential IDs for every unique type
    // First unique type added is 0, next is 1, then 2, etc...
    const uint32_t resTypeId = resource_id::type<TYPE_T>;

    // Resize m_groups to ensure that resTypeId a valid index
    if (m_groups.size() <= resTypeId)
    {
        m_groups.resize(resTypeId + 1);
    }

    entt::any &groupAny = m_groups[resTypeId];
    //entt::any groupAny{std::make_unique< GroupType<TYPE_T> >()};

    // Initialize GroupType if blank. This only happens for the first TYPE_T
    // added.
    if(!groupAny)
    {
        groupAny = GroupType<TYPE_T>();
    }

    GroupType<TYPE_T> &group = entt::any_cast< GroupType<TYPE_T>& >(groupAny);

    // Check if resource already exists
    auto resIt = group.m_resources.find(path); // Find resource by path

    if (resIt != group.m_resources.end())
    {
        // Resource already exists, replace if not loaded
        if (!resIt->second.m_data.has_value())
        {
            resIt->second.m_data.emplace(std::forward<ARGS_T>(args)...);
        }
        return {};
    }

    // Emplace without needing copy constructor
    auto newIt = group.m_resources
        .try_emplace(std::string{path}, construct_tag(), std::forward<ARGS_T>(args)...);

    // check if emplace fails, Resource already exists
    if (!newIt.second)
    {
        return {}; // return empty if so
    }

    return DependRes<TYPE_T>(newIt.first);
}

template<class TYPE_T>
DependRes<TYPE_T> Package::get(std::string_view path) noexcept
{
    const uint32_t resTypeId = resource_id::type<TYPE_T>;

    if (m_groups.size() < resTypeId)
    {
        return {}; // Return if resTypeId is not a valid index to m_groups
    }

    entt::any &groupAny = m_groups[resTypeId];

    if(groupAny.type() != entt::type_id< GroupType<TYPE_T> >())
    {
        return {}; // Return if GroupType is not initialized
    }

    GroupType<TYPE_T> &group = entt::any_cast<GroupType<TYPE_T>&>(groupAny);

    auto resIt = group.m_resources.find(path); // Find resource by path

    if (resIt == group.m_resources.end())
    {
        return {}; // resource not found
    }

    // found
    return DependRes<TYPE_T>(resIt);
}

template<class TYPE_T>
DependRes<TYPE_T> Package::get_or_reserve(std::string_view path) noexcept
{
    const uint32_t resTypeId = resource_id::type<TYPE_T>;

    // Resize m_groups to ensure that resTypeId a valid index
    if (m_groups.size() <= resTypeId)
    {
        m_groups.resize(resTypeId + 1);
    }

    entt::any &groupAny = m_groups[resTypeId];

    // Initialize GroupType if blank. This only happens for the first TYPE_T
    // added.
    if(!groupAny)
    {
        groupAny = GroupType<TYPE_T>();
    }

    GroupType<TYPE_T> &group = entt::any_cast<GroupType<TYPE_T>&>(groupAny);

    auto resIt = group.m_resources.find(path); // Find resource by path

    if (resIt == group.m_resources.end())
    {
        // Not found, reserve with m_data as empty optional
        auto newIt = group.m_resources.try_emplace(std::string{path},
                                                   reserve_tag());
        return DependRes<TYPE_T>(newIt.first);
    }

    // found
    return DependRes<TYPE_T>(resIt);
}

template<class TYPE_T>
Package::group_t<TYPE_T> const& Package::group_get()
{
    const uint32_t resTypeId = resource_id::type<TYPE_T>;

    // Resize m_groups to ensure that resTypeId a valid index
    if (m_groups.size() <= resTypeId)
    {
        m_groups.resize(resTypeId + 1);
    }

    entt::any &groupAny = m_groups[resTypeId];

    // Initialize GroupType if blank. This only happens for the first TYPE_T
    // added.
    if(!groupAny)
    {
        groupAny = GroupType<TYPE_T>();
    }

    GroupType<TYPE_T> &group = entt::any_cast<GroupType<TYPE_T>&>(groupAny);

    return group.m_resources;
}

template<class TYPE_T>
void Package::clear() noexcept
{
    const uint32_t resTypeId = resource_id::type<TYPE_T>;
    //m_groups.erase(entt::type_info<TYPE_T>::id());
    if (m_groups.size() > resTypeId)
    {
        m_groups[resTypeId] = entt::any{}; // Destruct GroupType
    }
}

}
