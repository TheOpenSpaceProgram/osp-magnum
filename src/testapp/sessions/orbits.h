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

#include "../scenarios.h"

#include <osp/scientific/kepler.h>

namespace testapp::scenes
{
    
/**
 * @brief Setup planets for orbit scenario, initialized with random velocities and positions
 */
osp::Session setup_orbit_planets(
        osp::TopTaskBuilder& rBuilder,
        osp::ArrayView<entt::any> topData,
        osp::Session &uniCore,
        osp::Session &uniScnFrame);

/**
 * @brief Setup pure kepler orbital dynamics for the given planets.
 *        Planets will orbit around the center of their coordinate space.
 */
osp::Session setup_orbit_dynamics_kepler(
        osp::TopTaskBuilder& rBuilder,
        osp::ArrayView<entt::any> topData,
        osp::Session &uniCore,
        osp::Session &uniPlanets,
        osp::Session &uniScnFrame);

} // namespace testapp::scenes
