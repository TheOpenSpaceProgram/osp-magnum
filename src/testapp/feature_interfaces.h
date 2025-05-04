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
#pragma once

#include <adera_app/feature_interfaces.h>

namespace ftr_inter
{

struct FICinREPL {
    struct DataIds {
        DataId cinLines;
    };

    struct Pipelines {
        PipelineDef<EStgIntr> cinLines          {"cinLines"};
    };
};

struct FIMagnum {
    struct DataIds {
        DataId magnumApp;
        DataId renderGl;
    };

    struct Pipelines {
        PipelineDef<EStgCont> meshGL            {"meshGL"};
        PipelineDef<EStgCont> textureGL         {"textureGL"};

        PipelineDef<EStgCont> entMeshGL         {"entMeshGL"};
        PipelineDef<EStgCont> entTextureGL      {"entTextureGL"};
    };
};


struct FIMagnumScene {
    struct DataIds {
        DataId scnRenderGl;
        DataId groupFwd;
        DataId camera;
    };

    struct Pipelines {
        PipelineDef<EStgFBO>  fbo               {"fboRender"};
        PipelineDef<EStgCont> camera            {"camera"};
    };
};


struct FIShaderVisualizer {
    struct DataIds {
        DataId shader;
    };

    struct Pipelines { };
};

struct FIShaderFlat {
    struct DataIds {
        DataId shader;
    };

    struct Pipelines { };
};

struct FIShaderPhong {
    struct DataIds {
        DataId shader;
    };

    struct Pipelines { };
};


struct FITerrainDrawMagnum {
    struct DataIds {
        DataId drawTerrainGL;
    };

    struct Pipelines { };
};


} // namespace ftr_inter
