#pragma once

#include <vector>

#include <Corrade/Containers/LinkedList.h>

#include "SysWire.h"

namespace osp
{

using Corrade::Containers::LinkedList;
using Corrade::Containers::LinkedListItem;


class Machine;

// Machine component to add to entt Entities
class CompMachine
{
    LinkedList<Machine> m_machines;

};


// Base machine
class Machine : public WireElement,
                public LinkedListItem<Machine, LinkedList<Machine>>
{

public:
    Machine() = default;
    Machine(Machine const& copy) = delete;
    Machine(Machine&& move) = default;
    virtual ~Machine() = default;

    virtual void propagate_output(WireOutput* output) = 0;
    virtual void request_output(std::string const& name) {};

    virtual std::vector<WireInput*> available_inputs() = 0;
    virtual std::vector<WireOutput*> available_outputs() = 0;
};

class AbstractSysMachine
{
public:
    AbstractSysMachine() = default;
    virtual void update() = 0;
};

// Template for making Machine Systems
template<class Derived, class Mach>
class SysMachine : public AbstractSysMachine
{

    friend Derived;

public:

    //virtual void update_sense() = 0;
    //virtual void update_respond() = 0;
    void update() { static_cast<Derived&>(*this).doUpdate(); }

    /**
     * Create a new Machine and add to internal vector
     * @param args
     * @return
     */
    template <class... Args>
    Mach& emplace(Args&& ... args);

private:
    std::vector<Mach> m_machines;

};

template<class Derived, class Mach>
template<class... Args>
Mach& SysMachine<Derived, Mach>::emplace(Args&& ... args)
{
    m_machines.emplace_back();//std::forward<Args>(args)...);
    return m_machines.back();
}

}
