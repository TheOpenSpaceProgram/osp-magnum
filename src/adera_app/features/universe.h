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

#include <osp/framework/builder.h>
#include <osp/universe/universe.h>
#include <osp/drawing/drawing.h>


namespace adera
{

// Universe Scenario

/**
 * @brief Core Universe struct with addressable Coordinate Spaces
 */
extern osp::fw::FeatureDef const ftrUniverseCore;


/**
 * @brief Represents the physics scene's presence in a Universe
 */
extern osp::fw::FeatureDef const ftrUniverseSceneFrame;


/**
 * @brief Unrealistic planets test, allows SceneFrame to move around and get captured into planets
 */
extern osp::fw::FeatureDef const ftrUniverseTestPlanets;


/**
 * @brief Draw universe, specifically designed for setup_uni_test_planets
 */
extern osp::fw::FeatureDef const ftrUniverseTestPlanetsDraw;


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
extern osp::fw::FeatureDef const ftrSolarSystem;

struct PlanetDrawParams
{
    osp::draw::MaterialId planetMat;
    osp::draw::MaterialId axisMat;
};

/**
 * @brief Draw Solar System, specifically designed for ftrSolarSystemPlanets
 */
extern osp::fw::FeatureDef const ftrSolarSystemDraw;

} // namespace adera

