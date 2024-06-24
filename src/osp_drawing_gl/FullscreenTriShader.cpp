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
#include "FullscreenTriShader.h"

#include <Magnum/GL/Version.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Texture.h>

// used by attachShaders
#include <Corrade/Containers/Iterable.h>  // for Containers::Iterable
#include <Corrade/Containers/Reference.h>

using namespace osp;
using namespace Magnum;

FullscreenTriShader::FullscreenTriShader()
{
    GL::Shader vert{GL::Version::GL430, GL::Shader::Type::Vertex};
    GL::Shader frag{GL::Version::GL430, GL::Shader::Type::Fragment};
    vert.addFile("OSPData/adera/Shaders/FullscreenTri.vert");
    frag.addFile("OSPData/adera/Shaders/FullscreenTri.frag");

    CORRADE_INTERNAL_ASSERT_OUTPUT(vert.compile() && frag.compile());
    attachShaders({vert, frag});
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    setUniform(static_cast<Int>(EUniformPos::FramebufferSampler),
        static_cast<Int>(ETextureSlot::Framebuffer));
}

void FullscreenTriShader::display_texure(GL::Mesh& surface, GL::Texture2D& texture)
{
    set_framebuffer(texture);
    draw(surface);
}

FullscreenTriShader& FullscreenTriShader::set_framebuffer(GL::Texture2D& rTex)
{
    rTex.bind(static_cast<Int>(ETextureSlot::Framebuffer));
    return *this;
}
