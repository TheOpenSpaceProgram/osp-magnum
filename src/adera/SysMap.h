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
#include <osp/Trajectories/NBody.h>
#include <osp/Active/SysRender.h>

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/AbstractShaderProgram.h>

#include <Magnum/Shaders/VertexColor.h>

namespace adera::active
{

class ProcessMapCoordsCompute : public Magnum::GL::AbstractShaderProgram
{
public:
    ProcessMapCoordsCompute() { init(); }
    ~ProcessMapCoordsCompute() = default;
    ProcessMapCoordsCompute(ProcessMapCoordsCompute const& copy) = delete;
    ProcessMapCoordsCompute(ProcessMapCoordsCompute&& move) = default;

    void process(
        Magnum::GL::Buffer& rawInput, Magnum::GL::Buffer& dest,
        size_t sigCount, size_t sigCountPadded,
        size_t insigCount, size_t insigCountPadded);
private:
    static constexpr Magnum::Vector3ui smc_BLOCK_SIZE{64, 1, 1};

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

    void set_input_counts(size_t nSigPoints, size_t nSigPointsPadded,
        size_t nInsigPoints, size_t nInsigPointsPadded);
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
        Magnum::GL::Buffer& pointBuffer,
        size_t numPaths,
        Magnum::GL::Buffer& pathMetadata,
        Magnum::GL::Buffer& pathOperationBuffer,
        size_t numBlocks,
        Magnum::GL::Buffer& groupBoundaries,
        Magnum::GL::Buffer& pathVertBuffer,
        Magnum::GL::Buffer& pathIndexBuffer);

    enum class EPathOperation : GLuint
    {
        Skip = 1,
        PushVertFromPointSource = 1 << 1,
        FadeVertices = 1 << 2
        // other operations? use enum as flags?
    };
private:
    static constexpr Magnum::Vector3ui smc_BLOCK_SIZE{64, 1, 1};

    void init();

    enum class EBufferBinding : Magnum::Int
    {
        PointVerts = 0,
        PathData = 1,
        PathIndices = 2,
        PathsInfo = 3,
        PathGroupBoundaries = 4,
        PathOperation = 5
    };

    using Magnum::GL::AbstractShaderProgram::draw;
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    void bind_point_locations(Magnum::GL::Buffer& points);
    void bind_path_vert_data(Magnum::GL::Buffer& pathVerts);
    void bind_path_index_data(Magnum::GL::Buffer& pathIndices);
    void bind_path_metadata(Magnum::GL::Buffer& data);
    void bind_path_group_boundaries(Magnum::GL::Buffer& boundaries);
    void bind_path_update_op_buffer(Magnum::GL::Buffer& operations);
};

/*
Device memory:
- A buffer of vertices to represent all body positions
- An indexed buffer of vertices to represent orbit paths

Every frame, a buffer of object positions will be written into device memory.
A compute shader uses those points to push incremental updates to paths on
the map.
*/
class MapRenderData
{
    friend class MapUpdateCompute;
    friend class SysMap;
public:
    // "Primitive restart" index which signifies to break a line strip at that vertex
    static constexpr GLuint smc_PRIMITIVE_RESTART = std::numeric_limits<GLuint>::max();

    MapRenderData() {}
    ~MapRenderData() = default;
    MapRenderData(MapRenderData const& copy) = delete;
    MapRenderData(MapRenderData&& move) noexcept = default;
    MapRenderData& operator=(MapRenderData&& move) = default;

public:
    // Temporary flag to check initialization state
    bool m_isInitialized{false};

//#pragma pack(push, 1)
    struct ColorVert
    {
        //Magnum::Vector4 m_pos;
        Magnum::Vector4 m_pos;
        Magnum::Color4 m_color;

        ColorVert(Magnum::Vector4 v, Magnum::Color4 c) : m_pos(v), m_color(c) {}
        ColorVert() : ColorVert(Magnum::Vector4{}, Magnum::Color4{1.0}) {}
    };
//#pragma pack(pop)

    /**
     * Path Data
     * 
     * Paths are stored as a single indexed mesh which draws using GL_LINE_STRIP
     * and uses primitive restart to create breaks in the paths. Vertices and
     * indices for each path are contiguous in the buffer. The number of
     * vertices may vary from path to path, and the number of indices in a path
     * is (num. vertices + 1) to account for the primitive restart index that
     * terminates each sequence of indices. However, the vertices are padded out
     * to the same length as the indices, so that the indices line up, which
     * keeps things eaiser. It wastes one vertex per path, but saves the need
     * to store extra metadata.
     */

