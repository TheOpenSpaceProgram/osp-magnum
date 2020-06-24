#pragma once

#include <memory>

#include "activetypes.h"

namespace osp
{

class DebugObject
{

public:

    DebugObject(ActiveScene &scene, ActiveEnt ent);

    virtual void update_sensor() = 0;
    virtual void update_physics(float delta) = 0;

private:
    ActiveScene &m_scene;
    ActiveEnt m_ent;
};

struct CompDebugObject
{
    std::unique_ptr<DebugObject> m_obj;
};

class SysDebugObject
{
public:
    SysDebugObject(ActiveScene &scene);

    void update_sensor();
    void update_physics(float delta);

private:
    ActiveScene &m_scene;
};

}
