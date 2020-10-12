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

    // true when the ActiveArea is moving
    bool m_inMotion;
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
