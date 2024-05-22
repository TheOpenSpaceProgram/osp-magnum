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

// IWYU pragma: begin_exports
#include <Magnum/Types.h>

#include <Magnum/Math/Matrix.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Matrix3.h>

#include <Magnum/Math/Vector.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>

#include <Magnum/Math/Quaternion.h>
// IWYU pragma: end_exports

namespace osp
{

using Matrix3       = Magnum::Math::Matrix3<Magnum::Float>;
using Matrix4       = Magnum::Math::Matrix4<Magnum::Float>;

using Vector2i      = Magnum::Math::Vector2<Magnum::Int>;

using Vector3u      = Magnum::Math::Vector3<Magnum::UnsignedInt>;

using Vector3l      = Magnum::Math::Vector3<Magnum::Long>;

using Vector2       = Magnum::Math::Vector2<Magnum::Float>;
using Vector3       = Magnum::Math::Vector3<Magnum::Float>;
using Vector4       = Magnum::Math::Vector4<Magnum::Float>;

using Vector3d      = Magnum::Math::Vector3<Magnum::Double>;

using Quaternion    = Magnum::Math::Quaternion<Magnum::Float>;
using Quaterniond   = Magnum::Math::Quaternion<Magnum::Double>;

using Rad           = Magnum::Math::Rad<Magnum::Float>;
using Radd          = Magnum::Math::Rad<Magnum::Double>;
using Deg           = Magnum::Math::Deg<Magnum::Float>;

using Magnum::Math::ZeroInit;
using Magnum::Math::IdentityInit;


} // namespace osp
