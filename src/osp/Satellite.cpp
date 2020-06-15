#include <iostream>

#include "Satellite.h"

namespace osp
{

SatelliteObject::SatelliteObject()
{
}

Satellite::Satellite(Universe* universe) :
    m_physical(false),
    m_loadRadius(30.0f), // 1km radius sphere around sat
    m_mass(0.0f),
    m_loadedBy(nullptr),
    m_object(),
    m_universe(universe),
    m_name("Default Satellite"),
    m_position({0, 0, 0}, 10)
{

    // 2^10 = 1024
    // 1024 units = 1 meter
    // enough for a solar system
    //m_precision = 10;

    //std::cout << "satellite created\n";
}


Satellite::Satellite(Satellite&& move) :
        m_physical(std::exchange(move.m_physical, false)),
        m_loadRadius(std::exchange(move.m_loadRadius, 0)),
        m_mass(std::exchange(move.m_mass, 0.0f)),
        m_loadedBy(std::exchange(move.m_loadedBy, nullptr)),
        m_object(std::move(move.m_object)),
        m_universe(std::exchange(move.m_universe, nullptr)),
        m_name(std::move(move.m_name)),
        m_position(move.m_position)
{
    if (m_object.get())
    {
        m_object->m_sat = this;
    }
    std::cout << "satellite moved\n";
}


Satellite::~Satellite()
{
    //std::cout << "satellite destroyed!\n";

    //m_object.release(); // not the right thing to do
}

Satellite& Satellite::operator=(Satellite&& move)
{
    m_physical = std::exchange(move.m_physical, false);
    m_loadRadius = std::exchange(move.m_loadRadius, 0);
    m_mass = std::exchange(move.m_mass, 0.0f);
    m_loadedBy = std::exchange(move.m_loadedBy, nullptr);
    m_object = std::move(move.m_object);
    m_universe = std::exchange(move.m_universe, nullptr);
    m_name = std::move(move.m_name);
    m_position = move.m_position;

    if (m_object.get())
    {
        m_object->m_sat = this;
    }

    std::cout << "satellite move assigned\n";

    return *this;
}


bool Satellite::is_loadable() const
{
    if (m_object.get())
    {
         return m_object->is_loadable();
    }
    return false;
}

Vector3sp Satellite::position_relative_to(Satellite& referenceFrame) const
{
    // might do more stuff here
    return (m_position - referenceFrame.m_position);
}

//Satellite::~Satellite()
//{
//    m_object.reset();
//}

}
