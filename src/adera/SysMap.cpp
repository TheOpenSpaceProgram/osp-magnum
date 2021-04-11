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
#include <Magnum/Mesh.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Containers/ArrayViewStl.h>

#include <osp/Universe.h>
#include <osp/Trajectories/NBody.h>
#include <osp/CommonMath.h>
#include <adera/Shaders/MapTrailShader.h>

using namespace adera::active;
using namespace adera::shader;
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
    using Magnum::Shaders::VertexColor3D;
    auto& resources = rScene.get_context_resources();

    resources.add<VertexColor3D>("vert_color_shader");
    resources.add<MapTrailShader>("map_trail_shader");
    resources.add<MapUpdateCompute>("map_compute");
    resources.add<ProcessMapCoordsCompute>("map_preproccess");

    rScene.reg_emplace<MapRenderData>(rScene.hier_get_root());

    glPrimitiveRestartIndex(MapRenderData::smc_PRIMITIVE_RESTART);

    // Path pass
    rScene.debug_render_add(rScene.get_render_order(),
        "paths_pass", "", "",
        [](ActiveScene& rScene, ACompCamera& rCamera)
        {
            using namespace Magnum;
            using Magnum::GL::Renderer;

            GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

            auto& reg = rScene.get_registry();
            auto& resources = rScene.get_context_resources();

            auto& data = reg.get<MapRenderData>(rScene.hier_get_root());
            auto shader = resources.get<MapTrailShader>("map_trail_shader");

            Renderer::setPointSize(1.0f);
            Renderer::enable(Renderer::Feature::Blending);
            Renderer::setBlendFunction(
                Renderer::BlendFunction::SourceAlpha,
                Renderer::BlendFunction::OneMinusSourceAlpha);

            Matrix4 transform = rCamera.m_projection * rCamera.m_inverse;

            glEnable(GL_PRIMITIVE_RESTART);

            (*shader)
                .set_transform_matrix(transform)
                .draw(data.m_mesh);

            glDisable(GL_PRIMITIVE_RESTART);

            Renderer::disable(Renderer::Feature::Blending);
        });

    // Point pass
    rScene.debug_render_add(rScene.get_render_order(),
        "points_pass", "", "",
        [](ActiveScene& rScene, ACompCamera& rCamera)
        {
            using namespace Magnum;
            using Magnum::GL::Renderer;

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

void SysMap::register_system(ActiveScene& rScene)
{
    auto& rUni = rScene.get_application().get_universe();
    auto& reg = rUni.get_reg();
    auto& glres = rScene.get_context_resources();
    MapRenderData& renderData = rScene.reg_get<MapRenderData>(rScene.hier_get_root());

    // Clear data
    renderData.m_points.clear();
    renderData.m_pointMapping.clear();
    renderData.m_pathMetadata.clear();
    renderData.m_pathMapping.clear();

    // Enumerate total number of map objects and paths

    size_t pointIndex = 0;
    size_t pathIndex = 0;
    auto view = reg.view<UCompTransformTraj, ACompMapVisible>();
    for (auto [sat, traj, vis] : view.each())
    {
        renderData.m_points.emplace_back(Vector4{0.0}, Color4{traj.m_color, 1.0});
        renderData.m_pointMapping.emplace(sat, pointIndex);

        if (reg.has<UCompEmitsGravity>(sat))
        {
            // Body is fully dynamic; leave trail

            MapRenderData::PathMetadata pathInfo;
            pathInfo.m_pointIndex = pointIndex;
            pathInfo.m_startIdx = pathIndex * MapRenderData::smc_N_INDICES_PER_PATH;
            pathInfo.m_endIdx = pathInfo.m_startIdx + MapRenderData::smc_N_VERTS_PER_PATH - 1;
            pathInfo.m_nextIdx = pathInfo.m_startIdx;

            renderData.m_pathMetadata.push_back(std::move(pathInfo));
            renderData.m_pathMapping.emplace(sat, pathIndex);

            pathIndex++;
        }
        pointIndex++;
    }
    renderData.m_numPoints = pointIndex;
    renderData.m_numPaths = pathIndex;

    // Create point buffer, mesh

    renderData.m_pointBuffer.setData(renderData.m_points);
    renderData.m_pointMesh
        .setPrimitive(Magnum::GL::MeshPrimitive::Points)
        .setCount(renderData.m_points.size())
        .addVertexBuffer(renderData.m_pointBuffer, 0,
            Magnum::GL::Attribute<0, Vector4>{},
            Magnum::GL::Attribute<2, Color4>{});

    // Size path data

    size_t nTotalPathIndices = renderData.m_numPaths * MapRenderData::smc_N_INDICES_PER_PATH;
    /* Making the number of vertices equal to the number of indices keeps things easy by
     * making them line up. It wastes one vertex per path, but it's worth it for now.
     */
    size_t nTotalPathVerts = nTotalPathIndices;
    renderData.m_vertexData = std::vector<MapRenderData::ColorVert>(nTotalPathVerts,
        {Vector4{0.0}, Color4{0.0}});
    renderData.m_indexData.resize(renderData.m_numPaths * MapRenderData::smc_N_INDICES_PER_PATH);

    // Initialize index data
    for (auto& metadata : renderData.m_pathMetadata)
    {
        for (size_t i = metadata.m_startIdx; i < metadata.m_endIdx; i++)
        {
            renderData.m_indexData[i] = metadata.m_endIdx - i;
        }
        renderData.m_indexData[metadata.m_endIdx + 1] = MapRenderData::smc_PRIMITIVE_RESTART;
    }
    
    using Magnum::Shaders::VertexColor3D;

    // Create path buffers, mesh

    renderData.m_vertexBuffer.setData(renderData.m_vertexData);
    renderData.m_indexBuffer.setData(renderData.m_indexData);
    renderData.m_mesh
        .setPrimitive(Magnum::GL::MeshPrimitive::LineStrip)
        .setCount(nTotalPathVerts)
        .addVertexBuffer(renderData.m_vertexBuffer, 0,
            MapTrailShader::Position{},
            MapTrailShader::Color{})
        .setIndexBuffer(renderData.m_indexBuffer, 0, Magnum::MeshIndexType::UnsignedInt);

    // Create path metadata buffer

    renderData.m_pathMetadataBuffer.setData(
        renderData.m_pathMetadata, Magnum::GL::BufferUsage::StaticDraw);
}

#if 1
void SysMap::update_map(ActiveScene& rScene)
{
    auto& rUni = rScene.get_application().get_universe();
    auto& reg = rUni.get_reg();
    auto& glres = rScene.get_context_resources();

    MapRenderData& renderData = rScene.reg_get<MapRenderData>(rScene.hier_get_root());
    auto* nbody = rUni.get_traj<TrajNBody>(0);

    if (!renderData.m_isInitialized)
    {
        register_system(rScene);
        renderData.m_isInitialized = true;
    }

    DependRes<ProcessMapCoordsCompute> preproccess =
        glres.get<ProcessMapCoordsCompute>("map_preproccess");

    EvolutionTable::RawStepData state = nbody->get_latest_state();
    renderData.m_rawState.setData(state.m_data, Magnum::GL::BufferUsage::DynamicDraw);
    preproccess->process(renderData.m_rawState, state.m_nElements, state.m_nElementsPadded,
        renderData.m_pointBuffer, 0);

    DependRes<MapUpdateCompute> mapUpdate =
        glres.get<MapUpdateCompute>("map_compute");
    mapUpdate->update_map(
        renderData.m_pointBuffer,
        renderData.m_numPaths, renderData.m_pathMetadataBuffer,
        renderData.smc_N_VERTS_PER_PATH, renderData.m_vertexBuffer,
        renderData.smc_N_INDICES_PER_PATH, renderData.m_indexBuffer
        );
}
#endif
#if 0
void SysMap::update_map(ActiveScene& rScene)
{
    auto& rUni = rScene.get_application().get_universe();
    auto& reg = rUni.get_reg();
    auto& glres = rScene.get_context_resources();

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
            renderData.m_points.push_back({Vector4{}, Color4{traj.m_color, 1.0}});
        }
        else
        {
            pointIndex = itr->second;
        }
        renderData.m_points[pointIndex].m_pos =
            Vector4{universe_to_render_space(traj.m_position), 1.0};
    }
    renderData.m_pointBuffer.setData(renderData.m_points, BufferUsage::DynamicDraw);
    renderData.m_pointMesh.setCount(renderData.m_points.size());
}
#endif

