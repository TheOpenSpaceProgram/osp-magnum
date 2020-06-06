#include <iostream>

#include "Universe.h"
#include "Satellites/SatActiveArea.h"

namespace osp
{

Universe::Universe()
{
    // put something here eventually


}

void Universe::add_part(PrototypePart& prototype)
{

}

void Universe::add_parts(const std::vector<PrototypePart> &prototypes)
{
    std::cout << "Adding prototypes: " << prototypes.size() << "\n";
}

Satellite& Universe::create_sat()
{
    m_satellites.emplace_back(this);
    return m_satellites.back();
}



}
