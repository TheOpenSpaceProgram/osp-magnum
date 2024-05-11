/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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

#include "kepler.h"
#include <algorithm>
#include <cassert>
#include <cmath>

using Magnum::Math::cross;
using Magnum::Math::dot;

namespace {

// PI isn't standard, so define it here. Should it be added to constants.h?
static constexpr double PI = Magnum::Math::Constants<double>::pi();

/**
 * @brief Wrap an angle to the range [0, 2π)
 */
static constexpr double wrap_angle(double const angle)
{
    double wrapped = fmod(angle, 2.0 * PI);
    if (wrapped < 0)
    {
        wrapped += 2.0 * PI;
    }
    return wrapped;
}

/**
 * @brief Solve Kepler's equation for elliptic orbits (E = M + e * sin(E))
 * 
 * @return Eccentric anomaly E
 */
static double solve_kepler_elliptic(double const meanAnomaly, double const eccentricity, int steps)
{
    double const tolerance = 1.0E-10;

    // If eccentricity is low, then this is a circular orbit, so M = E
    if (std::abs(eccentricity) < 1.0E-9)
    {
        return meanAnomaly;
    }

    double delta;

    // initial guess
    double E = meanAnomaly;

    // use newton's method
    // todo: implement something which converges more quickly
    do
    {
        double f_E = E - eccentricity * sin(E) - meanAnomaly;
        double df_E = 1.0 - eccentricity * cos(E);

        delta = f_E / df_E;
        E = E - delta;

        E = wrap_angle(E); // Sometimes this sometimes fails to converge without this line
        --steps;
    } while (std::abs(delta) > tolerance && steps > 1);
    return E;
}

/**
 * @brief Solve Kepler's equation for hyperbolic orbits (E = M + e * sinh(E))
 * 
 * @return Eccentric anomaly E
 */
static double solve_kepler_hyperbolic(double const meanAnomaly, double const eccentricity, int steps)
{
    double const tolerance = 1.0E-10;

    double delta;

    // initial guess
    double E = std::log(std::abs(meanAnomaly));

    // use newton's method
    do
    {
        double f_E = eccentricity * sinh(E) - E - meanAnomaly;
        double df_E = eccentricity * cosh(E) - 1.0;

        delta = f_E / df_E;
        E = E - delta;
        --steps;
    } while (std::abs(delta) > tolerance && steps > 1);
    return E;
}

static double elliptic_eccentric_anomaly_to_true_anomaly(double const &ecc, double const &E)
{
    // This formula supposedly has better numerical stability than the one below
    double const beta = ecc / (1 + std::sqrt(1 - ecc * ecc));
    return E + 2.0 * atan2(beta * sin(E), 1 - beta * cos(E));

    // return 2.0 * atan2(std::sqrt(1 + ecc) * sin(E / 2.0), std::sqrt(1 - ecc) * cos(E / 2.0));
}

static double hyperbolic_eccentric_anomaly_to_true_anomaly(double const &ecc, double const &H)
{
    return 2 * atan(sqrt((ecc + 1) / (ecc - 1)) * tanh(H / 2));
}

static double true_anomaly_to_eccentric_anomaly(double const &ecc, double const &v)
{
    return 2.0 * atan(std::sqrt((1 - ecc) / (1 + ecc)) * tan(v / 2.0));
}

static double true_anomaly_to_hyperbolic_eccentric_anomaly(double const &ecc, double const &v)
{
    return 2.0 * atanh(std::clamp(std::sqrt((ecc - 1) / (ecc + 1)) * tan(v / 2.0), -1.0, 1.0));
}

static double mean_motion(double const mu, double const a)
{
    return sqrt(mu / (a * a * a));
}

static double get_orbiting_radius(double const ecc, double const semiMajorAxis, double const trueAnomaly)
{
    return semiMajorAxis * (1 - ecc * ecc) / (1 + ecc * cos(trueAnomaly));
}

} // namespace




