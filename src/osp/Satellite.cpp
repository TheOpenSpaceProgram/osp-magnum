#include <iostream>

#include "Satellite.h"

namespace osp
{

SatelliteObject::SatelliteObject()
{
}

Satellite::Satellite()
{
    m_name = "Innocent Satellite";

}

Satellite::Satellite(Satellite&& sat) : m_object(std::move(sat.m_object)),
                                        m_name(std::move(sat.m_name))
{
}

//Satellite::~Satellite()
//{
//    m_object.reset();
//}

}
