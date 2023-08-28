/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "links.h"

using namespace osp;

using osp::link::MachTypeReg_t;
using osp::link::MachTypeId;

namespace adera
{

float thruster_influence(Vector3 const pos, Vector3 const dir, Vector3 const cmdLin, Vector3 const cmdAng) noexcept
{
    using Magnum::Math::cross;
    using Magnum::Math::dot;

    float influence = 0.0f;

    if (cmdAng.dot() > 0.0f)
    {
        Vector3 const torque = cross(pos, dir).normalized();
        influence += dot(torque, cmdAng.normalized());
    }

    if (cmdLin.dot() > 0.0f)
    {
        influence += dot(dir, cmdLin.normalized());
    }

    if (influence < 0.01f)
    {
        return 0.0f; // Ignore small contributions
    }

    if (Magnum::Math::isNan(influence))
    {
        return 0.0f;
    }

    return std::clamp(influence, 0.0f, 1.0f);
}


} // namespace adera
