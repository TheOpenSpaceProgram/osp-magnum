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
#pragma once

#include "osp/core/math_types.h"

#include <optional>

namespace osp
{

/**
 * @brief Implementation of the analytical solution to Kepler's equation using the Classical Orbital Elements
 */
struct KeplerOrbit
{
private:
static constexpr double KINDA_SMALL_NUMBER = 1.0E-8;

public:

    /**
     * Set of elements to fully define an orbit
     */
    struct KeplerOrbitParams {
        double semiMajorAxis;
        double eccentricity;
        double inclination;
        double argumentOfPeriapsis;
        double longitudeOfAscendingNode;
        double meanAnomalyAtEpoch;
        double gravitationalParameter;
        double epoch;
    };

    KeplerOrbitParams m_params;

    KeplerOrbit(KeplerOrbitParams const params) : m_params(params) {}

    /**
     * @brief Get the true anomaly at a given time
     */
    double get_true_anomaly(double const time) const;

    /**
     * @brief Compute the position, velocity of the orbit at a given true anomaly
     */
    void get_state_vectors_at_true_anomaly(double const trueAnomaly, Vector3d &position, Vector3d &velocity) const;

    /**
     * @brief Compute the position, velocity of the orbit at a given time
     */
    void get_state_vectors_at_time(double const time, Vector3d &position, Vector3d &velocity) const;

    /**
     * @brief Compute the acceleration of the orbit at a given position
     */
    Vector3d get_acceleration(Vector3d const &radius) const;

    /**
     * @brief Returns the euler angle rotation defined by argument of periapsis, longitude of ascending node, and inclination
     */
    void get_uvw_vectors(Vector3d &u, Vector3d &v, Vector3d &w) const;

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

    /**
     * @brief Returns true if this orbit is elliptic *or* circular (e<1.0)
     */
    constexpr bool is_elliptic() const noexcept { 
        return m_params.eccentricity < 1.0; 
    }

    /**
     * @brief Returns true if this orbit is circular (e==0.0), a special case of elliptic orbits
     */
    constexpr bool is_circular() const noexcept { 
        return m_params.eccentricity <= KINDA_SMALL_NUMBER; 
    }

    /**
     * @brief Returns true if this orbit is hyperbolic (e>1.0)
     */ 
    constexpr bool is_hyperbolic() const noexcept { 
        return m_params.eccentricity > 1.0; 
    }

    /**
     * @brief Returns true if this orbit is parabolic (e==1.0)
     * 
     * @note Parabolic orbits will likely not be supported by this class
     * If supporting parabolic orbits is a requirement, look into implementing Modified Equinoctial Elements
     */
    constexpr bool is_parabolic() const noexcept {
        return m_params.eccentricity == 1.0;
    }

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
    static KeplerOrbit from_initial_conditions(Vector3d const radius, Vector3d velocity, double const gravitationalParameter, double const epoch = 0.0);
    
    /**
     * @brief Solve for the next time the radius = r, if such a time exists
     */
    std::optional<double> get_next_time_radius_equals(const double r, const double currentTime) const;
};

} // namespace osp
