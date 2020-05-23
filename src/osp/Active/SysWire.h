#pragma once

#include <variant>
#include <string>


#include <Corrade/Containers/LinkedList.h>

namespace osp
{

using Corrade::Containers::LinkedList;
using Corrade::Containers::LinkedListItem;

// Supported data types
using WireData = std::variant<int>;

class WireInput;
class WireOutput;

// Things that can have WireInputs and WireOutputs
// so far, just Machines
class WireElement :
        public LinkedList<WireInput>,
        public LinkedList<WireOutput>
{
public:
    WireElement() = default;
    WireElement(WireElement&& move) = default;
    virtual ~WireElement() = default;

    virtual void propagate_output(WireOutput* output) = 0;
};



class WireInput : public LinkedListItem<WireInput, LinkedList<WireInput>>
{
public:
    WireInput(LinkedList<WireInput>* list, std::string const& name)
    {
        list->insert(this);
    }

};

//template <void ()()>
//struct WireOutput
//{
//};

class WireOutput : public LinkedListItem<WireOutput, WireElement>
{
public:
    /**
     * Construct a WireOutput
     * @param element Associated WireElement, usually a Machine
     * @param name
     */
    WireOutput(WireElement* element, std::string const& name) :
        m_name(name)
    {
        static_cast<LinkedList<WireOutput>* >(element)->insert(this);
    }

    WireOutput(WireElement* element, std::string const& name,
               WireInput& propagateDepend ...) :
        WireOutput(element, name)
    {

    }

    //WireOutput(WireElement* element, std::string const& name,
    //           void (WireElement::*propagate_output)()) :
    //        WireOutput(element, name)
    //{
    //    m_propagate_output = propagate_output;
    //}

    void propagate()
    {
        //(list()->*m_propagate_output)();
        list()->propagate_output(this);
    }

    std::string const& get_name() { return m_name; }

private:
    WireData m_value;
    std::string m_name;

    //void (WireElement::*m_propagate_output)();

    //std::vector<>
    //virtual void propagate() {}
};




class SysWire
{
public:
    SysWire() = default;
};


}
