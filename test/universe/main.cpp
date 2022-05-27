/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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
#include <osp/universe/universe.h>
#include <osp/universe/coordinates.h>
#include <osp/CommonMath.h>

#include <Magnum/Math/Functions.h>

#include <gtest/gtest.h>

using namespace osp;
using namespace osp::universe;

using osp::math::int_2pow;
using osp::math::mul_2pow;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

constexpr Vector3g const gc_v3gZero{0, 0, 0};

/**
 * @return coefficient * 10^exp * 2^prec
 */
static int64_t sci64(int64_t coefficient, int exp, int prec)
{
    return coefficient * Magnum::Math::pow<int64_t>(10, exp)
                       * Magnum::Math::pow<int64_t>(2, prec);
}

static void expect_near_vec(Vector3g a, Vector3g b, spaceint_t maxError)
{
    spaceint_t const dist = (a - b).length();
    EXPECT_NEAR(dist, 0, maxError);
}

static Vector3g change_precision(Vector3g in, int precFrom, int precTo)
{
    return mul_2pow<Vector3g, spaceint_t>(in, precTo - precFrom);
}

/**
 * @brief Expect two CoordTransformers to be inverses of each other
 */
static void expect_inverse(CoordTransformer const& a, CoordTransformer const& b)
{
    CoordTransformer const c = coord_composite(a, b);
    CoordTransformer const d = coord_composite(b, a);
    EXPECT_TRUE(c.is_identity());
    EXPECT_TRUE(d.is_identity());
}


// Test transforming positions between coordinate spaces using CoordTransformer
TEST(Universe, CoordTransformer)
{
    // Example solar system, similar scale to real life Sun-Earth-Moon
    CoSpaceTransform sun
    {
        .m_precision = 10 // 2^10 units = 1 meter
    };
    CoSpaceTransform planet
    {
        .m_position  = {sci64(150, 9, 10), sci64(150, 9, 10), sci64(42, 0, 10) },
        .m_precision = 12 // 2^12 units = 1 meter
    };
    CoSpaceTransform moon
    {
        .m_position  = {sci64(280, 6, 12), sci64(280, 6, 12), sci64(69, 3, 12) },
        .m_precision = 15 // 2^15 units = 1 meter
    };
    // Moon is parented to Planet, Planet is parented to Sun.
    // m_parent isn't used. Test only calls coord_parent_to_child and
    // coord_child_to_parent, which don't care about m_parent

    // Point 100000m above planet in 3 different coordinate spaces
    Vector3g const abovePlanetPlanet = {0, 0, sci64(100, 3, 12)};
    Vector3g const abovePlanetSun    = planet.m_position + change_precision(abovePlanetPlanet, 12, 10);
    Vector3g const abovePlanetMoon   = change_precision(-moon.m_position, 12, 15) + change_precision(abovePlanetPlanet, 12, 15);

    // Point 100000m above moon in 3 different coordinate spaces
    Vector3g const aboveMoonMoon   = {0, 0, sci64(100, 3, 15)};
    Vector3g const aboveMoonPlanet = moon.m_position   + change_precision(aboveMoonMoon,   15, 12);
    Vector3g const aboveMoonSun    = planet.m_position + change_precision(aboveMoonPlanet, 12, 10);

    // All 6 possible coordinate space transformations
    CoordTransformer const sunToPlanet  = coord_parent_to_child(sun, planet);
    CoordTransformer const planetToSun  = coord_child_to_parent(sun, planet);
    CoordTransformer const planetToMoon = coord_parent_to_child(planet, moon);
    CoordTransformer const moonToPlanet = coord_child_to_parent(planet, moon);
    CoordTransformer const sunToMoon    = coord_composite(planetToMoon, sunToPlanet);
    CoordTransformer const moonToSun    = coord_composite(planetToSun, moonToPlanet);

    expect_inverse(sunToPlanet,     planetToSun);
    expect_inverse(planetToMoon,    moonToPlanet);
    expect_inverse(sunToMoon,       moonToSun);

    // Confirm Planet position in Sun's space == Planet's origin
    EXPECT_EQ(sunToPlanet.transform_position(planet.m_position), gc_v3gZero);
    EXPECT_EQ(planetToSun.transform_position(gc_v3gZero), planet.m_position);

    // Confirm Moon position in Planets's space == Moon's origin
    EXPECT_EQ(planetToMoon.transform_position(moon.m_position), gc_v3gZero);
    EXPECT_EQ(moonToPlanet.transform_position(gc_v3gZero), moon.m_position);

    // Confirm point above Planet is consistent between spaces
    EXPECT_EQ(sunToPlanet.transform_position(abovePlanetSun), abovePlanetPlanet);
    EXPECT_EQ(planetToSun.transform_position(abovePlanetPlanet), abovePlanetSun);
    EXPECT_EQ(moonToPlanet.transform_position(abovePlanetMoon), abovePlanetPlanet);
    EXPECT_EQ(planetToMoon.transform_position(abovePlanetPlanet), abovePlanetMoon);

    // Confirm point above Moon is consistent between spaces
    EXPECT_EQ(planetToMoon.transform_position(aboveMoonPlanet), aboveMoonMoon);
    EXPECT_EQ(moonToPlanet.transform_position(aboveMoonMoon), aboveMoonPlanet);
    EXPECT_EQ(sunToMoon.transform_position(aboveMoonSun), aboveMoonMoon);
    EXPECT_EQ(moonToSun.transform_position(aboveMoonMoon), aboveMoonSun);
}

