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

#include "kepler.h"
#include <algorithm>
#include <cassert>


using Magnum::Math::cross;
using Magnum::Math::dot;

namespace osp
{

    constexpr double KINDA_SMALL_NUMBER = 1.0E-8;

    /**
     * @brief Wrap an angle to the range [0, 2π), useful for keplerian orbits
     */
    inline constexpr double wrap_angle(double angle)
    {
        double wrapped = std::fmod(angle, 2.0 * M_PI);
        if (wrapped < 0)
        {
            wrapped += 2.0 * M_PI;
        }
        return wrapped;
    }

    double stumpff_2(double z)
    {
        if (z > 0)
        {
            return (1 - std::cos(std::sqrt(z))) / z;
        }
        else if (z < 0)
        {
            return (std::cosh(std::sqrt(-z)) - 1) / -z;
        }
        else
        {
            return 1.0 / 2.0;
        }
    }

    double stumpff_3(double z)
    {
        if (z > 0)
        {
            return (std::sqrt(z) - std::sin(std::sqrt(z))) / std::pow(z, 1.5);
        }
        else if (z < 0)
        {
            return (std::sinh(std::sqrt(-z)) - std::sqrt(-z)) / std::pow(-z, 1.5);
        }
        else
        {
            return 1.0 / 6.0;
        }
    }

    double universal_kepler(double chi, double r_0, double v_0, double alpha, double delta_t, double mu)
    {
        double z = alpha * chi * chi;
        double firstTerm = r_0 * v_0 / std::sqrt(mu) * chi * chi * stumpff_2(z);
        double secondTerm = (1 - alpha * r_0) * chi * chi * chi * stumpff_3(z);
        double thirdTerm = r_0 * chi;
        double fourthTerm = std::sqrt(mu) * delta_t;
        return firstTerm + secondTerm + thirdTerm - fourthTerm;
    }

    double d_universal_kepler_d_chi(double chi, double r_0, double v_0, double alpha, double delta_t, double mu)
    {
        double z = alpha * chi * chi;
        double firstTerm = r_0 * v_0 / std::sqrt(mu) * chi * (1 - z * stumpff_3(z));
        double secondTerm = (1.0 - alpha - r_0) * chi * chi * stumpff_2(z);
        double thirdTerm = r_0;
        return firstTerm + secondTerm + thirdTerm;
    }

    double solve_kepler_universal(double r_0, double v_0, double semiMajorAxis, double delta_t, double mu, int steps)
    {
        // https://orbital-mechanics.space/time-since-periapsis-and-keplers-equation/universal-variables.html
        // This seems to converge disastrously slowly
        // The author recommends using Laguerre's method for faster convergence 
        double alpha = 1.0 / semiMajorAxis;
        double chi = std::sqrt(mu) * std::abs(alpha) * delta_t;
        double delta;
        do
        {
            double f_chi = universal_kepler(chi, r_0, v_0, alpha, delta_t, mu);
            double df_chi = d_universal_kepler_d_chi(chi, r_0, v_0, alpha, delta_t, mu);
            delta = f_chi / df_chi;
            chi = chi - delta;
            --steps;
        } while (std::abs(delta) > 1.0E-10 && steps > 1);
        return chi;
    }

    double solve_kepler_elliptic(double meanAnomaly, double eccentricity, int steps)
    {
        const double tolerance = 1.0E-10;
        if (std::abs(eccentricity) < 1.0E-9)
        {
            return meanAnomaly;
        }

        double delta;
        double E = meanAnomaly;

        // use newton's method
        // todo: implement something which converges more quickly
        // we really shouldn't need to do 20000 steps
        do
        {
            double f_E = E - eccentricity * std::sin(E) - meanAnomaly;
            double df_E = 1.0 - eccentricity * std::cos(E);

            delta = f_E / df_E;
            E = E - delta;
            
            // Not strictly necessary but sometimes this fails to converge without it
            E = wrap_angle(E);
            --steps;
        } while (std::abs(delta) > tolerance && steps > 1);
        return E;
    }

    double solve_kepler_hyperbolic(double meanAnomaly, double eccentricity, int steps)
    {
        const double tolerance = 1.0E-10;

        double delta;
        double E = std::log(std::abs(meanAnomaly));

        // use newton's method
        do
        {
            double f_E = eccentricity * std::sinh(E) - E - meanAnomaly;
            double df_E = eccentricity * std::cosh(E) - 1.0;

            delta = f_E / df_E;
            E = E - delta;
            --steps;
        } while (std::abs(delta) > tolerance && steps > 1);
        return E;
    }

