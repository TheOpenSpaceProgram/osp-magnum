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

#include <osp/Satellites/SatActiveArea.h>
#include <osp/Universe.h>


namespace planeta::universe
{

struct UCompPlanet
{
    double m_radius;

    // Approximate max length of a triangle edge on the surface. Lower number
    // means higher detail.
    float m_resolutionSurfaceMax;

    // Approximate max length of a triangle edge on the screen. The length is
    // measured on a screen 1 meter away from the viewer. Lower number means
    // higher detail.
    //
    // screenEdgeLength = physicalLength / distance
    //
    // If you stand 1m away from a meter stick perpendicular of you, then it
    // will appear 1m wide on your screen. If you walk backwards 1m, then the
    // meter stick will shrink to 0.5m, because it's further away.
    float m_resolutionScreenMax;

    // TODO: Use this until a UCompMass is added
    float m_mass;
};


class SatPlanet
{
public:
    /**
     * Set the type of a Satellite and add a UCompPlanet to it
     * @param rUni                 [out] Universe containing satellite
     * @param sat                  [in] Satellite add a planet to
     * @param radius               [in] Radius of planet in meters
     * @param mass                 [in] Mass of planet in kg
     * @param resolutionSurfaceMax [in] See UCompPlanet definition
     * @param resolutionScreenMax  [in] See UCompPlanet definition
     * @return Reference to new UCompPlanet added
     */
    static UCompPlanet& add_planet(
        osp::universe::Universe& rUni, osp::universe::Satellite sat,
        double radius, float mass, float resolutionSurfaceMax,
        float resolutionScreenMax);
};

}

namespace entt
{
    template<>
    struct type_name<planeta::universe::SatPlanet>
    {
        [[nodiscard]] static constexpr std::string_view value() noexcept {
            return "Planet";
        }
    };
} // namespace entt
