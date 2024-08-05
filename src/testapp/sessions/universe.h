#if 0
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

#include "../scenarios.h"

#include "osp/universe/universe.h"
#include <osp/drawing/drawing.h>

namespace testapp::scenes
{

// Universe Scenario

/**
 * @brief Core Universe struct with addressable Coordinate Spaces
 */
osp::Session setup_uni_core(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::PipelineId             updateOn);

/**
 * @brief Represents the physics scene's presence in a Universe
 */
osp::Session setup_uni_sceneframe(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         uniCore);

/**
 * @brief Unrealistic planets test, allows SceneFrame to move around and get captured into planets
 */
osp::Session setup_uni_testplanets(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         uniCore,
        osp::Session const&         uniScnFrame);


/**
 * @brief Draw universe, specifically designed for setup_uni_test_planets
 */
osp::Session setup_testplanets_draw(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         sceneRenderer,
        osp::Session const&         cameraCtrl,
        osp::Session const&         commonScene,
        osp::Session const&         uniCore,
        osp::Session const&         uniScnFrame,
        osp::Session const&         uniTestPlanets,
        osp::draw::MaterialId const matPlanets,
        osp::draw::MaterialId const matAxis);

// Solar System Scenario

struct CoSpaceNBody
{
    osp::universe::TypedStrideDesc<float> mass;
    osp::universe::TypedStrideDesc<float> radius;
    osp::universe::TypedStrideDesc<Magnum::Color3> color;
};

/**
 * @brief Initializes planet information, position, mass etc...
 */
osp::Session setup_solar_system_testplanets(
    osp::TopTaskBuilder& rBuilder,
    osp::ArrayView<entt::any>   topData,
    osp::Session const& solarSystemCore,
    osp::Session const& solarSystemScnFrame);


/**
 * @brief Draw Solar System, specifically designed for setup_solar_system_test_planets
 */
osp::Session setup_solar_system_planets_draw(
    osp::TopTaskBuilder& rBuilder,
    osp::ArrayView<entt::any>   topData,
    osp::Session const& windowApp,
    osp::Session const& sceneRenderer,
    osp::Session const& cameraCtrl,
    osp::Session const& commonScene,
    osp::Session const& solarSystemCore,
    osp::Session const& solarSystemScnFrame,
    osp::Session const& solarSystemTestPlanets,
    osp::draw::MaterialId const matPlanets);
}
#endif
