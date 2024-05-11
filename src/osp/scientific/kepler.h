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
 * @brief Holds parameters to fully define a Keplerian (ideal 2-body) orbit
 */
struct KeplerOrbit
{
private:
    
    static constexpr double KINDA_SMALL_NUMBER = 1.0E-8;
    // These are private as we may want to change the internal representation in the future

    struct KeplerOrbitParams
    {
        double semiMajorAxis;
        double eccentricity;
        double inclination;
        double argumentOfPeriapsis;
        double longitudeOfAscendingNode;
        double meanAnomalyAtEpoch;
        double epoch;
        double gravitationalParameter;
        double angularMomentum;
    };

    KeplerOrbitParams m_params;

    KeplerOrbit(KeplerOrbitParams const params): m_params(params) {}

    /**
     * @brief Get the eccentric anomaly at a given time
     */
    double get_eccentric_anomaly(double const time) const;

    /**
     * @brief Get the true anomaly at a given eccentric anomaly
     *
     * @note This is not a very satisfying interface for this function,
     * but having a "get_true_anomaly_at_time" would require double the computation if you wanted both
     */
    double get_true_anomaly_from_eccentric(double const eccentric_anomaly) const;

    /**
     * @brief Compute the position, velocity of the orbit at a given eccentric anomaly
     */
    void get_state_vectors_at_eccentric_anomaly(double const true_anomaly, Vector3d &position, Vector3d &velocity) const;

public:
    /**
     * @brief Compute the position, velocity of the orbit at a given time
     */
    void get_state_vectors_at_time(double const time, Vector3d &position, Vector3d &velocity) const;

    /**
     * @brief Compute the acceleration of the orbit at a given position
     */
    Vector3d get_acceleration(Vector3d const &radius) const;

    /**
     * @brief Returns a fixed reference frame for the orbit, invariant of time
     */
    void get_fixed_reference_frame(Vector3d &radial, Vector3d &transverse, Vector3d &outOfPlane) const;

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

    constexpr bool is_elliptic() const noexcept { return m_params.eccentricity < 1.0 - KINDA_SMALL_NUMBER; }
    constexpr bool is_circular() const noexcept { return m_params.eccentricity <= KINDA_SMALL_NUMBER; }
    constexpr bool is_hyperbolic() const noexcept { return m_params.eccentricity > 1.0 + KINDA_SMALL_NUMBER; }
    constexpr bool is_parabolic() const noexcept { return m_params.eccentricity <= KINDA_SMALL_NUMBER + 1.0 && m_params.eccentricity >= 1.0 - KINDA_SMALL_NUMBER; }

    /**
     * @brief Recompute the orbit to be centralized around a new epoch
     * This might be worth doing once in a great while for numerical stability
     */
    void rebase_epoch(double newEpoch);

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
    static KeplerOrbit from_initial_conditions(Vector3d const radius, Vector3d const velocity, double const epoch = 0.0, double const gravitationalParameter = 1.0);
};

} // namespace osp
