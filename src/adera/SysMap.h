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

#include <osp/Active/ActiveScene.h>
#include <osp/types.h>
#include <osp/Universe.h>

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/AbstractShaderProgram.h>

#include <Magnum/Shaders/VertexColor.h>

namespace adera::active
{

/*
Device memory:
- A buffer of vertices to represent all body positions
- An indexed buffer of vertices to represent orbit paths

Every frame, the buffer will be mmapped and new positions written. A compute
shader can increment the index buffer and fade the orbits over time.
*/
class MapRenderData
{
    friend class MapUpdateCompute;
    friend class SysMap;
public:
    // "Primitive restart" index which signifies to break a line strip at that vertex
    static constexpr GLuint PRIMITIVE_RESTART = std::numeric_limits<GLuint>::max();

    MapRenderData(osp::active::ActiveScene& scene, size_t maxPoints, size_t maxPathVertices);
    ~MapRenderData() = default;
    MapRenderData(MapRenderData const& copy) = delete;
    MapRenderData(MapRenderData&& move) noexcept = default;
    MapRenderData& operator=(MapRenderData&& move) = default;

public:
    GLuint m_maxPoints;
    GLuint m_maxPathVerts;

//#pragma pack(push, 1)
    struct ColorVert
    {
        //Magnum::Vector4 m_pos;
        Magnum::Vector3 m_pos;
        Magnum::Color4 m_color;
    };
//#pragma pack(pop)

    // Path data
    Magnum::GL::Buffer m_rawState;
    std::vector<GLuint> m_indexData;
    Magnum::GL::Buffer m_indexBuffer;
    std::vector<ColorVert> m_vertexData;
    Magnum::GL::Buffer m_vertexBuffer;
    Magnum::GL::Mesh m_mesh;

    // Point object data
    std::vector<ColorVert> m_points;
    Magnum::GL::Buffer m_pointBuffer;
    Magnum::GL::Mesh m_pointMesh;

    struct PathMetadata
    {
        GLuint m_pointIndex;
        GLuint m_startIdx;
        GLuint m_endIdx;
        GLuint m_nextIdx;
    };

    // Mapping from satellites to path metadata index
    std::map<osp::universe::Satellite, size_t> m_pathMapping;
    // Path metadata
    std::vector<PathMetadata> m_pathMetadata;
    Magnum::GL::Buffer m_pathMetadataBuffer;

    // Mapping from satellites to point sprites
    std::map<osp::universe::Satellite, GLuint> m_pointMapping;
};

class ProcessMapCoordsCompute : public Magnum::GL::AbstractShaderProgram
{
public:
    ProcessMapCoordsCompute() { init(); }
    ~ProcessMapCoordsCompute() = default;
    ProcessMapCoordsCompute(ProcessMapCoordsCompute const& copy) = delete;
    ProcessMapCoordsCompute(ProcessMapCoordsCompute&& move) = default;

    void process(
        Magnum::GL::Buffer& rawInput, size_t inputCount, size_t inputCountPadded,
        Magnum::GL::Buffer& dest, size_t destOffset);
private:
    void init();

    enum class UniformPos : Magnum::Int
    {
        Counts = 0
    };

    enum class BufferBinding : Magnum::Int
    {
        RawInput = 0,
        Output = 1
    };

    void set_input_counts(size_t nInputPoints, size_t nInputPointsPadded, size_t outputOffset);
    void bind_input_buffer(Magnum::GL::Buffer& input);
    void bind_output_buffer(Magnum::GL::Buffer& output);
};

class MapUpdateCompute : public Magnum::GL::AbstractShaderProgram
{
public:
    MapUpdateCompute() { init(); }
    ~MapUpdateCompute() = default;
    MapUpdateCompute(MapUpdateCompute const& copy) = delete;
    MapUpdateCompute(MapUpdateCompute&& move) = default;

    void update_map(
        size_t numPoints, Magnum::GL::Buffer& pointBuffer,
        size_t numPaths, Magnum::GL::Buffer& pathMetadata,
        size_t numPathVerts, Magnum::GL::Buffer& pathVertBuffer,
        size_t numPathIndices, Magnum::GL::Buffer& pathIndexBuffer);
private:
    void init();

    enum class EUniformPos : Magnum::Int
    {
        BlockCounts = 0,
    };

    enum class EBufferBinding : Magnum::Int
    {
        RawInput = 0,
        PointVerts = 1,
        PathData = 2,
        PathIndices = 3,
        PathsInfo = 4
    };

    using Magnum::GL::AbstractShaderProgram::draw;
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    void set_uniform_counts(size_t numPoints, size_t numPaths,
        size_t numPathVerts, size_t numPathIndices);
    void bind_raw_position_data(Magnum::GL::Buffer& data);
    void bind_point_locations(Magnum::GL::Buffer& points);
    void bind_path_vert_data(Magnum::GL::Buffer& pathVerts);
    void bind_path_index_data(Magnum::GL::Buffer& pathIndices);
    void bind_path_metadata(Magnum::GL::Buffer& data);
};

struct ACompMapVisible
{
    bool m_visible{true};
};

class SysMap
{
public:
    static void add_functions(osp::active::ActiveScene& rScene);
    static void update_map(osp::active::ActiveScene& rScene);

    static Magnum::Vector3 universe_to_render_space(osp::Vector3s v3s);

private:
    static constexpr size_t m_orbitSamples = 512;
    static void configure_render_passes(osp::active::ActiveScene& rScene);
};

} // namespace adera::active
