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


class Package
{



public:


    Package(Package&& move) = default;
    Package(const Package& copy) = delete;

    Package(std::string const& prefix, std::string const& packageName);

    //ResourceTable m_resources;

    typedef uint32_t Prefix;
    //typedef char Prefix[4];

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


private:

    std::map<entt::id_type, std::unique_ptr<GroupType>> m_groups;

    std::string m_packageName;

    Prefix m_prefix;

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
