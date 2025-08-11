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
#include <osp/core/math_2pow.h>

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
    EXPECT_TRUE(CoordTransformer::from_composite(a, b).is_identity());
    EXPECT_TRUE(CoordTransformer::from_composite(b, a).is_identity());
}


struct CoSpaceTransform
{
    osp::Quaterniond    rotation;
    Vector3g            position; ///< using parent cospace's precision
    int                 precision{10}; ///< for child satellites and child cospaces. 1m = 2^precision
};

// Test transforming positions between coordinate spaces using CoordTransformer
TEST(Universe, CoordTransformer)
{
    // Example solar system, similar scale to real life Sun-Earth-Moon
    CoSpaceTransform const sun
    {
        .precision = 10 // 2^10 units = 1 meter
    };
    CoSpaceTransform const planet
    {
        .position  = {sci64(150, 9, 10), sci64(150, 9, 10), sci64(42, 0, 10) },
        .precision = 12 // 2^12 units = 1 meter
    };
    CoSpaceTransform const moon
    {
        .position  = {sci64(280, 6, 12), sci64(280, 6, 12), sci64(69, 3, 12) },
        .precision = 15 // 2^15 units = 1 meter
    };
    CospaceRelationship const moonAndPlanet // Moon is parented to Planet
    {
        .parentPrecision = planet.precision,
        .childPrecision  = moon.precision,
        .childPos        = moon.position,
        .childRot        = moon.rotation
    };
    CospaceRelationship const planetAndSun // Planet is parented to Sun.
    {
        .parentPrecision = sun.precision,
        .childPrecision  = planet.precision,
        .childPos        = planet.position,
        .childRot        = planet.rotation
    };

    // Point 100000m above planet in 3 different coordinate spaces
    Vector3g const abovePlanetPlanet = {0, 0, sci64(100, 3, 12)};
    Vector3g const abovePlanetSun    = planet.position + change_precision(abovePlanetPlanet, 12, 10);
    Vector3g const abovePlanetMoon   = change_precision(-moon.position, 12, 15) + change_precision(abovePlanetPlanet, 12, 15);

    // Point 100000m above moon in 3 different coordinate spaces
    Vector3g const aboveMoonMoon   = {0, 0, sci64(100, 3, 15)};
    Vector3g const aboveMoonPlanet = moon.position   + change_precision(aboveMoonMoon,   15, 12);
    Vector3g const aboveMoonSun    = planet.position + change_precision(aboveMoonPlanet, 12, 10);

    // All 6 possible coordinate space transformations
    auto const sunToPlanet  = CoordTransformer::from_parent_to_child(planetAndSun);
    auto const planetToSun  = CoordTransformer::from_child_to_parent(planetAndSun);
    auto const planetToMoon = CoordTransformer::from_parent_to_child(moonAndPlanet);
    auto const moonToPlanet = CoordTransformer::from_child_to_parent(moonAndPlanet);
    auto const sunToMoon    = CoordTransformer::from_composite(planetToMoon, sunToPlanet);
    auto const moonToSun    = CoordTransformer::from_composite(planetToSun, moonToPlanet);

    expect_inverse(sunToPlanet,     planetToSun);
    expect_inverse(planetToMoon,    moonToPlanet);
    expect_inverse(sunToMoon,       moonToSun);

    // Confirm Planet position in Sun's space == Planet's origin
    EXPECT_EQ(sunToPlanet.transforposition(planet.position), gc_v3gZero);
    EXPECT_EQ(planetToSun.transforposition(gc_v3gZero), planet.position);

    // Confirm Moon position in Planets's space == Moon's origin
    EXPECT_EQ(planetToMoon.transforposition(moon.position), gc_v3gZero);
    EXPECT_EQ(moonToPlanet.transforposition(gc_v3gZero), moon.position);

    // Confirm point above Planet is consistent between spaces
    EXPECT_EQ(sunToPlanet.transforposition(abovePlanetSun), abovePlanetPlanet);
    EXPECT_EQ(planetToSun.transforposition(abovePlanetPlanet), abovePlanetSun);
    EXPECT_EQ(moonToPlanet.transforposition(abovePlanetMoon), abovePlanetPlanet);
    EXPECT_EQ(planetToMoon.transforposition(abovePlanetPlanet), abovePlanetMoon);

    // Confirm point above Moon is consistent between spaces
    EXPECT_EQ(planetToMoon.transforposition(aboveMoonPlanet), aboveMoonMoon);
    EXPECT_EQ(moonToPlanet.transforposition(aboveMoonMoon), aboveMoonPlanet);
    EXPECT_EQ(sunToMoon.transforposition(aboveMoonSun), aboveMoonMoon);
    EXPECT_EQ(moonToSun.transforposition(aboveMoonMoon), aboveMoonSun);
}

