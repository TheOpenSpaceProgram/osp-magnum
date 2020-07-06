#pragma once

#include <map>

#include <Magnum/Math/Color.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/PixelFormat.h>
//#include <Magnum/SceneGraph/Camera.h>


#include "../types.h"
#include "../Satellite.h"
//#include "../scene.h"

#include "../Resource/Package.h"
#include "../Resource/PrototypePart.h"

//#include "../Active/ActiveScene.h"

#include "../Active/activetypes.h"

#include "../OSPApplication.h"
#include "../UserInputHandler.h"
//#include "../Active/NewtonPhysics.h"

//#include "../Active/FtrNewtonBody.h"

namespace osp
{


class OSPMagnum;
class SatActiveArea;

typedef int (*LoadStrategy)(SatActiveArea& area, SatelliteObject& loadMe);

class SatActiveArea : public SatelliteObject
{

    friend OSPMagnum;

public:
    SatActiveArea(UserInputHandler& userInput);
    ~SatActiveArea();

    static Id const& get_id_static()
    {
        static Id id{ typeid(SatActiveArea).name() };
        return id;
    }

    Id const& get_id() override
    {
        return get_id_static();
    }

    bool is_activatable() const override { return false; }

    ActiveScene* get_scene() { return m_scene.get(); }

    ActiveEnt get_camera() { return m_camera; }

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
    void activate_func_add(Id const* id, LoadStrategy function);

    /**
     * Attempt to load a satellite
     * @return status, zero for no error
     */
    int activate_satellite(Satellite& sat);

    bool is_loaded_active() { return m_loadedActive; }

private:

    std::vector<SatelliteObject*> m_activatedSats;
    std::map<Id const*, LoadStrategy> m_loadFunctions;

    bool m_loadedActive;

    ActiveEnt m_camera;

    Magnum::GL::Mesh *m_bocks;
    std::unique_ptr<Magnum::Shaders::Phong> m_shader;

    std::shared_ptr<ActiveScene> m_scene;

    //GroupFtrNewtonBody m_newtonBodies;
    ActiveEnt m_debug_aEnt;

    UserInputHandler& m_userInput;

};





}
