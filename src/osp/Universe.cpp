#include <iostream>

#include "Universe.h"
#include "Satellites/SatActiveArea.h"

namespace osp
{

Universe::Universe()
{
    // put something here eventually

    // temporary: add a default package

    Package p({'l','z','d','b'}, "lazy-debug");
    m_packages.push_back(std::move(p));
}

void Universe::add_part(PartPrototype& prototype)
{

}

void Universe::add_parts(const std::vector<PartPrototype> &prototypes)
{
    std::cout << "Adding prototypes: " << prototypes.size() << "\n";
}

Satellite& Universe::create_sat()
{
    m_satellites.emplace_back();
    return m_satellites.back();
}



}
