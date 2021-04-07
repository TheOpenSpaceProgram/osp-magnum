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
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Attribute.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Shaders/VertexColor.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Containers/ArrayViewStl.h>

#include <osp/Universe.h>
#include <osp/Trajectories/NBody.h>

using namespace adera::active;
using namespace osp;
using namespace osp::active;
using namespace osp::universe;
using Magnum::Vector4ui;
using Magnum::Vector3ui;
using Magnum::Vector2ui;
using Magnum::Color4;
using Magnum::Int;
using Magnum::UnsignedInt;
using Magnum::GL::Buffer;
using Magnum::GL::Shader;
using Magnum::GL::BufferUsage;

Vector3 SysMap::universe_to_render_space(osp::Vector3s v3s)
{
    constexpr double units_per_m = 1024.0;
    float x = static_cast<double>(v3s.x() / units_per_m) / 1e6;
    float y = static_cast<double>(v3s.y() / units_per_m) / 1e6;
    float z = static_cast<double>(v3s.z() / units_per_m) / 1e6;
    return Vector3{x, y, z};
}

void SysMap::add_functions(ActiveScene& rScene)
{
    rScene.debug_update_add(rScene.get_update_order(),
        "SystemMap", "", "", &update_map);

    configure_render_passes(rScene);
}

void SysMap::configure_render_passes(ActiveScene& rScene)
{
    // Temporary setup
    using Magnum::Shaders::VertexColor3D;
    rScene.get_context_resources().add<VertexColor3D>("vert_color_shader");

    auto& mapdata = rScene.reg_emplace<MapRenderData>(rScene.hier_get_root(), rScene, 100, 100);

    mapdata.m_points = std::vector<MapRenderData::ColorVert>(0);
    mapdata.m_pointBuffer.setData(mapdata.m_points);
    mapdata.m_pointMesh
        .setPrimitive(Magnum::GL::MeshPrimitive::Points)
        .setCount(0)
        .addVertexBuffer(mapdata.m_pointBuffer, 0,
            VertexColor3D::Position{},
            VertexColor3D::Color4{});


    // Point pass
    rScene.debug_render_add(rScene.get_render_order(),
        "points_pass", "", "",
        [](ActiveScene& rScene, ACompCamera& rCamera)
        {
            using namespace Magnum;
            using Magnum::GL::Renderer;

            GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

            auto& reg = rScene.get_registry();
            auto& resources = rScene.get_context_resources();

            auto& data = reg.get<MapRenderData>(rScene.hier_get_root());
            auto shader = resources.get<VertexColor3D>("vert_color_shader");
            
            Renderer::setPointSize(2.0f);

            Matrix4 transform = rCamera.m_projection * rCamera.m_inverse;

            (*shader)
                .setTransformationProjectionMatrix(transform)
                .draw(data.m_pointMesh);
        });
}

void SysMap::update_map(ActiveScene& rScene)
{
    auto& rUni = rScene.get_application().get_universe();
    auto& reg = rUni.get_reg();

    MapRenderData& renderData = rScene.reg_get<MapRenderData>(rScene.hier_get_root());

    auto view = reg.view<UCompTransformTraj, ACompMapVisible>();
    for (auto [sat, traj, vis] : view.each())
    {
        size_t pointIndex{0};
        auto itr = renderData.m_pathMapping.find(sat);
        if (itr == renderData.m_pathMapping.end())
        {
            // Add point
            pointIndex = renderData.m_points.size();
            renderData.m_pathMapping.emplace(sat, pointIndex);
            renderData.m_points.push_back({Vector3{}, Color4{traj.m_color, 1.0}});
        }
        else
        {
            pointIndex = itr->second;
        }
        renderData.m_points[pointIndex].m_pos =
            universe_to_render_space(traj.m_position);
    }

    renderData.m_pointBuffer.setData(renderData.m_points, BufferUsage::DynamicDraw);
    renderData.m_pointMesh.setCount(renderData.m_points.size());
}

void MapUpdateCompute::update_map(
    size_t numPoints, Buffer& pointBuffer,
    size_t numPaths,  Buffer& pathMetadata,
    size_t numPathVerts, Buffer& pathVertBuffer,
    size_t numPathIndices, Buffer& pathIndexBuffer)
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
    using namespace Magnum;
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

void MapUpdateCompute::bind_raw_position_data(Buffer& data)
{
    data.bind(Buffer::Target::ShaderStorage,
        static_cast<Int>(EBufferBinding::RawInput));
}

void MapUpdateCompute::bind_point_locations(Buffer& points)
{
    points.bind(Buffer::Target::ShaderStorage,
        static_cast<Int>(EBufferBinding::PointVerts));
}

void MapUpdateCompute::bind_path_vert_data(Buffer& pathVerts)
{
    pathVerts.bind(Buffer::Target::ShaderStorage,
        static_cast<Int>(EBufferBinding::PathData));
}

void MapUpdateCompute::bind_path_index_data(Buffer& pathIndices)
{
    pathIndices.bind(Buffer::Target::ShaderStorage,
        static_cast<Int>(EBufferBinding::PathIndices));
}

void MapUpdateCompute::bind_path_metadata(Magnum::GL::Buffer& data)
{
    data.bind(Buffer::Target::ShaderStorage,
        static_cast<Int>(EBufferBinding::PathsInfo));
}

MapRenderData::MapRenderData(ActiveScene& scene, size_t maxPoints, size_t maxPathVertices)
    : m_maxPoints(maxPoints), m_maxPathVerts(maxPathVertices)
{
    assert(maxPoints < std::numeric_limits<GLuint>::max());
    assert(maxPathVertices < std::numeric_limits<GLuint>::max());
}

void ProcessMapCoordsCompute::process(
    Buffer& rawInput, size_t inputCount,
    Buffer& dest, size_t destOffset)
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
    using namespace Magnum;
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

void ProcessMapCoordsCompute::bind_input_buffer(Buffer& input)
{
    input.bind(Buffer::Target::ShaderStorage,
        static_cast<Int>(BufferBinding::RawInput));
}

void ProcessMapCoordsCompute::bind_output_buffer(Buffer& output)
{
    output.bind(Buffer::Target::ShaderStorage,
        static_cast<Int>(BufferBinding::Output));
}
