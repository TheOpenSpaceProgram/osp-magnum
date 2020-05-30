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
#include "../Resource/PartPrototype.h"

#include "../Active/ActiveScene.h"

#include "../UserInputHandler.h"
//#include "../Active/NewtonPhysics.h"

//#include "../Active/FtrNewtonBody.h"

namespace osp
{


class OSPMagnum;
class SatActiveArea;

typedef int (*LoadStrategy)(SatActiveArea& area, SatelliteObject& loadMe);

//using enum Magnum::Platform::Application::KeyEvent;
//using namespace Magnum::Platform;

using Magnum::Platform::Application;

class SatActiveArea : public SatelliteObject//, public GroupFtrNewtonBody
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
     * 
     */
    void update_physics(float deltaTime);

    //NewtonWorld* get_newton_world() { return m_nwtWorld; }

    /**
     * Add a loading strategy to add support for loading a type of satellite 
     * @param function
     */
    template<class T>
    void load_func_add(LoadStrategy function);

    /**
     * 
     * @param part
     * @return Pointer to object created
     */
    entt::entity part_instantiate(PartPrototype& part);

    /**
     * Attempt to load a satellite
     * @return
     */
    int load_satellite(Satellite& sat);

    bool is_loaded_active() { return m_loadedActive; }

    //Scene3D& get_scene() { return m_scene; }

    //Magnum::SceneGraph::DrawableGroup3D& get_drawables() { return m_drawables; }

    void input_key_press(Application::KeyEvent& event);
    void input_key_release(Application::KeyEvent& event);

    void input_mouse_press(Application::MouseEvent& event);
    void input_mouse_release(Application::MouseEvent& event);
    void input_mouse_move(Application::MouseMoveEvent& event);



private:

    std::map<Id const*, LoadStrategy> m_loadFunctions;

    bool m_loadedActive;
    //Scene3D m_scene;
    //Magnum::SceneGraph::Camera3D* m_camera;
    //Magnum::SceneGraph::DrawableGroup3D m_drawables;

    // temporary test variables only
    //Object3D* m_partTest;
    entt::entity m_camera;

    Magnum::GL::Mesh *m_bocks;
    std::unique_ptr<Magnum::Shaders::Phong> m_shader;

    std::shared_ptr<ActiveScene> m_scene;

    //GroupFtrNewtonBody m_newtonBodies;
    entt::entity m_debug_aEnt;

    UserInputHandler& m_userInput;

};



template<class T>
void SatActiveArea::load_func_add(LoadStrategy function)
{
    m_loadFunctions[&(T::get_id_static())] = function;
}

}
