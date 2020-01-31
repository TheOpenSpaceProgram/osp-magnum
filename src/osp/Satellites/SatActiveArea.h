#pragma once


#include <Magnum/GL/Mesh.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/Shaders/Phong.h>

#include "../Types.h"
#include "../Satellite.h"

class OSPMagnum;

namespace osp
{

class SatActiveArea : public SatelliteObject
{

    friend OSPMagnum;

public:
    SatActiveArea();
    ~SatActiveArea();

    bool is_loaded_active() { return m_loadedActive; }


    /**
     * Do actual drawing of scene. Call only on context thread.
     */
    void draw_gl();

    Scene3D& get_scene() { return m_scene; }


    /**
     * Load objects into scene. Call only on context thread.
     */
    int on_load();

private:


    bool m_loadedActive;
    Scene3D m_scene;
    Magnum::SceneGraph::Camera3D* m_camera;
    Magnum::SceneGraph::DrawableGroup3D m_drawables;


    // temporary test variables only
    std::unique_ptr<Magnum::GL::Mesh> m_bocks;
    std::unique_ptr<Magnum::Shaders::Phong> m_shader;

};

}
