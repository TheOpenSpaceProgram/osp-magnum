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

#include <osp/Universe.h>


namespace planeta::universe
{

//using namespace osp;
//using namespace osp::universe;

struct UCompPlanet
{
    double m_radius;

    // Approximate max length of a triangle edge on the surface. Lower number
    // means higher resolution
    float m_resolutionSurfaceMax;

    // Approximate max length of a triangle edge on the surface. Lower number
    // means higher resolution
    float m_resolutionScreenMax;

    // TODO: Use this until a UCompMass is added
    float m_mass;
};


class SatPlanet : public osp::universe::CommonTypeSat<
        SatPlanet, UCompPlanet,
        osp::universe::UCompActivatable>
{
public:

    static const std::string smc_name;

    SatPlanet(osp::universe::Universe& universe);
    ~SatPlanet() = default;

    virtual std::string get_name() { return smc_name; };

private:


};

}