    /**
     * STREAM_DRAW buffer used to transmit raw position data from the universe
     * to the GPU for compute/rendering
     */
    Magnum::GL::Buffer m_rawState;

    /**
     * STATIC_DRAW buffer used to delineate the boundaries between paths. This
     * is necessary for dividing the work up into work groups for compute
     * shaders in a predictable way. The elements of this buffer are uint
     * indices which represent the index of the first work group ID per path.
     * 
     * example:
     * Groups: | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
     * Paths:  |       0       |   1   |         2          |
     * Boundaries: [0, 4, 6]
     */
    Magnum::GL::Buffer m_pathBoundaries;

    /**
     * Number of blocks to dispatch in update
     * 
     * For convenient block-wise distribution of workload, the number of blocks
     * needed is stored on initialization. This number is larger than expected
     * because paths are allocated on block boundaries, requiring extra padding.
     */
    size_t m_numComputeBlocks{0};

    /* ### Path mesh data ### */

    size_t m_numPaths{0};
    std::vector<GLuint> m_indexData;
    Magnum::GL::Buffer m_indexBuffer;
    std::vector<ColorVert> m_vertexData;
    Magnum::GL::Buffer m_vertexBuffer;
    Magnum::GL::Mesh m_mesh;

    struct PathMetadata
    {
        /**
         * Stores the index of a point source in the point buffer
         * 
         * If the path is a trail, the point source is the current position of 
         * the marking object, which will be pushed to the trail each update. 
         * If the path is a prediction, the point source is a slot that stores 
         * arbitrary points that may be pushed to the front of the path on each 
         * update.
         */
        GLuint m_pointIndex;

        // The index of the first vertex/index element of this path
        GLuint m_startIdx;
        // The index of the last vertex/index this path (inclusive)
        GLuint m_endIdx;
        // The index of next vertex/index to be updated
        GLuint m_nextIdx;
    };
    // Path metadata
    std::vector<PathMetadata> m_pathMetadata;
    Magnum::GL::Buffer m_pathMetadataBuffer;

    // TODO documentation; friendly interface?
    std::vector<GLuint> m_pathUpdateCommands;
    Magnum::GL::Buffer m_pathUpdateCommandBuffer;

    /* ### Point object data ### */

    // Number of point markers on the map (representing planets, ships, etc)
    size_t m_numDrawablePoints{0};
    // Number of points in the point buffer, including non-drawing data, e.g.
    // points for feeding data to predicted trajectories
    size_t m_numAllPoints{0};

    std::vector<ColorVert> m_points;
    Magnum::GL::Buffer m_pointBuffer;
    Magnum::GL::Mesh m_pointMesh;

    // Mapping from satellites to path metadata index
    //std::map<osp::universe::Satellite, size_t> m_trailMapping;
    size_t m_predictionPathIndex{0};

    // Mapping from satellites to point sprites
    std::map<osp::universe::Satellite, GLuint> m_pointMapping;
    size_t m_predictionPointIndex{0};
};

struct ACompMapVisible {};

struct ACompMapLeavesTrail
{
    uint32_t m_numSamples{0};
};

struct ACompMapFocus
{
    osp::universe::Satellite m_sat{entt::null};
    bool m_dirty{false};
};

struct ACompPredictTraj
{
    GLuint m_pointIndex;
    GLuint m_trailIndex;
};

//struct ACompMapPointSprite
//{
//    osp::universe::Satellite m_sat;
//    GLuint m_index;
//};

//struct ACompMapTrail
//{
//    osp::universe::Satellite m_sat;
//    GLuint m_index;
//};

class SysMap
{
public:
    static void add_functions(osp::active::ActiveScene& rScene);
    static void setup(osp::active::ActiveScene& rScene);

    static void update_map(osp::active::ActiveScene& rScene);

    static Magnum::Vector3 universe_to_render_space(osp::Vector3s v3s);


private:
    static osp::active::RenderPipeline create_map_renderer();
    static void register_system(osp::active::ActiveScene& rScene);
    static void process_raw_state(osp::active::ActiveScene& rScene,
        MapRenderData& rMapData, osp::universe::TrajNBody* traj);

    static void select_prediction_target(
        osp::active::ActiveScene& rScene,
        osp::universe::Satellite sat);
    static void read_table_prediction_data(
        osp::active::ActiveScene& rScene, osp::universe::Satellite sat);
    static void generate_prediction_data(
        osp::active::ActiveScene& rScene, osp::universe::Satellite sat);

    static void update_prediction(osp::active::ActiveScene& rScene);
};

} // namespace adera::active
