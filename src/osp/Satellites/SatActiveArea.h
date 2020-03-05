#pragma once


#include <Magnum/Math/Color.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/SceneGraph/Camera.h>


#include "../types.h"
#include "../Satellite.h"
#include "../scene.h"

#include "../Resource/Package.h"
#include "../Resource/PartPrototype.h"

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

    Scene3D& get_scene() { return m_scene; }

    /**
     * Do actual drawing of scene. Call only on context thread.
     */
    void draw_gl();

    void part_instantiate(PartPrototype& part, Package& pack);

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
    Magnum::GL::Mesh *m_bocks;
    std::unique_ptr<Magnum::Shaders::Phong> m_shader;

};

}
