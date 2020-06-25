#include <iostream>

#include "SysWire.h"
#include "ActiveScene.h"

namespace osp
{

WireInput::WireInput(WireElement* element, std::string const& name) :
        m_element(element),
        m_name(name)
{

}

WireInput::WireInput(WireElement *element, WireInput&& move) :
        LinkedListItem<WireInput, WireOutput>(std::move(move)),
        m_element(element),
        m_name(std::move(move.m_name))
{

}

WireData* WireInput::connected_value()
{
    WireOutput* woConnected = list();
    if (woConnected)
    {
        return &(woConnected->value());
    }
    else
    {
        return nullptr;
    }
}

WireOutput::WireOutput(WireElement* element, std::string const& name) :
        m_element(element),
        m_name(name)
{

}

WireOutput::WireOutput(WireElement* element, std::string const& name,
                       WireInput& propagateDepend ...) :
        m_element(element),
        m_name(name)
{

}

WireOutput::WireOutput(WireElement *element, WireOutput&& move) :
        LinkedList<WireInput>(std::move(move)),
        m_element(element),
        m_name(std::move(move.m_name))
{

}

void WireInput::doErase()
{
     list()->cut(this);
};

SysWire::SysWire(ActiveScene &scene) :
        m_updateWire(scene.get_update_order(), "wire", "", "",
                     std::bind(&SysWire::update_propigate, this))
{

}


void SysWire::update_propigate()
{
    for (DependentOutput &output : m_dependentOutputs)
    {
        output.m_element->propagate_output(output.m_output);
    }
}

void SysWire::connect(WireOutput &wireFrom, WireInput &wireTo)
{
    std::cout << "Connected " << wireFrom.get_name() << " to "
              << wireTo.get_name() << "\n";
    wireFrom.insert(&wireTo);
    // TODO: check for dependent outputs and add to list and sort
}


}
