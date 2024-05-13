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

using namespace osp;

// PI isn't standard, so define it here. Should it be added to constants.h?
static constexpr double PI = Magnum::Math::Constants<double>::pi();

/**
 * @brief Wrap an angle to the range [0, 2π)
 */
static double wrap_angle(double const angle)
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
    if (std::abs(eccentricity) <= 1.0E-8)
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
    double E = std::log(std::abs(meanAnomaly) + 0.1);

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

/**
 * @brief Convert eccentric anomaly (E) to true anomaly (ν) for elliptic orbits
 */
static double elliptic_eccentric_anomaly_to_true_anomaly(double const &ecc, double const &E)
{
    // This formula supposedly has better numerical stability than the one below
    double const beta = ecc / (1 + std::sqrt(1 - ecc * ecc));
    return E + 2.0 * atan2(beta * sin(E), 1 - beta * cos(E));

    // return 2.0 * atan2(std::sqrt(1 + ecc) * sin(E / 2.0), std::sqrt(1 - ecc) * cos(E / 2.0));
}

/**
 * @brief Convert hyperbolic eccentric anomaly (E) to true anomaly (ν)
 */
static double hyperbolic_eccentric_anomaly_to_true_anomaly(double const &ecc, double const &H)
{
    return 2 * atan(sqrt((ecc + 1) / (ecc - 1)) * tanh(H / 2));
}

/**
 * @brief Convert true anomaly (ν) to eccentric anomaly (E) for elliptic orbits
 */
static double true_anomaly_to_elliptic_eccentric_anomaly(double const &ecc, double const &v)
{
    return 2.0 * atan(std::sqrt((1 - ecc) / (1 + ecc)) * tan(v / 2.0));
}

/**
 * @brief Convert true anomaly (ν) to hyperbolic eccentric anomaly (E)
 */ 
static double true_anomaly_to_hyperbolic_eccentric_anomaly(double const &ecc, double const &v)
{
    return 2.0 * atanh(std::clamp(std::sqrt((ecc - 1) / (ecc + 1)) * tan(v / 2.0), -1.0, 1.0));
}

/**
 * @brief Compute mean motion 
 */
static double mean_motion(double const mu, double const a)
{
    return sqrt(std::abs(mu / (a * a * a)));
}

/**
 * @brief Compute the radius of an orbiting body at a given true anomaly
 */
static double get_orbiting_radius(double const ecc, double const semiMajorAxis, double const trueAnomaly)
{
    return semiMajorAxis * (1 - ecc * ecc) / (1 + ecc * cos(trueAnomaly));
}

/**
 * @brief Compute the elliptic eccentric anomaly at a given time since epoch
 */ 
static double time_to_elliptic_eccentric_anomaly(double const M0, double const eccentricity, double const semiMajorAxis, double const mu, double const timeSinceEpoch)
{
    double const nu = mean_motion(mu, semiMajorAxis);
    double Mt = M0 + timeSinceEpoch * nu;
    Mt = wrap_angle(Mt);
    return solve_kepler_elliptic(Mt, eccentricity, 20000);
}

/**
 * @brief Compute the hyperbolic eccentric anomaly at a given time since epoch
 */
static double time_to_hyperbolic_eccentric_anomaly(double const M0, double const eccentricity, double const semiMajorAxis, double const mu, double const timeSinceEpoch)
{
    double const nu = mean_motion(mu, -semiMajorAxis);
    double const Mt = M0 + timeSinceEpoch * nu;
    return solve_kepler_hyperbolic(Mt, eccentricity, 20000);
}

/**
 * @brief Compute the position and velocity vectors of an orbiting body at the given anomaly
 */
static void get_state_vectors(double   const eccentricAnomaly, 
                              double   const trueAnomaly, 
                              double   const eccentricity, 
                              double   const semiMajorAxis, 
                              double   const gravitationalParameter, 
                              Vector3d const u, 
                              Vector3d const v, 
                              Vector3d const w,
                              Vector3d      &position, 
                              Vector3d      &velocity)
{
    double const r = get_orbiting_radius(eccentricity, semiMajorAxis, trueAnomaly);
    position = r * (cos(trueAnomaly) * u + sin(trueAnomaly) * v);
    if (eccentricity < 1.0)
    {
        velocity = (std::sqrt(gravitationalParameter * semiMajorAxis) / r) * 
            (-sin(eccentricAnomaly) * u + std::sqrt(1 - eccentricity * eccentricity) * cos(eccentricAnomaly) * v);
    }
    else
    {
        double angularMomentum = std::sqrt(gravitationalParameter * -semiMajorAxis * (eccentricity * eccentricity - 1));

        double const v_r = (gravitationalParameter / angularMomentum * eccentricity * sin(trueAnomaly));
        double const v_perp = (angularMomentum / r);
        Vector3d const radusDir = position.normalized();
        Vector3d const perpDir = cross(w, radusDir).normalized();

        velocity = v_r * radusDir + v_perp * perpDir;
    }
}

} // namespace