    double eccentric_anomaly_to_true_anomaly(const double &ecc, const double &E)
    {
        // This formula supposedly has better numerical stability than the one below
        double beta = ecc / (1 + std::sqrt(1 - ecc * ecc));
        return E + 2.0 * std::atan2(beta * std::sin(E), 1 - beta * std::cos(E));

        // return 2.0 * std::atan2(std::sqrt(1 + ecc) * std::sin(E / 2.0), std::sqrt(1 - ecc) * std::cos(E / 2.0));
    }

    double hyperbolic_eccentric_anomaly_to_true_anomaly(const double &ecc, const double &H)
    {
        return 2 * atan(sqrt((ecc + 1) / (ecc - 1)) * tanh(H / 2));
    }

    double mean_motion(double mu, double a)
    {
        return sqrt(mu / (a * a * a));
    }

    double get_orbiting_radius(double ecc, double semiMajorAxis, double trueAnomaly)
    {
        return semiMajorAxis * (1 - ecc * ecc) / (1 + ecc * std::cos(trueAnomaly));
    }

    Vector3d KeplerOrbit::get_position_and_velocity(double time, Vector3d &velocity) const
    {
        double trueAnomaly = 0.0;
        double E = 0.0;
        if (m_eccentricity <= 1.0)
        {
#if 1 // Solve for eccentric anomaly
            double nu = mean_motion(m_gravitationalParameter, m_semiMajorAxis);
            double Mt = m_meanAnomalyAtEpoch + (time - m_epoch) * nu;
            Mt = wrap_angle(Mt);
            E = solve_kepler_elliptic(Mt, m_eccentricity, 20000);
            trueAnomaly = eccentric_anomaly_to_true_anomaly(m_eccentricity, E);
#else // Use universal kepler solver
            
            double chi = solve_kepler_universal(m_r0, m_v0, m_semiMajorAxis, time - m_epoch, m_gravitationalParameter, 200);
            E = chi / std::sqrt(m_semiMajorAxis) + m_E0;
            E = wrap_angle(E);
            trueAnomaly = eccentric_anomaly_to_true_anomaly(m_eccentricity, E);
            // trueAnomaly = wrap_angle(trueAnomaly);
#endif
        }
        else if (is_hyperbolic())
        {
#if 1 // Solve for hyperbolic eccentric anomaly
            double nu = mean_motion(m_gravitationalParameter, -m_semiMajorAxis);
            double Mt = m_meanAnomalyAtEpoch + (time - m_epoch) * nu;
            // Mt = wrap_angle(Mt);
            E = solve_kepler_hyperbolic(Mt, m_eccentricity, 20000);
            trueAnomaly = hyperbolic_eccentric_anomaly_to_true_anomaly(m_eccentricity, E);
#else // Use universal kepler solver
            
            double chi = solve_kepler_universal(m_r0, m_v0, m_semiMajorAxis, time - m_epoch, m_gravitationalParameter, 2000);
            double F = chi / std::sqrt(-m_semiMajorAxis) + m_F0;

            trueAnomaly = hyperbolic_eccentric_anomaly_to_true_anomaly(m_eccentricity, F);
#endif
        }
        double r = m_semiMajorAxis * (1 - m_eccentricity * m_eccentricity) / (1 + m_eccentricity * std::cos(trueAnomaly));

        double a = m_argumentOfPeriapsis;
        double b = m_longitudeOfAscendingNode;
        double c = m_inclination;

        Vector3d radial = Vector3d(std::cos(a) * std::cos(b) - std::sin(a) * std::sin(b) * std::cos(c),
                                   std::cos(a) * std::sin(b) + std::sin(a) * std::cos(b) * std::cos(c),
                                   std::sin(a) * std::sin(c));

        Vector3d transverse = Vector3d(-std::sin(a) * std::cos(b) - std::cos(a) * std::sin(b) * std::cos(c),
                                       -std::sin(a) * std::sin(b) + std::cos(a) * std::cos(b) * std::cos(c),
                                       std::cos(a) * std::sin(c));

        Vector3d outOfPlane = Vector3d(std::sin(b) * std::sin(c),
                                       -std::cos(b) * std::sin(c),
                                       std::cos(c));

        Vector3d newPosition = r * (std::cos(trueAnomaly) * radial + std::sin(trueAnomaly) * transverse);
        if (m_eccentricity < 1.0)
        {
            Vector3d newVelocity = (std::sqrt(m_gravitationalParameter * m_semiMajorAxis) / r) * (-std::sin(E) * radial + (std::sqrt(1 - m_eccentricity * m_eccentricity) * std::cos(E)) * transverse);
            velocity = newVelocity;
        }
        else
        {
            double v_r = (m_gravitationalParameter / m_h * m_eccentricity * std::sin(trueAnomaly));
            double v_perp = (m_h / r);
            Vector3d radusDir = newPosition.normalized();
            Vector3d perpDir = cross(outOfPlane, radusDir).normalized();

            Vector3d newVelocity = v_r * radusDir + v_perp * perpDir;
            velocity = newVelocity;
        }
        return newPosition;
    }

