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

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Attribute.h>

namespace osp::active
{

class RenderTexture : public Magnum::GL::AbstractShaderProgram
{
public:
    // Vertex attribs
    typedef Magnum::GL::Attribute<0, Magnum::Vector2> Position;
    typedef Magnum::GL::Attribute<1, Magnum::Vector2> TextureCoordinates;

    // Outputs
    enum : Magnum::UnsignedInt
    {
        ColorOutput = 0
    };

    RenderTexture();

    void render_texure(Magnum::GL::Mesh& surface, Magnum::GL::Texture2D& texture);
private:
    // Uniforms
    enum class UniformPos : Magnum::Int
    {
        FramebufferSampler = 0
    };

    // Texture2D slots
    enum class TextureSlot : Magnum::Int
    {
        FramebufferSlot = 0
    };

    // Hide irrelevant calls
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    RenderTexture& set_framebuffer(Magnum::GL::Texture2D& rTex);
};

}  // namespace osp::active
