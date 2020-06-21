#include "SysMachine.h"

#include "ActiveScene.h"

using namespace osp;

//AbstractSysMachine::AbstractSysMachine(ActiveScene& scene)
//{

//}

Machine::Machine(ActiveEnt ent) : m_ent(ent) {}

void Machine::doErase()
{
    list()->cut(this);
}
