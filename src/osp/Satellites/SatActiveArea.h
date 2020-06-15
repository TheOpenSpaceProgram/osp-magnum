#pragma once

#include <map>

#include <Magnum/Math/Color.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/PixelFormat.h>
//#include <Magnum/SceneGraph/Camera.h>


#include "../../types.h"
#include "../Satellite.h"
//#include "../scene.h"

#include "../Resource/Package.h"
#include "../Resource/PrototypePart.h"

#include "../Active/ActiveScene.h"

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

    bool is_loadable() const override { return false; }

    ActiveScene* get_scene() { return m_scene.get(); }


    /**
     * Setup magnum scene and sets m_loadedActive to true.
     * @return only 0 for now
     */
    int activate();

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
     * @param function
     */
    template<class T>
    void load_func_add(LoadStrategy function);

    /**
     * Create a Physical Part from a PrototypePart and put it in the world
     * @param part the part to instantiate
     * @param rootParent Entity to put part into
     * @return Pointer to object created
     */
    ActiveEnt part_instantiate(PrototypePart& part,
                                  ActiveEnt rootParent);

    /**
     * Attempt to load a satellite
     * @return status, zero for no error
     */
    int load_satellite(Satellite& sat);

    bool is_loaded_active() { return m_loadedActive; }

private:

    std::vector<SatelliteObject*> m_loadedSats;
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



template<class T>
void SatActiveArea::load_func_add(LoadStrategy function)
{
    m_loadFunctions[&(T::get_id_static())] = function;
}

}
