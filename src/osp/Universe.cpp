#include <iostream>
#include <iterator>

#include "Universe.h"
#include "Satellites/SatActiveArea.h"

namespace osp
{

Universe::Universe()
{
    // put something here eventually


}


Satellite& Universe::sat_create()
{
    m_satellites.emplace_back(this);
    return m_satellites.back();
}

void Universe::sat_remove(Satellite* sat)
{
    // TODO: might be a better way to do this, or pack it into its own function

    Satellite* data = m_satellites.data();

    // check if the pointer is in the vector
    assert(data < sat && sat < (data + m_satellites.size()));

    //m_satellites.begin() - sat;
    m_satellites.erase(m_satellites.begin() + (sat - data));

}

}
