/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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
#include "physics.h"

#include "../feature_interfaces.h"

#include <osp/activescene/basic.h>
#include <osp/activescene/physics_fn.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/core/Resources.h>

#include <Magnum/Trade/Trade.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>

using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp;

namespace adera
{


FeatureDef const ftrPhysics = feature_def("Physics", [] (
        FeatureBuilder          &rFB,
        Implement<FIPhysics>    phys,
        DependOn<FIMainApp>     mainApp,
        DependOn<FIScene>       scn,
        DependOn<FICommonScene> comScn)
{
    rFB.pipeline(phys.pl.mass)      .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(phys.pl.physUpdate).parent(mainApp.loopblks.mainLoop);

    rFB.data_emplace< ACtxPhysics > (phys.di.phys);

    rFB.task()
        .name       ("Delete Physics components")
        .sync_with  ({comScn.pl.activeEntDelete(UseOrRun), phys.pl.mass(Delete)})
        .args       ({         phys.di.phys,              comScn.di.activeEntDel })
        .func       ([] (ACtxPhysics &rPhys, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        SysPhysics::update_delete_phys(rPhys, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });
}); // ftrPhysics


} // namespace adera
