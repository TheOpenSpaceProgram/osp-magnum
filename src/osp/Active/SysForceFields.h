/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
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

    static const std::string smc_name;

    SysFFGravity(ActiveScene &scene);
    ~SysFFGravity() = default;

    void update_force();

private:


    ActiveScene &m_scene;
    //SysPhysics &m_physics;

    UpdateOrderHandle m_updateForce;
};



}
