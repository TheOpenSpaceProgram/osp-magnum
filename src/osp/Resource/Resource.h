#pragma once

#include <string>

#include <Corrade/Containers/LinkedList.h>

namespace osp
{

template <class T>
struct Resource;

using Corrade::Containers::LinkedList;
using Corrade::Containers::LinkedListItem;


template <class T>
class ResDependency : public LinkedListItem<ResDependency<T>, Resource<T> >
{

public:

    ResDependency() = default;
    ResDependency(Resource<T>& resource) :
            LinkedListItem<ResDependency<T>, Resource<T> >()
    { resource.insert(this); }

    void bind(LinkedList<ResDependency<T> >& resource)
    { resource.insert(this); }

    constexpr T* get_data()
    {
        return &(LinkedListItem<ResDependency<T>, Resource<T> >::list()->m_data);
    }

};


struct AbstractResource
{

    //std::string m_name; // name is kind of useless when there's path
    std::string m_path;

    bool m_loaded;

    // TODO: * some sort of type identification, or just use typeof.
    //         implement when needed.
    //       * a way to identify loading strategy: dir, network, zip, etc
    //       * dependency to other resources

};

// TODO: make this a class
template <class T>
struct Resource : public AbstractResource, LinkedList<ResDependency<T> >
{
    Resource() = default;
    Resource(T&& move) : m_data(std::move(move)) {}
    Resource(const T& copy) : m_data(copy) {}

    //LinkedList<ResDependency<T> > m_usedBy;
    T m_data;
};

}
