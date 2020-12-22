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

#include "SysWire.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace osp::active
{

using Corrade::Containers::LinkedList;
using Corrade::Containers::LinkedListItem;

class ISysMachine;
class Machine;

using MapSysMachine = std::map<std::string,
                               std::unique_ptr<ISysMachine>>;

//-----------------------------------------------------------------------------

/**
 * This component is added to a part, and stores a vector that keeps track of
 * all the Machines it uses. Machines are stored in multiple entities, so the
 * vector stores pairs of [Entity, Machine Type (iterator to system class)]
 */
struct ACompMachines
{
    struct PartMachine
    {
        PartMachine(ActiveEnt partEnt, MapSysMachine::iterator system) :
            m_partEnt(partEnt), m_system(system) {}

        ActiveEnt m_partEnt;
        MapSysMachine::iterator m_system;
    };

    ACompMachines() noexcept = default;
    ACompMachines(ACompMachines&& move) noexcept = default;
    ACompMachines(ACompMachines const& move) = delete;
    ACompMachines& operator=(ACompMachines&& move) = default;
    ACompMachines& operator=(ACompMachines const& move) = delete;

    //LinkedList<Machine> m_machines;
    std::vector<PartMachine> m_machines;
};

//-----------------------------------------------------------------------------

/**
 * Machine Base class. This should tecnically be an AComp Virtual functions are
 * only for wiring; this will probably change soon.
 *
 * Calling updates are handled by SysMachines
 */
class Machine : public IWireElement
{

public:
    constexpr Machine() noexcept = default;
    constexpr Machine(bool enable) noexcept;
    constexpr Machine(Machine&& move) noexcept = default;
    constexpr Machine& operator=(Machine&& move) noexcept = default;

    Machine(Machine const& copy) = delete;
    Machine& operator=(Machine const& move) = delete;

    constexpr void enable(void) noexcept;
    constexpr void disable(void) noexcept;

protected:
    bool m_enable = false;
};

//-----------------------------------------------------------------------------

class ISysMachine
{
public:

    virtual ~ISysMachine() = default;

    // TODO: make some config an argument
    virtual Machine& instantiate(ActiveEnt ent) = 0;

    virtual Machine& get(ActiveEnt ent) = 0;
};

//-----------------------------------------------------------------------------

// Template for making Machine Systems
template<class Derived, class MACH_T>
class SysMachine : public ISysMachine
{
    friend Derived;

public:
    SysMachine(ActiveScene &scene) : m_scene(scene) {}
    ~SysMachine() = default;

    virtual Machine& instantiate(ActiveEnt ent) = 0;

private:
    ActiveScene &m_scene;
};

//-----------------------------------------------------------------------------

constexpr Machine::Machine(bool enable) noexcept
 : m_enable(enable)
{ }

constexpr void Machine::enable(void) noexcept
{
    m_enable = true;
}

constexpr void Machine::disable(void) noexcept
{
    m_enable = false;
}


} // namespace osp::active
