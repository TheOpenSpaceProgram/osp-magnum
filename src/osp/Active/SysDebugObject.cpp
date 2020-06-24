#include "SysDebugObject.h"

using namespace osp;

DebugObject::DebugObject(ActiveScene &scene, ActiveEnt ent) :
    m_scene(scene),
    m_ent(ent)
{

}

SysDebugObject::SysDebugObject(ActiveScene &scene) :
        m_scene(scene)
{

}
