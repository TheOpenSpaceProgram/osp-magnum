#pragma once

#include "physics.h"

namespace osp::active
{

// do this thing when this actually gets complex and repetitive
//template <class FORCEFIELD_T, class COMP_T>
//class SysForceField : public IDynamicSystem
//{
//public:
//    SysForceField();
//    ~SysForceField() = default;

//    void update_force();

//private:

//    FORCEFIELD_T m_field;
//};

class SysForceField : public IDynamicSystem
{
};

struct BaseACompFF
{
    // put stuff here like filters
};

struct ACompFFGravity : public BaseACompFF
{
    float m_Gmass;

};

class SysFFGravity : public SysForceField
{
public:
    SysFFGravity(ActiveScene &scene);
    ~SysFFGravity() = default;

    void update_force();

private:


    ActiveScene &m_scene;
    SysPhysics &m_physics;

    UpdateOrderHandle m_updateForce;
};



}
