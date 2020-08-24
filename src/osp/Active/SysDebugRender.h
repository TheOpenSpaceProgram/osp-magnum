#pragma once

#include <Magnum/Math/Color.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Shaders/Phong.h>

#include "../types.h"
#include "activetypes.h"

namespace osp::active
{

struct CompDrawableDebug
{
    Magnum::GL::Mesh* m_mesh;
    Magnum::Shaders::Phong* m_shader;
    Magnum::Color4 m_color;
};

class SysDebugRender : public IDynamicSystem
{
public:
    SysDebugRender(ActiveScene &rScene);
    ~SysDebugRender() = default;

    void draw(CompCamera const& camera);

private:
    ActiveScene &m_scene;

    RenderOrderHandle m_renderDebugDraw;
};

}