    Vector3d KeplerOrbit::get_acceleration(Vector3d radius) const
    {
        double a = m_gravitationalParameter / radius.dot();
        return -a * radius.normalized();
    }

    std::optional<double> KeplerOrbit::get_apoapsis() const
    {
        if (m_eccentricity < 1.0)
        {
            return m_semiMajorAxis * (1 + m_eccentricity);
        }
        return std::nullopt;
    }

    std::optional<double> KeplerOrbit::get_period() const
    {
        if (m_eccentricity < 1.0)
        {
            return 2.0 * M_PI * std::sqrt(m_semiMajorAxis * m_semiMajorAxis * m_semiMajorAxis / m_gravitationalParameter);
        }
        return std::nullopt;
    }

    double KeplerOrbit::get_periapsis() const
    {
        return m_semiMajorAxis * (1 - m_eccentricity);
    }

    KeplerOrbit KeplerOrbit::from_initial_conditions(Vector3d radius, Vector3d velocity, double epoch, double GM)
    {
        Vector3d h = cross(radius, velocity);
        Vector3d eVec = (cross(velocity, h) / GM) - radius.normalized();
        double e = eVec.length();

        // Vector pointing towards the ascending node
        Vector3d n = cross(Vector3d(0.0, 0.0, 1.0), h);

        double semiMajorAxis = 1.0 / (2.0 / radius.length() - velocity.dot() / GM);

        // Latitude of ascending node
        double LAN = acos(std::clamp(n.x() / n.length(), -1.0, 1.0));
        if (n.y() < 0)
        {
            LAN = 2.0 * M_PI - LAN;
        }
        if (n.length() <= KINDA_SMALL_NUMBER)
        {
            LAN = 0.0;
        }

        const double inclination = acos(h.z() / h.length());

        // Compute true anomaly v and argument of periapsis w
        double v;
        double w;
        if (e > KINDA_SMALL_NUMBER)
        {
            // True anomaly
            double m = dot(eVec, radius) / (e * radius.length());
            v = std::acos(std::clamp(m, -1.0, 1.0));
            if (dot(radius, velocity) < 0)
            {
                v = 2.0 * M_PI - v;
            }
            if (m >= 1.0)
            {
                v = 0.0;
            }

            // Argument of periapsis (angle between ascending node and periapsis)
            w = acos(std::clamp(dot(n, eVec) / (e * n.length()), -1.0, 1.0));
            if (eVec.z() < 0)
            {
                w = 2.0 * M_PI - w;
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
            v = std::acos(std::clamp(m, -1.0, 1.0));
            if (radius.z() < 0)
            {
                v = 2.0 * M_PI - v;
            }
            if (m >= 1.0 - KINDA_SMALL_NUMBER)
            {
                v = 0.0;
            }
            w = 0.0;
        }

        // Compute eccentric anomaly (E) from true anomaly (F)
        // use that to find mean anomaly (M0)
        double M0;
        double E;
        if (e > 1.0+KINDA_SMALL_NUMBER)
        {
            const double F = 2 * std::atanh(std::clamp(std::sqrt((e - 1) / (e + 1)) * std::tan(v / 2.0), -1.0, 1.0));
            M0 = e * sinh(F) - F;
            E = F;
        }
        else if (e >= 1.0 - KINDA_SMALL_NUMBER) {
            // parabolic orbit, todo
            M0 = 0.5 * std::tan(v/2.0) + (1/6.0) * std::pow(std::tan(v/2.0), 3);
            E = v;
        }
        else if (e > KINDA_SMALL_NUMBER)
        {
            E = 2 * std::atan(std::sqrt((1 - e) / (1 + e)) * std::tan(v / 2.0));
            M0 = E - e * std::sin(E);
        }
        else
        {
            // circular orbit, todo
            E = v;
            M0 = E - e * std::sin(E);
        }

        return KeplerOrbit(semiMajorAxis, e, inclination, w, LAN, M0, epoch, GM, radius.length(), velocity.length(), E, h.length());
    }

} // namespace osp