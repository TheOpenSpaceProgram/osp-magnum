/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include <newtondynamics_physics/SysNewton.h>
#include <newtondynamics_physics/ospnewton.h>

template<class TRIANGLE_IT_T>
void debug_create_tri_mesh_static(
        osp::active::ActiveScene& rScene, osp::active::ACompShape& rShape,
        osp::active::ActiveEnt chunkEnt,
        TRIANGLE_IT_T const& start, TRIANGLE_IT_T const& end)
{
    // TODO: this is actually horrendously slow and WILL cause issues later on.
    //       Tree collisions aren't made for real-time loading. Consider
    //       manually hacking up serialized data instead of add face, or use
    //       Newton's dgAABBPolygonSoup stuff directly

    using osp::Vector3;

    osp::active::ActiveReg_t &rReg = rScene.get_registry();

    auto &rWorldCtx = rReg.ctx<ospnewton::ACtxNwtWorld>();
    NewtonWorld const* pNwtWorld = rWorldCtx.m_nwtWorld.get();
    NewtonCollision const* pTree = NewtonCreateTreeCollision(pNwtWorld, 0);

    NewtonTreeCollisionBeginBuild(pTree);

    Vector3 triangle[3];

    for (TRIANGLE_IT_T it = start; it != end; it += 3)
    {
        triangle[0] = *reinterpret_cast<Vector3 const*>((it + 0).position());
        triangle[1] = *reinterpret_cast<Vector3 const*>((it + 1).position());
        triangle[2] = *reinterpret_cast<Vector3 const*>((it + 2).position());

        NewtonTreeCollisionAddFace(
                pTree, 3, reinterpret_cast<float*>(triangle), sizeof(float) * 3, 0);

    }

    NewtonTreeCollisionEndBuild(pTree, 2);

    rReg.emplace<ospnewton::ACompNwtCollider>(chunkEnt, pTree);

}