// Test CoordTransformer with rotated coordinate spaces
TEST(Universe, CoordTransformerRotations)
{
    CoSpaceTransform const sun
    {
        .precision = 10 // 2^10 units = 1 meter
    };
    CoSpaceTransform const planet
    {
        .rotation = Quaterniond::rotation(90.0_deg, {0.0f, 0.0f, 1.0f}),
        .position  = {sci64(150, 9, 10), sci64(150, 9, 10), sci64(42, 0, 10)},
        .precision = 13 // 2^10 units = 1 meter
    };
    CoSpaceTransform moon
    {
        .position  = {sci64(160, 9, 10), sci64(170, 9, 10), sci64(69, 3, 10)},
        .precision = 15 // 2^10 units = 1 meter
    };

    // Set moon rotation so its X-axis points at the planet (like a tidal lock)
    Vector3d const diff = Vector3d(planet.position - moon.position) / int_2pow<int>(10);
    Vector3d const forward{1.0, 0.0, 0.0};
    auto ang  = Magnum::Math::angle(diff.normalized(), forward);
    auto axis = Magnum::Math::cross(forward, diff.normalized()).normalized();
    moon.rotation = Quaterniond::rotation(ang, axis);

    CospaceRelationship const moonAndSun // Moon is parented to Sun (different from prev test)
    {
        .parentPrecision = sun.precision,
        .childPrecision  = moon.precision,
        .childPos        = moon.position,
        .childRot        = moon.rotation
    };
    CospaceRelationship const planetAndSun // Planet is parented to Sun.
    {
        .parentPrecision = sun.precision,
        .childPrecision  = planet.precision,
        .childPos        = planet.position,
        .childRot        = planet.rotation
    };

    // Point +X of planet. due to 90deg CCW rotation, sun-space sees +Y
    Vector3g const aheadPlanetPlanet = {sci64(200, 3, 13), 0, 0};
    Vector3g const aheadPlanetSun    = planet.position + Vector3g{0, sci64(200, 3, 10), 0};

    auto const sunToPlanet  = CoordTransformer::from_parent_to_child(planetAndSun);
    auto const planetToSun  = CoordTransformer::from_child_to_parent(planetAndSun);
    auto const sunToMoon    = CoordTransformer::from_parent_to_child(moonAndSun);
    auto const moonToSun    = CoordTransformer::from_child_to_parent(moonAndSun);
    auto const planetToMoon = CoordTransformer::from_composite(sunToMoon, planetToSun);
    auto const moonToPlanet = CoordTransformer::from_composite(sunToPlanet, moonToSun);

    expect_inverse(sunToPlanet,     planetToSun);
    expect_inverse(sunToMoon,       moonToSun);
    expect_inverse(planetToMoon,    moonToPlanet);

    // Confirm point ahead of planet is properly rotated
    EXPECT_EQ(planetToSun.transforposition(aheadPlanetPlanet), aheadPlanetSun);
    EXPECT_EQ(sunToPlanet.transforposition(aheadPlanetSun), aheadPlanetPlanet);

    // Confirm distance between planet and moon are consistent
    double const dist           = diff.length();
    double const distSunPlanet  = Vector3d(sunToPlanet.transforposition(moon.position)).length() / int_2pow<int>(13);
    double const distSunMoon    = Vector3d(sunToMoon.transforposition(planet.position)).length() / int_2pow<int>(15);
    double const distMoonPlanet = Vector3d(moonToPlanet.transforposition({})).length() / int_2pow<int>(13);
    double const distPlanetMoon = Vector3d(planetToMoon.transforposition({})).length() / int_2pow<int>(15);

    EXPECT_NEAR(dist, distSunPlanet,  0.1f);
    EXPECT_NEAR(dist, distSunMoon,    0.1f);
    EXPECT_NEAR(dist, distPlanetMoon, 0.1f);
    EXPECT_NEAR(dist, distMoonPlanet, 0.1f);

    // Moon's +X points directly at the planet. Expect X coordinate = distance
    EXPECT_NEAR(dist, double(planetToMoon.transforposition({}).x()) / int_2pow<int>(15), 0.1f);

    // Expect (dist) meters +X of moon to be the Planet's position
    Vector3g const moonRay{spaceint_t(dist * int_2pow<int>(15)), 0, 0};
    expect_near_vec(moonToPlanet.transforposition(moonRay), {}, 4);
    expect_near_vec(moonToSun.transforposition(moonRay), planet.position, 4);
}

// TODO: Test CoordTransformer for hopping across nested rotated coordinate spaces