namespace osp
{

double KeplerOrbit::get_true_anomaly(double const time) const
{
    if (is_elliptic())
    {
        double const E = time_to_elliptic_eccentric_anomaly(m_params.meanAnomalyAtEpoch, m_params.eccentricity, 
                    m_params.semiMajorAxis, m_params.gravitationalParameter, time - m_params.epoch);
        return elliptic_eccentric_anomaly_to_true_anomaly(m_params.eccentricity, E);
    }
    else
    {
        double const F = time_to_hyperbolic_eccentric_anomaly(m_params.meanAnomalyAtEpoch, m_params.eccentricity, 
                    m_params.semiMajorAxis, m_params.gravitationalParameter, time - m_params.epoch);
        return hyperbolic_eccentric_anomaly_to_true_anomaly(m_params.eccentricity, F);
    }
}

void KeplerOrbit::get_uvw_vectors(Vector3d &u, Vector3d &v, Vector3d &w) const
{
    double const a = m_params.argumentOfPeriapsis;
    double const b = m_params.longitudeOfAscendingNode;
    double const c = m_params.inclination;

    u = Vector3d(cos(a) * cos(b) - sin(a) * sin(b) * cos(c),
                 cos(a) * sin(b) + sin(a) * cos(b) * cos(c),
                 sin(a) * sin(c));

    v = Vector3d(-sin(a) * cos(b) - cos(a) * sin(b) * cos(c),
                 -sin(a) * sin(b) + cos(a) * cos(b) * cos(c),
                  cos(a) * sin(c));

    w = Vector3d(sin(b) * sin(c),
                -cos(b) * sin(c),
                 cos(c));
}

void KeplerOrbit::get_state_vectors_at_time(double const time, Vector3d &position, Vector3d &velocity) const
{
    double E;
    double trueAnomaly;
    if (is_elliptic())
    {
        E = time_to_elliptic_eccentric_anomaly(m_params.meanAnomalyAtEpoch, m_params.eccentricity, 
                    m_params.semiMajorAxis, m_params.gravitationalParameter, time - m_params.epoch);
        trueAnomaly = elliptic_eccentric_anomaly_to_true_anomaly(m_params.eccentricity, E);
    }
    else
    {
        E = time_to_hyperbolic_eccentric_anomaly(m_params.meanAnomalyAtEpoch, m_params.eccentricity, 
                    m_params.semiMajorAxis, m_params.gravitationalParameter, time - m_params.epoch);
        trueAnomaly = hyperbolic_eccentric_anomaly_to_true_anomaly(m_params.eccentricity, E);
    }
    Vector3d u;
    Vector3d v;
    Vector3d w;
    get_uvw_vectors(u, v, w);
    get_state_vectors(E, trueAnomaly, m_params.eccentricity, m_params.semiMajorAxis, 
                m_params.gravitationalParameter, u, v, w, position, velocity);
}

void KeplerOrbit::get_state_vectors_at_true_anomaly(double const trueAnomaly, Vector3d &position, Vector3d &velocity) const
{
    double E;
    if (is_elliptic())
    {
        E = true_anomaly_to_elliptic_eccentric_anomaly(m_params.eccentricity, trueAnomaly);
    }
    else
    {
        E = true_anomaly_to_hyperbolic_eccentric_anomaly(m_params.eccentricity, trueAnomaly);
    }
    Vector3d u;
    Vector3d v;
    Vector3d w;
    get_uvw_vectors(u, v, w);
    get_state_vectors(E, trueAnomaly, m_params.eccentricity, m_params.semiMajorAxis, 
                m_params.gravitationalParameter, u, v, w, position, velocity);
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

KeplerOrbit KeplerOrbit::from_initial_conditions(Vector3d const radius, Vector3d velocity, double const GM, double const epoch)
{
    // Don't allow perfectly zero velocity
    if (velocity.length() < KINDA_SMALL_NUMBER)
    {
        velocity += Vector3d(std::copysign(KINDA_SMALL_NUMBER, velocity.x()), 0.0, 0.0);
    }

    Vector3d const h = cross(radius, velocity);
    Vector3d eVec = (cross(velocity, h) / GM) - radius.normalized();
    double e = eVec.length();

    // Vector pointing towards the ascending node
    Vector3d n = cross(Vector3d(0.0, 0.0, 1.0), h);

    double semiMajorAxis = 1.0 / (2.0 / radius.length() - velocity.dot() / GM);

    // Mess things up a little bit to avoid creating a parabolic orbit
    if (std::abs(e - 1.0) < KINDA_SMALL_NUMBER) {
        if (e < 1.0) {
            e = 1.0 - KINDA_SMALL_NUMBER;
        } else {
            e = 1.0 + KINDA_SMALL_NUMBER;
        }
        eVec = e * eVec.normalized();
    }

    // Latitude of ascending node
    double LAN; 
    if (n.length() <= KINDA_SMALL_NUMBER)
    {
        LAN = 0.0;
    } else {
        LAN = acos(std::clamp(n.x() / n.length(), -1.0, 1.0));
    }
    if (n.y() < 0)
    {
        LAN = 2.0 * PI - LAN;
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

        if (n.length() <= KINDA_SMALL_NUMBER) {
            w = 0.0;
        } else {
            // Argument of periapsis (angle between ascending node and periapsis)
            w = acos(std::clamp(dot(n, eVec) / (e * n.length()), -1.0, 1.0));

            if (eVec.z() < 0)
            {
                w = 2.0 * PI - w;
            }
        }
    }
    else if (inclination > KINDA_SMALL_NUMBER)
    {
        // If e == 0, the orbit is circular and has no periapsis, so we use argument of latitude as a substitute for argument of periapsis
        // see https://en.wikipedia.org/wiki/True_anomaly#From_state_vectors
        double m = dot(n, radius) / (n.length() * radius.length());
        v = acos(std::clamp(m, -1.0, 1.0));
        if (radius.z() < 0)
        {
            v = 2.0 * PI - v;
        }
        w = 0.0;
    } else {
        // If both e and inclination are 0, the orbit has no argument of periapsis or latitude
        // Use true longitude instead:
        double m = radius.x() / radius.length();
        v = acos(std::clamp(m, -1.0, 1.0));
        if (velocity.x() > 0)
        {
            v = 2.0 * PI - v;
        }
        w = 0.0;
    }

    // Compute eccentric anomaly from true anomaly
    // use that to find mean anomaly
    double M0;
    if (e > 1.0)
    {
        double const F = true_anomaly_to_hyperbolic_eccentric_anomaly(e, v);
        M0 = e * sinh(F) - F;
    }
    else if (e < 1.0)
    {
        double const E = true_anomaly_to_elliptic_eccentric_anomaly(e, v);
        M0 = E - e * sin(E);
    }
    
    KeplerOrbitParams params{
        .semiMajorAxis = semiMajorAxis,
        .eccentricity = e,
        .inclination = inclination,
        .argumentOfPeriapsis = w,
        .longitudeOfAscendingNode = LAN,
        .meanAnomalyAtEpoch = M0,
        .gravitationalParameter = GM,
        .epoch = epoch,
    };

    return KeplerOrbit(params); 
}

std::optional<double> KeplerOrbit::get_next_time_radius_equals(const double r, const double currentTime) const {
    std::optional<double> apoapsis = get_apoapsis();
    if (apoapsis.has_value() && apoapsis.value() <= r) {
        return std::nullopt;
    }

    double a = m_params.semiMajorAxis;
    double e = m_params.eccentricity;
    double cos_f = (a - r - e*e*a) / (e*r);
    double trueAnomaly = std::acos(std::clamp(cos_f,-1.0,1.0));

    double T;
    double T2;
    if (is_elliptic()) {
        double E = true_anomaly_to_elliptic_eccentric_anomaly(e,trueAnomaly);
        double M = E - e * std::sin(E);
        double E2 = -E;
        double M2 = E2 - e * std::sin(E2);
        T = (M-m_params.meanAnomalyAtEpoch) * std::sqrt(a*a*a / m_params.gravitationalParameter);
        T2 = (M2-m_params.meanAnomalyAtEpoch) * std::sqrt(a*a*a / m_params.gravitationalParameter);
    } else {
        double F = true_anomaly_to_hyperbolic_eccentric_anomaly(e,trueAnomaly);
        double M = e * std::sinh(F) - F;
        double F2 = -F;
        double M2 = e * std::sinh(F2) - F2;
        T = -(M-m_params.meanAnomalyAtEpoch) * std::sqrt(-a*a*a / m_params.gravitationalParameter);
        T2 = -(M2-m_params.meanAnomalyAtEpoch) * std::sqrt(-a*a*a / m_params.gravitationalParameter);
    }

    double t = T+m_params.epoch;
    double t2 = T2+m_params.epoch;

    if (is_elliptic()) {
        t = std::fmod(t-currentTime, get_period().value());
        if (t < 0) {
            t += get_period().value();
        }
        t = t;
        t2 = std::fmod(t2-currentTime, get_period().value());   
        if (t2 < 0) {
            t2 += get_period().value();
        }
        t2 = t2;
    }
    
    if (t2 < 0 && t < 0) {
        return std::nullopt;
    }
    if (t2 < t && t2 >= 0) {
        return t2 + currentTime;
    }
    else {
        return t + currentTime;
    }
}

} // namespace osp
