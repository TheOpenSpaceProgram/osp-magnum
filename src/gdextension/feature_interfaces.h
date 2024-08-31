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

struct FIGodot {
    struct DataIds {
        DataId app;
        DataId render;
    };

    struct Pipelines {
        PipelineDef<EStgCont> mesh            {"mesh"};
        PipelineDef<EStgCont> texture         {"texture"};

        PipelineDef<EStgCont> entMesh         {"entMesh"};
        PipelineDef<EStgCont> entTexture      {"entTexture"};
    };
};


struct FIGodotScene {
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


struct FIShaderVisualizerGD {
    struct DataIds {
        DataId shader;
    };

    struct Pipelines { };
};

struct FIShaderFlatGD {
    struct DataIds {
        DataId shader;
    };

    struct Pipelines { };
};

struct FIShaderPhongGD {
    struct DataIds {
        DataId shader;
    };

    struct Pipelines { };
};


} // namespace ftr_inter
