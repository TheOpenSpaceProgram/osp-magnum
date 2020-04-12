#pragma once

#include <map>

#include <Magnum/Math/Color.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/SceneGraph/Camera.h>

#include <Newton.h>

#include "../types.h"
#include "../Satellite.h"
#include "../scene.h"

#include "../Resource/Package.h"
#include "../Resource/PartPrototype.h"



namespace osp
{

class OSPMagnum;
class SatActiveArea;

typedef int (*LoadStrategy)(SatActiveArea& area, SatelliteObject& loadMe);


class SatActiveArea : public SatelliteObject
{

    friend OSPMagnum;

public:
    SatActiveArea();
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



    /**
     * Setup magnum scene and sets m_loadedActive to true.
     * @return only 0 for now
     */
    int activate();

    /**
     * Do actual drawing of scene. Call only on context thread.
     */
    void draw_gl();

    template<class T>
    void load_func_add(LoadStrategy function);

    Object3D* part_instantiate(PartPrototype& part);

    /**
     * Attempt to load a satellite
     * @return
     */
    int load_satellite(Satellite& sat);

    bool is_loaded_active() { return m_loadedActive; }

    Scene3D& get_scene() { return m_scene; }



private:

    std::map<Id const*, LoadStrategy> m_loadFunctions;

    bool m_loadedActive;
    Scene3D m_scene;
    Magnum::SceneGraph::Camera3D* m_camera;
    Magnum::SceneGraph::DrawableGroup3D m_drawables;

    // temporary test variables only
    Object3D* m_partTest;
    Magnum::GL::Mesh *m_bocks;
    std::unique_ptr<Magnum::Shaders::Phong> m_shader;

    // Newton dynamics stuff
    NewtonWorld* const m_nwtWorld;

};



template<class T>
void SatActiveArea::load_func_add(LoadStrategy function)
{
    m_loadFunctions[&(T::get_id_static())] = function;
}

}
