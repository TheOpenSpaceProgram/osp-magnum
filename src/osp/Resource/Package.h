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

#include <any>
#include <iostream>
#include <cstdint>
#include <map>
#include <memory>
#include <string>


namespace osp
{

//class SturdyFile;
//class PrototypePart;

//
// What I think might be a good idea:
//
// Packages cache, address, and maybe store all resources usable in the game
// They have names like "cool-rocket-engines-v2"
// They also have 4-character unique ASCII prefix to refer to them
//
// resources paths look like this:
// prfx:dirmaybe/resource
//
// note: dirs are kind of pointless
//
// TODO: Some sort of data-loading strategy/policy: folder, zip, network etc...
//       For now, packages only store loaded data
//


// supported resources

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

class Package
{
public:


    Package(Package&& move) = default;
    Package(const Package& copy) = delete;
    Package& operator=(Package const& copy) = delete;

    Package(std::string prefix, std::string packageName);

    //ResourceTable m_resources;

    //virtual Magnum::GL::Mesh* request_mesh(const std::string& path);

    //TypeMap<int>::fish;
    //std::vector< Resource<Magnum::Trade::ImageData> > g_imageData;

    //constexpr ResourceTable& debug_get_resource_table() { return m_resources; }

//    template<class T>
//    Resource<T>* debug_add_resource(Resource<T>&& resource);

//    template<class T>
//    Resource<T>* get_resource(std::string const& path);

//    template<class T>
//    Resource<T>* get_resource(unsigned resIndex);

    template<class TYPE_T, typename ... ARGS_T>
    DependRes<TYPE_T> add(std::string const& path, ARGS_T&& ... args);

    template<class TYPE_T>
    DependRes<TYPE_T> get(std::string const& path);

    template<class TYPE_T>
    void clear();

    void clear_all();

    struct GroupType
    {
        virtual ~GroupType() = default;
    };

    template<class TYPE_T>
    struct GroupTypeRes : public GroupType
    {
        ~GroupTypeRes() = default;
        //std::vector<TYPE_T> m_types;
        std::map<std::string, Resource<TYPE_T>> m_resources;
    };

    ResPrefix_t get_prefix() const { return m_prefix; }

private:

    std::map<entt::id_type, std::unique_ptr<GroupType>> m_groups;

    std::string m_packageName;

    ResPrefix_t m_prefix;

    std::string m_displayName;

};

template<class TYPE_T, typename ... ARGS_T>
DependRes<TYPE_T> Package::add(std::string const& path,
                               ARGS_T&& ... args)
{
    // this should create a blank if it doesn't exist yet
    //std::any& groupAny(m_groups[entt::type_info<TYPE_T>::id()]);
    std::unique_ptr<GroupType>& groupAny(m_groups[entt::type_info<TYPE_T>::id()]);



    // check if blank
    if(!groupAny)
    {
        groupAny = std::make_unique<GroupTypeRes<TYPE_T>>();
    }

    GroupTypeRes<TYPE_T> &group = *static_cast<GroupTypeRes<TYPE_T>*>(groupAny.get());

    // problem: emplace without needing copy constructor
    //group.m_resources.try_emplace(path, {std::forward<ARGS_T>(args)...}, false, 0);
    //group.m_resources.emplace(std::piecewise_construct,
    //                          std::forward_as_tuple(path),
    //                          std::forward_as_tuple({std::forward<ARGS_T>(args)...}, false, 0));
    //auto final = group.m_resources.try_emplace(path. );
    auto final = group.m_resources.try_emplace(path, false, std::forward<ARGS_T>(args)...);

    if (!final.second)
    {
        // resource already exists
        std::cout << "resource already exists\n";
    }

    return DependRes<TYPE_T>(final.first);
}

template<class TYPE_T>
DependRes<TYPE_T> Package::get(std::string const& path)
{
    auto itType = m_groups.find(entt::type_info<TYPE_T>::id());

    if (itType == m_groups.end())
    {
        // type not found
        return DependRes<TYPE_T>();
    }

    GroupTypeRes<TYPE_T> &group = *static_cast<GroupTypeRes<TYPE_T>*>(itType->second.get());

    auto resIt = group.m_resources.find(path);

    if (resIt == group.m_resources.end())
    {
        // resource not found
        return DependRes<TYPE_T>();
    }

    // found
    return DependRes<TYPE_T>(resIt);
}

template<class TYPE_T>
void Package::clear()
{
    m_groups.erase(entt::type_info<TYPE_T>::id());
}

}
