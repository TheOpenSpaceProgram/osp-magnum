#pragma once

#include "../Universe.h"

#include <map>

#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>
//#include <Magnum/SceneGraph/Camera.h>


#include "../types.h"
//#include "../scene.h"

#include "../Resource/Package.h"
#include "../Resource/PrototypePart.h"

//#include "../Active/ActiveScene.h"

#include "../Active/activetypes.h"

#include "../OSPApplication.h"
#include "../UserInputHandler.h"
//#include "../Active/NewtonPhysics.h"

//#include "../Active/FtrNewtonBody.h"

namespace osp::universe
{


class OSPMagnum;
class SatActiveArea;

typedef int (*LoadStrategy)(ActiveScene& scene, SatActiveArea& area,
                            Satellite areaSat, Satellite loadMe);

namespace ucomp
{

struct ActiveArea
{
    std::map<ITypeSatellite*, LoadStrategy> m_loadFunctions;
    std::vector<Satellite*> m_activatedSats;

    ActiveEnt m_camera;

    unsigned m_sceneIndex;
};

}


class SatActiveArea : public ITypeSatellite
{

    friend OSPMagnum;

public:
    SatActiveArea(UserInputHandler& userInput);
    ~SatActiveArea() = default;

    /**
     * Setup magnum scene and sets m_loadedActive to true.
     * @return only 0 for now
     */
    int activate(OSPApplication& app);

    /**
     * Do actual drawing of scene. Call only on context thread.
     */
    void draw_gl();
    
    /**
     * Load nearby satellites, Maybe request a floating origin translation,
     * then calls update_physics of ActiveScene
     */
    void update_physics(float deltaTime);

    /**
     * Add a loading strategy to add support for loading a type of satellite 
     * @param id
     * @param function
     */
    void activate_func_add(ITypeSatellite*, LoadStrategy function);

    virtual std::string get_name() { return "ActiveArea"; };

private:

    //bool m_loadedActive;

    //ActiveEnt m_camera;

    //std::shared_ptr<ActiveScene> m_scene;

    //UserInputHandler& m_userInput;

};





}