// Test CoordTransformer with rotated coordinate spaces
TEST(Universe, CoordTransformerRotations)
{
    CoSpaceTransform sun
    {
        .m_precision = 10 // 2^10 units = 1 meter
    };
    CoSpaceTransform planet
    {
        .m_rotation = Quaterniond::rotation(90.0_deg, {0.0f, 0.0f, 1.0f}),
        .m_position  = {sci64(150, 9, 10), sci64(150, 9, 10), sci64(42, 0, 10)},
        .m_precision = 13 // 2^10 units = 1 meter
    };
    CoSpaceTransform moon
    {
        .m_position  = {sci64(160, 9, 10), sci64(170, 9, 10), sci64(69, 3, 10)},
        .m_precision = 15 // 2^10 units = 1 meter
    };
    // Planet and Moon are parented to Sun. Different from previous test!!!

    // Moon's X points at the planet (like a tidal lock)
    Vector3d const diff = Vector3d(planet.m_position - moon.m_position) / int_2pow<int>(10);
    Vector3d const forward{1.0, 0.0, 0.0};
    auto ang  = Magnum::Math::angle(diff.normalized(), forward);
    auto axis = Magnum::Math::cross(forward, diff.normalized()).normalized();
    moon.m_rotation = Quaterniond::rotation(ang, axis);

    // Point +X of planet. due to 90deg CCW rotation, sun-space sees +Y
    Vector3g const aheadPlanetPlanet = {sci64(200, 3, 13), 0, 0};
    Vector3g const aheadPlanetSun    = planet.m_position + Vector3g{0, sci64(200, 3, 10), 0};

    CoordTransformer const sunToPlanet  = coord_parent_to_child(sun, planet);
    CoordTransformer const planetToSun  = coord_child_to_parent(sun, planet);
    CoordTransformer const sunToMoon    = coord_parent_to_child(sun, moon);
    CoordTransformer const moonToSun    = coord_child_to_parent(sun, moon);
    CoordTransformer const planetToMoon = coord_composite(sunToMoon, planetToSun);
    CoordTransformer const moonToPlanet = coord_composite(sunToPlanet, moonToSun);

    expect_inverse(sunToPlanet,     planetToSun);
    expect_inverse(sunToMoon,       moonToSun);
    expect_inverse(planetToMoon,    moonToPlanet);

    // Confirm point ahead of planet is properly rotated
    EXPECT_EQ(planetToSun.transform_position(aheadPlanetPlanet), aheadPlanetSun);
    EXPECT_EQ(sunToPlanet.transform_position(aheadPlanetSun), aheadPlanetPlanet);

    // Confirm distance between planet and moon are consistent
    double const dist           = diff.length();
    double const distSunPlanet  = Vector3d(sunToPlanet.transform_position(moon.m_position)).length() / int_2pow<int>(13);
    double const distSunMoon    = Vector3d(sunToMoon.transform_position(planet.m_position)).length() / int_2pow<int>(15);
    double const distMoonPlanet = Vector3d(moonToPlanet.transform_position({})).length() / int_2pow<int>(13);
    double const distPlanetMoon = Vector3d(planetToMoon.transform_position({})).length() / int_2pow<int>(15);

    EXPECT_NEAR(dist, distSunPlanet,  0.1f);
    EXPECT_NEAR(dist, distSunMoon,    0.1f);
    EXPECT_NEAR(dist, distPlanetMoon, 0.1f);
    EXPECT_NEAR(dist, distMoonPlanet, 0.1f);

    // Moon's +X points directly at the planet. Expect X coordinate = distance
    EXPECT_NEAR(dist, double(planetToMoon.transform_position({}).x()) / int_2pow<int>(15), 0.1f);

    // Expect (dist) meters +X of moon to be the Planet's position
    Vector3g const moonRay{spaceint_t(dist * int_2pow<int>(15)), 0, 0};
    expect_near_vec(moonToPlanet.transform_position(moonRay), {}, 4);
    expect_near_vec(moonToSun.transform_position(moonRay), planet.m_position, 4);
}

// TODO: Test CoordTransformer for hopping across nested rotated coordinate spaces
