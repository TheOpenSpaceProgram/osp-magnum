/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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

#include "../Universe.h"

#include <map>

//#include <Magnum/Math/Color.h>
//#include <Magnum/PixelFormat.h>
//#include <Magnum/SceneGraph/Camera.h>

#include "../types.h"
//#include "../scene.h"

#include "../Resource/Package.h"
#include "../Resource/PrototypePart.h"

//#include "../Active/ActiveScene.h"
//#include "../OSPApplication.h"
//#include "../UserInputHandler.h"
//#include "../Active/FtrNewtonBody.h"

namespace osp::universe
{


class OSPMagnum;
class SatActiveArea;


struct UCompActiveArea
{
    //active::ActiveEnt m_camera;

    unsigned m_sceneIndex;
};


class SatActiveArea : public CommonTypeSat<SatActiveArea, UCompActiveArea>
{

public:
    SatActiveArea(Universe& universe);
    ~SatActiveArea() = default;

    /**
     * Setup magnum scene and sets m_loadedActive to true.
     * @return only 0 for now
     */
    //int activate(OSPApplication& app);

    /**
     * Do actual drawing of scene. Call only on context thread.
     */
    void draw_gl();
    
    /**
     * Load nearby satellites, Maybe request a floating origin translation,
     * then calls update_physics of ActiveScene
     */
    void update_physics(float deltaTime);

    std::string get_name() { return "ActiveArea"; };

private:

    //bool m_loadedActive;

    //ActiveEnt m_camera;

    //std::shared_ptr<ActiveScene> m_scene;

    //UserInputHandler& m_userInput;

};

}