void MapUpdateCompute::update_map(
    Buffer& pointBuffer,
    size_t numPaths, Buffer& pathMetadata,
    size_t nVertsPerPath, Buffer& pathVertBuffer,
    size_t nIndicesPerPath, Buffer& pathIndexBuffer)
{
    bind_point_locations(pointBuffer);
    bind_path_vert_data(pathVertBuffer);
    bind_path_index_data(pathIndexBuffer);
    bind_path_metadata(pathMetadata);
    set_uniform_counts(nVertsPerPath, nIndicesPerPath);

    using osp::math::num_blocks;

    Vector3ui nGroups{
        num_blocks(static_cast<Magnum::UnsignedInt>(nVertsPerPath), smc_BLOCK_SIZE.x()),
        static_cast<Magnum::UnsignedInt>(numPaths),
        1};
    dispatchCompute(nGroups);

    using Magnum::GL::Renderer;
    Renderer::setMemoryBarrier(Renderer::MemoryBarrier::VertexAttributeArray);
    Renderer::setMemoryBarrier(Renderer::MemoryBarrier::ElementArray);
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

void MapUpdateCompute::set_uniform_counts(size_t nVertsPerPath, size_t nIndicesPerPath)
{
    setUniform(static_cast<Int>(EUniformPos::BlockCounts),
        Vector4ui{
            static_cast<UnsignedInt>(nVertsPerPath),
            static_cast<UnsignedInt>(nIndicesPerPath),
            0, 0});
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

void ProcessMapCoordsCompute::process(
    Buffer& rawInput, size_t inputCount, size_t inputCountPadded,
    Buffer& dest, size_t destOffset)
{
    set_input_counts(inputCount, inputCountPadded, destOffset);
    bind_input_buffer(rawInput);
    bind_output_buffer(dest);

    constexpr size_t blockLength = smc_BLOCK_SIZE.x();
    // Allocate just enough blocks to enclose all data
    size_t numBlocks = (inputCount / blockLength)
        + ((inputCount % blockLength) > 0) ? 1 : 0;
    dispatchCompute(Vector3ui{static_cast<UnsignedInt>(numBlocks), 1, 1});

    using Magnum::GL::Renderer;
    Renderer::setMemoryBarrier(Renderer::MemoryBarrier::ShaderStorage);
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

void ProcessMapCoordsCompute::set_input_counts(size_t nInputPoints, size_t nInputPointsPadded,
    size_t outputOffset)
{
    setUniform(static_cast<Int>(UniformPos::Counts),
        Vector3ui{
            static_cast<UnsignedInt>(nInputPoints),
            static_cast<UnsignedInt>(nInputPointsPadded),
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