namespace osp
{

double KeplerOrbit::get_true_anomaly_from_eccentric(double const eccentric) const
{
    if (is_elliptic())
    {
        return elliptic_eccentric_anomaly_to_true_anomaly(m_params.eccentricity, eccentric);
    }
    else if (is_hyperbolic())
    {
        return hyperbolic_eccentric_anomaly_to_true_anomaly(m_params.eccentricity, eccentric);
    }
}

double KeplerOrbit::get_eccentric_anomaly(double const time) const
{
    if (is_elliptic())
    {
        double const nu = mean_motion(m_params.gravitationalParameter, m_params.semiMajorAxis);
        double Mt = m_params.meanAnomalyAtEpoch + (time - m_params.epoch) * nu;
        Mt = wrap_angle(Mt);
        return solve_kepler_elliptic(Mt, m_params.eccentricity, 20000);
    }
    else if (is_hyperbolic())
    {
        double const nu = mean_motion(m_params.gravitationalParameter, -m_params.semiMajorAxis);
        double const Mt = m_params.meanAnomalyAtEpoch + (time - m_params.epoch) * nu;
        return solve_kepler_hyperbolic(Mt, m_params.eccentricity, 20000);
    }
}

void KeplerOrbit::get_fixed_reference_frame(Vector3d &radial, Vector3d &transverse, Vector3d &outOfPlane) const
{
    double const a = m_params.argumentOfPeriapsis;
    double const b = m_params.longitudeOfAscendingNode;
    double const c = m_params.inclination;

    radial = Vector3d(cos(a) * cos(b) - sin(a) * sin(b) * cos(c),
                        cos(a) * sin(b) + sin(a) * cos(b) * cos(c),
                        sin(a) * sin(c));

    transverse = Vector3d(-sin(a) * cos(b) - cos(a) * sin(b) * cos(c),
                            -sin(a) * sin(b) + cos(a) * cos(b) * cos(c),
                            cos(a) * sin(c));

    outOfPlane = Vector3d(sin(b) * sin(c),
                            -cos(b) * sin(c),
                            cos(c));
}

void KeplerOrbit::get_state_vectors_at_time(double const time, Vector3d &position, Vector3d &velocity) const
{
    double const E = get_eccentric_anomaly(time);
    double const trueAnomaly = get_true_anomaly_from_eccentric(E);
    get_state_vectors_at_eccentric_anomaly(E, position, velocity);
}

void KeplerOrbit::get_state_vectors_at_eccentric_anomaly(double const E, Vector3d &position, Vector3d &velocity) const
{
    double const trueAnomaly = get_true_anomaly_from_eccentric(E);
    double const r = get_orbiting_radius(m_params.eccentricity, m_params.semiMajorAxis, trueAnomaly);
    Vector3d radial;
    Vector3d transverse;
    Vector3d outOfPlane;
    get_fixed_reference_frame(radial, transverse, outOfPlane);

    position = r * (cos(trueAnomaly) * radial + sin(trueAnomaly) * transverse);
    if (is_elliptic())
    {
        velocity = (std::sqrt(m_params.gravitationalParameter * m_params.semiMajorAxis) / r) * 
            (-sin(E) * radial + std::sqrt(1 - m_params.eccentricity * m_params.eccentricity) * cos(E) * transverse);
    }
    else
    {
        double const v_r = (m_params.gravitationalParameter / m_params.angularMomentum * m_params.eccentricity * sin(trueAnomaly));
        double const v_perp = (m_params.angularMomentum / r);
        Vector3d const radusDir = position.normalized();
        Vector3d const perpDir = cross(outOfPlane, radusDir).normalized();

        velocity = v_r * radusDir + v_perp * perpDir;
    }
}

void KeplerOrbit::rebase_epoch(const double newEpoch)
{
    double const new_E0 = get_eccentric_anomaly(newEpoch);
    double const new_m0 = new_E0 - m_params.eccentricity * sin(new_E0);
    Vector3d newPos;
    Vector3d newVel;
    get_state_vectors_at_eccentric_anomaly(new_E0, newPos, newVel);
    m_params.meanAnomalyAtEpoch = new_m0;
}

Vector3d KeplerOrbit::get_acceleration(Vector3d const& radius) const
{
    double const a = m_params.gravitationalParameter / radius.dot();
    return -a * radius.normalized();
}

std::optional<double> KeplerOrbit::get_apoapsis() const
{
    if (m_params.eccentricity < 1.0)
    {
        return m_params.semiMajorAxis * (1 + m_params.eccentricity);
    }
    return std::nullopt;
}

std::optional<double> KeplerOrbit::get_period() const
{
    double const a = m_params.semiMajorAxis;
    if (m_params.eccentricity < 1.0)
    {
        return 2.0 * PI * std::sqrt(a * a * a / m_params.gravitationalParameter);
    }
    return std::nullopt;
}

double KeplerOrbit::get_periapsis() const
{
    return m_params.semiMajorAxis * (1 - m_params.eccentricity);
}

KeplerOrbit KeplerOrbit::from_initial_conditions(Vector3d const radius, Vector3d const velocity, double const epoch, double const GM)
{
    Vector3d const h = cross(radius, velocity);
    Vector3d const eVec = (cross(velocity, h) / GM) - radius.normalized();
    double const e = eVec.length();

    // Vector pointing towards the ascending node
    Vector3d n = cross(Vector3d(0.0, 0.0, 1.0), h);

    double const semiMajorAxis = 1.0 / (2.0 / radius.length() - velocity.dot() / GM);

    // Latitude of ascending node
    double LAN = acos(std::clamp(n.x() / n.length(), -1.0, 1.0));
    if (n.y() < 0)
    {
        LAN = 2.0 * PI - LAN;
    }
    if (n.length() <= KINDA_SMALL_NUMBER)
    {
        LAN = 0.0;
    }

    double const inclination = acos(h.z() / h.length());

    // Compute true anomaly v and argument of periapsis w
    double v;
    double w;
    if (e > KINDA_SMALL_NUMBER)
    {
        // True anomaly
        double m = dot(eVec, radius) / (e * radius.length());
        v = acos(std::clamp(m, -1.0, 1.0));
        if (dot(radius, velocity) < 0)
        {
            v = 2.0 * PI - v;
        }
        if (m >= 1.0)
        {
            v = 0.0;
        }

        // Argument of periapsis (angle between ascending node and periapsis)
        w = acos(std::clamp(dot(n, eVec) / (e * n.length()), -1.0, 1.0));
        if (eVec.z() < 0)
        {
            w = 2.0 * PI - w;
        }
        if (e <= KINDA_SMALL_NUMBER)
        {
            w = 0.0;
        }
    }
    else
    {
        // If e == 0, the orbit is circular and has no periapsis, so we use argument of latitude as a substitute for argument of periapsis
        // see https://en.wikipedia.org/wiki/True_anomaly#From_state_vectors
        double m = dot(n, radius) / (n.length() * radius.length());
        v = acos(std::clamp(m, -1.0, 1.0));
        if (radius.z() < 0)
        {
            v = 2.0 * PI - v;
        }
        if (m >= 1.0 - KINDA_SMALL_NUMBER)
        {
            v = 0.0;
        }

        // This may not be the correct value for w to use in this case
        w = 0.0;
    }

    // Compute eccentric anomaly from true anomaly
    // use that to find mean anomaly
    double M0, E;
    if (e > 1.0 + KINDA_SMALL_NUMBER)
    {
        double const F = true_anomaly_to_hyperbolic_eccentric_anomaly(e, v);
        M0 = e * sinh(F) - F;
        E = F;
    }
    else if (e >= 1.0 - KINDA_SMALL_NUMBER)
    {
        // parabolic orbit, todo
        M0 = 0.5 * tan(v / 2.0) + (1 / 6.0) * std::pow(tan(v / 2.0), 3);
        E = v;
    }
    else if (e > KINDA_SMALL_NUMBER)
    {
        E = true_anomaly_to_eccentric_anomaly(e, v);
        M0 = E - e * sin(E);
    }
    else
    {
        // circular orbit, todo
        E = v;
        M0 = E - e * sin(E);
    }

    KeplerOrbitParams params{
        .semiMajorAxis = semiMajorAxis,
        .eccentricity = e,
        .inclination = inclination,
        .argumentOfPeriapsis = w,
        .longitudeOfAscendingNode = LAN,
        .meanAnomalyAtEpoch = M0,
        .epoch = epoch,
        .gravitationalParameter = GM,
        .angularMomentum = h.length()
    };

    return KeplerOrbit(params); 
}

} // namespace osp
