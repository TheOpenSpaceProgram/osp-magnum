#pragma once


#include <cstdint>
#include <iostream>
#include <vector>

#include <Corrade/Containers/LinkedList.h>

#include "activetypes.h"
#include "SysWire.h"

namespace osp
{

using Corrade::Containers::LinkedList;
using Corrade::Containers::LinkedListItem;

class Machine;

/**
 * Holds a LinkedList of abstract Machine classes, allowing Machines to be
 * added to entities.
 */
struct CompMachine
{
    CompMachine() = default;
    CompMachine(CompMachine&& move) : m_machines(std::move(move.m_machines)) {}

    CompMachine& operator=(CompMachine&& move) = default;

    LinkedList<Machine> m_machines;

};


/**
 * Machine Base class. Polymorphism is used only for wiring. Calling updates
 * are handled by SysMachines
 */
class Machine : public WireElement,
                public LinkedListItem<Machine, LinkedList<Machine>>
{

public:
    Machine(ActiveEnt ent);
    Machine(Machine const& copy) = delete;
    Machine(Machine&& move) = default;
    virtual ~Machine() = default;


    // polymorphic stuff is used only for wiring
    // use a system for updating

    virtual void propagate_output(WireOutput* output) override = 0;

    virtual WireOutput* request_output(WireOutPort port) override = 0;
    virtual WireInput* request_input(WireInPort port) override = 0;

    virtual std::vector<WireInput*> existing_inputs() override = 0;
    virtual std::vector<WireOutput*> existing_outputs() override = 0;

    ActiveEnt get_ent() { return m_ent; }

    void doErase() override;

    // just a simple bool
    //bool m_enable;
private:
    ActiveEnt m_ent;
};

class AbstractSysMachine
{
public:
    AbstractSysMachine() = default;
    AbstractSysMachine(AbstractSysMachine const& copy) = delete;
    AbstractSysMachine(AbstractSysMachine&& move) = delete;
    virtual ~AbstractSysMachine() = default;

    //virtual void update_sensor() = 0;
    //virtual void update_physics(float delta) = 0;

    // TODO: make some config an argument
    virtual Machine& instantiate(ActiveEnt ent) = 0;
};

// Template for making Machine Systems
template<class Derived, class Mach>
class SysMachine : public AbstractSysMachine
{
    friend Derived;

public:

    SysMachine(ActiveScene &scene) : m_scene(scene) {}
    ~SysMachine() = default;

    virtual Machine& instantiate(ActiveEnt ent) = 0;

    /**
     * Create a new Machine and add to internal vector
     * @param args
     * @return
     */
    template <class... Args>
    Mach& emplace(ActiveEnt ent, Args&& ... args);

private:
    ActiveScene &m_scene;
    std::vector<Mach> m_machines;

};

template<class Derived, class Mach>
template<class... Args>
Mach& SysMachine<Derived, Mach>::emplace(ActiveEnt ent, Args&& ... args)
{
    m_machines.emplace_back(ent, std::forward<Args>(args)...);
    return m_machines.back();
}

}
