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
#pragma once

#include "osp/core/math_types.h"

#include <optional>

namespace osp
{

/**
 * @brief Holds parameters to fully define a Kepler orbit
 */
struct KeplerOrbit
{
private:
    // These are private as we may want to change the representation in the future
    double m_semiMajorAxis;
    double m_eccentricity;
    double m_inclination;
    double m_argumentOfPeriapsis;
    double m_longitudeOfAscendingNode;
    double m_meanAnomalyAtEpoch;
    double m_epoch;
    double m_r0;
    double m_v0;
    double m_h;

    union {
        double m_F0; 
        double m_E0;
    };

    // Gravitational parameter of the central body (G*M)
    double m_gravitationalParameter;

    KeplerOrbit(double semiMajorAxis,
                double eccentricity,
                double inclination,
                double argumentOfPeriapsis,
                double longitudeOfAscendingNode,
                double meanAnomalyAtEpoch,
                double epoch,
                double gravitationalParameter,
                double r0,
                double v0,
                double m_E0,
                double h) : m_semiMajorAxis(semiMajorAxis),
                                                    m_eccentricity(eccentricity),
                                                    m_inclination(inclination),
                                                    m_argumentOfPeriapsis(argumentOfPeriapsis),
                                                    m_longitudeOfAscendingNode(longitudeOfAscendingNode),
                                                    m_meanAnomalyAtEpoch(meanAnomalyAtEpoch),
                                                    m_epoch(epoch),
                                                    m_gravitationalParameter(gravitationalParameter),
                                                    m_r0(r0),
                                                    m_v0(v0),
                                                    m_E0(m_E0),
                                                    m_h(h) {}

public:
    /**
     * @brief Compute the position of the orbit at a given time
     */
    Vector3d get_position_and_velocity(double time, Vector3d& velocity) const;

    /**
     * @brief Compute the acceleration of the orbit at a given position
     */
    Vector3d get_acceleration(Vector3d radius) const;

    /**
     * @brief Find Apoapsis of orbit
     * 
     * @return std::optional<double> Apoapsis distance, if eccentricity < 1
     */
    std::optional<double> get_apoapsis() const;

    /**
     * @brief Find period of orbit if elliptic
     */
    std::optional<double> get_period() const;

    /**
     * @brief Find Periapsis of orbit
     */
    double get_periapsis() const;


    bool is_elliptic() const { return m_eccentricity < 1.0; }
    bool is_hyperbolic() const { return m_eccentricity > 1.0; }
    bool is_parabolic() const { return m_eccentricity == 1.0; }


    /**
     * @brief Compute the Kepler orbit parameters from initial conditions
     *
     * @param radius                 [in] Initial radius vector relative to central body
     * @param velocity               [in] Initial velocity vector relative to central body
     * @param epoch                  [in] Time of the initial conditions
     * @param gravitationalParameter [in] Gravitational parameter of the central body
     *
     * @return KeplerOrbit
     */
    static KeplerOrbit from_initial_conditions(Vector3d radius, Vector3d velocity, double epoch = 0.0, double gravitationalParameter = 1.0);

};

} // namespace osp