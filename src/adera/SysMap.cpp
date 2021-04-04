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

#include "SysMap.h"
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>
#include <Corrade/Containers/Reference.h>

#include <osp/Universe.h>
#include <osp/Trajectories/NBody.h>

using namespace Magnum;
using namespace adera::active;
using namespace osp::active;
using namespace osp::universe;

Vector3 SysMap::universe_to_render_space(osp::Vector3s v3s)
{
    constexpr int64_t units_per_m = 1024ll;
    float x = static_cast<double>(v3s.x() / units_per_m) / 1e6;
    float y = static_cast<double>(v3s.y() / units_per_m) / 1e6;
    float z = static_cast<double>(v3s.z() / units_per_m) / 1e6;
    return Vector3{x, y, z};
}

void SysMap::add_functions(ActiveScene& rScene)
{
    rScene.debug_update_add(rScene.get_update_order(),
        "SystemMap", "", "", &update_map);
}

void SysMap::update_map(ActiveScene& rScene)
{
    auto& rUni = rScene.get_application().get_universe();
    auto& reg = rUni.get_reg();

    MapRenderData& renderData = rScene.reg_get<MapRenderData>(rScene.hier_get_root());

}

void MapUpdateCompute::update_map(
    size_t numPoints, GL::Buffer& pointBuffer,
    size_t numPaths,  GL::Buffer& pathMetadata,
    size_t numPathVerts, GL::Buffer& pathVertBuffer,
    size_t numPathIndices, GL::Buffer& pathIndexBuffer)
{
    bind_point_locations(pointBuffer);
    bind_path_vert_data(pathVertBuffer);
    bind_path_index_data(pathIndexBuffer);
    bind_path_metadata(pathMetadata);
    set_uniform_counts(numPoints, numPaths, numPathVerts, numPathIndices);

    Vector3ui nGroups{1, static_cast<Magnum::UnsignedInt>(numPaths), 1};
    dispatchCompute(nGroups);
}

void MapUpdateCompute::init()
{
    GL::Shader prog{GL::Version::GL430, GL::Shader::Type::Compute};
    prog.addFile("OSPData/adera/Shaders/MapUpdate.comp");

    CORRADE_INTERNAL_ASSERT_OUTPUT(prog.compile());
    attachShader(prog);
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());
}

void MapUpdateCompute::set_uniform_counts(
    size_t numPoints, size_t numPaths, size_t numPathVerts, size_t numPathIndices)
{
    setUniform(static_cast<Int>(EUniformPos::BlockCounts),
        Vector4ui{
            static_cast<UnsignedInt>(numPoints),
            static_cast<UnsignedInt>(numPaths),
            static_cast<UnsignedInt>(numPathVerts),
            static_cast<UnsignedInt>(numPathIndices)});
}

void MapUpdateCompute::bind_raw_position_data(GL::Buffer& data)
{
    data.bind(GL::Buffer::Target::ShaderStorage,
        static_cast<Int>(EBufferBinding::RawInput));
}

void MapUpdateCompute::bind_point_locations(GL::Buffer& points)
{
    points.bind(GL::Buffer::Target::ShaderStorage,
        static_cast<Int>(EBufferBinding::PointVerts));
}

void MapUpdateCompute::bind_path_vert_data(GL::Buffer& pathVerts)
{
    pathVerts.bind(GL::Buffer::Target::ShaderStorage,
        static_cast<Int>(EBufferBinding::PathData));
}

void MapUpdateCompute::bind_path_index_data(GL::Buffer& pathIndices)
{
    pathIndices.bind(GL::Buffer::Target::ShaderStorage,
        static_cast<Int>(EBufferBinding::PathIndices));
}

void MapUpdateCompute::bind_path_metadata(Magnum::GL::Buffer& data)
{
    data.bind(GL::Buffer::Target::ShaderStorage,
        static_cast<Int>(EBufferBinding::PathsInfo));
}

MapRenderData::MapRenderData(ActiveScene& scene, size_t maxPoints, size_t maxPathVertices)
    : m_maxPoints(maxPoints), m_maxPathVerts(maxPathVertices)
{
    assert(maxPoints < std::numeric_limits<GLuint>::max());
    assert(maxPathVertices < std::numeric_limits<GLuint>::max());
}

void ProcessMapCoordsCompute::process(
    GL::Buffer& rawInput, size_t inputCount,
    GL::Buffer& dest, size_t destOffset)
{
    set_input_counts(inputCount, destOffset);
    bind_input_buffer(rawInput);
    bind_output_buffer(dest);

    constexpr size_t blockLength = 32;
    size_t numBlocks = (inputCount / blockLength)
        + ((inputCount % blockLength) > 0) ? 1 : 0;
    dispatchCompute(Vector3ui{static_cast<UnsignedInt>(numBlocks), 1, 1});
}

void ProcessMapCoordsCompute::init()
{
    GL::Shader prog{GL::Version::GL430, GL::Shader::Type::Compute};
    prog.addFile("OSPData/adera/Shaders/MapPositionsConverter.comp");

    CORRADE_INTERNAL_ASSERT_OUTPUT(prog.compile());
    attachShader(prog);
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());
}

void ProcessMapCoordsCompute::set_input_counts(size_t nInputPoints, size_t outputOffset)
{
    setUniform(static_cast<Int>(UniformPos::Counts),
        Vector2ui{
            static_cast<UnsignedInt>(nInputPoints),
            static_cast<UnsignedInt>(outputOffset)});
}

void ProcessMapCoordsCompute::bind_input_buffer(GL::Buffer& input)
{
    input.bind(GL::Buffer::Target::ShaderStorage,
        static_cast<Int>(BufferBinding::RawInput));
}

void ProcessMapCoordsCompute::bind_output_buffer(GL::Buffer& output)
{
    output.bind(GL::Buffer::Target::ShaderStorage,
        static_cast<Int>(BufferBinding::Output));
}
