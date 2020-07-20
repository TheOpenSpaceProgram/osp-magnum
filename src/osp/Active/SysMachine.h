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

class ISysMachine;
class Machine;

using MapSysMachine = std::map<std::string,
                               std::unique_ptr<ISysMachine>>;


/**
 * Holds a LinkedList of abstract Machine classes, allowing Machines to be
 * added to entities.
 */
struct CompMachine
{

    struct PartMachine
    {
        PartMachine(ActiveEnt partEnt, MapSysMachine::iterator system) :
            m_partEnt(partEnt), m_system(system) {}
        ActiveEnt m_partEnt;
        MapSysMachine::iterator m_system;
    };

    CompMachine() = default;
    CompMachine(CompMachine&& move) = default;// : m_machines(std::move(move.m_machines)) {}

    CompMachine& operator=(CompMachine&& move) = default;

    //LinkedList<Machine> m_machines;
    std::vector<PartMachine> m_machines;

};


/**
 * Machine Base class. Polymorphism is used only for wiring. Calling updates
 * are handled by SysMachines
 */
class Machine : public IWireElement
                //public LinkedListItem<Machine, LinkedList<Machine>>
{

public:
    Machine(bool enable);
    Machine(Machine const& copy) = delete;
    Machine(Machine&& move) = default;
    virtual ~Machine() = default;

    // polymorphic stuff is used only for wiring.
    // use a system for updating

    virtual void propagate_output(WireOutput* output) override = 0;

    virtual WireOutput* request_output(WireOutPort port) override = 0;
    virtual WireInput* request_input(WireInPort port) override = 0;

    virtual std::vector<WireInput*> existing_inputs() override = 0;
    virtual std::vector<WireOutput*> existing_outputs() override = 0;

    //ActiveEnt get_ent() { return m_ent; }

    // i don't like getters and setters
    //void set_enable(bool enable) { m_enable = enable; }
    //bool get_enable() { return m_enable; }

    //void doErase() override;

    bool m_enable;

private:
    //bool m_enable;
    //ActiveEnt m_ent;

};

class ISysMachine
{
public:

    virtual ~ISysMachine() = default;

    //virtual void update_sensor() = 0;
    //virtual void update_physics(float delta) = 0;

    // TODO: make some config an argument
    virtual Machine& instantiate(ActiveEnt ent) = 0;

    virtual Machine& get(ActiveEnt ent) = 0;
};

// Template for making Machine Systems
template<class Derived, class MACH_T>
class SysMachine : public ISysMachine
{
    friend Derived;

public:

    SysMachine(ActiveScene &scene) : m_scene(scene) {}
    ~SysMachine() = default;

    virtual Machine& instantiate(ActiveEnt ent) = 0;

    /**
     * Create a new Machine component
     * @param args
     * @return
     */
    //template <class... Args>
    //MACH_T& emplace(ActiveEnt ent, Args&& ... args);

private:
    ActiveScene &m_scene;
    //std::vector<MACH_T> m_machines;

};

//template<class Derived, class MACH_T>
//template<class... Args>
//MACH_T& SysMachine<Derived, MACH_T>::emplace(ActiveEnt ent, Args&& ... args)
//{
//    //m_machines.emplace_back(ent, std::forward<Args>(args)...);
//    return m_scene.reg_emplace<MACH_T>(ent, std::forward<Args>(args)...);
//}

}
