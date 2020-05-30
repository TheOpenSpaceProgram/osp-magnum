#pragma once

#include <variant>
#include <string>
#include <vector>


#include <Corrade/Containers/LinkedList.h>

namespace osp
{

using Corrade::Containers::LinkedList;
using Corrade::Containers::LinkedListItem;

// Supported data types
using WireData = std::variant<int>;

class WireInput;
class WireOutput;

// Things that have WireInputs and WireOutputs. So far, just Machines inherit.
// keep WireInputs and WireOutputs as members. move them explicitly
class WireElement
{

public:

    WireElement() = default;
    WireElement(WireElement&& move) = default;
    virtual ~WireElement() = default;


    virtual void propagate_output(WireOutput* output) = 0;
    virtual void request_output(std::string const& name) = 0;

    //void add_input(WireInput* input);
    virtual std::vector<WireInput*> available_inputs() = 0;
    virtual std::vector<WireOutput*> available_outputs() = 0;

private:

    // tracking inputs with linkedlist or vectors would complicate moves
    // prefer listing WireI/O on-demend, since it doesn't happen often
    //std::vector<WireInput*> m_inputs;
    //std::vector<WireOutput*> m_outputs;

};


class WireInput : private LinkedListItem<WireInput, WireOutput>
{
    friend LinkedListItem<WireInput, WireOutput>;
    friend LinkedList<WireInput>;

public:

    WireInput(WireElement *element, std::string const& name);
    /**
     * Move with new m_element. Use when this is a member of the WireElement
     * where m_element becomes invalid on move
     * @param element
     * @param move
     */
    WireInput(WireElement *element, WireInput&& move);

    WireInput(WireInput const& copy) = delete;
    WireInput(WireInput&& move) = default;

    // :
    //        m_name(name)
    //{
    //    static_cast<LinkedList<WireInput>*>
    //            (element)->insert((this));
    //}

private:
    WireElement* m_element;
    std::string m_name;
};


//template <void ()()>
//struct WireOutput
//{
//};

class WireOutput : private LinkedList<WireInput>
{
    friend LinkedList<WireInput>;
    friend LinkedListItem<WireInput, WireOutput>;

public:

    /**
     * Construct a WireOutput
     * @param element Associated WireElement, usually a Machine
     * @param name
     */
    WireOutput(WireElement* element, std::string const& name);
    WireOutput(WireElement* element, std::string const& name,
               WireInput& propagateDepend ...);
    /**
     * Move with new m_element. Use when this is a member of the WireElement
     * where m_element becomes invalid on move
     * @param element
     * @param move
     */
    WireOutput(WireElement *element, WireOutput&& move);

    WireOutput(WireOutput const& copy) = delete;
    WireOutput(WireOutput&& move) = default;

    //WireOutput(WireElement* element, std::string const& name,
    //           void (WireElement::*propagate_output)()) :
    //        WireOutput(element, name)
    //{
    //    m_propagate_output = propagate_output;
    //}

    std::string const& get_name() { return m_name; }


    //using LinkedListItem<WireOutput, WireElement>::erase;
    //using LinkedList<WireInput>::erase;

    void propagate()
    {
        //(list()->*m_propagate_output)();
        //list()->propagate_output(this);
    }

private:
    WireData m_value;
    WireElement* m_element;
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
