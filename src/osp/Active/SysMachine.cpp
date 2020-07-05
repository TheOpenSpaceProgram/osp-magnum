#include "SysMachine.h"

#include "ActiveScene.h"

using namespace osp;

//AbstractSysMachine::AbstractSysMachine(ActiveScene& scene)
//{

//}

Machine::Machine(ActiveEnt ent, bool enable) : m_enable(enable),  m_ent(ent) {}

void Machine::doErase()
{
    list()->cut(this);
}
