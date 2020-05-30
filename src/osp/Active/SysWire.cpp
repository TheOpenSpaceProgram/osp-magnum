#include "SysWire.h"

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


}
