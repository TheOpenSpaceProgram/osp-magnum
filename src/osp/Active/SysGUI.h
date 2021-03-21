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

#include <functional>

#include "osp/Active/activetypes.h"
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <implot.h>

namespace osp::active
{

// TMP
struct ACompImGuiContext
{
    Magnum::ImGuiIntegration::Context m_imgui;
};

using ImPlotContext_t = std::unique_ptr<ImPlotContext, std::function<void(ImPlotContext*)>>;

struct ACompImPlotContext
{
    ImPlotContext_t m_implot;

    static void free_ctx(ImPlotContext* ctx)
    {
        ImPlot::DestroyContext(ctx);
    }
};

struct ACompGUIWindow
{
    std::function<void(ActiveScene&, bool&)> m_function;
    bool m_visible;
};

class SysGUI : public IDynamicSystem
{
public:
    static inline std::string smc_name = "GUI";

    SysGUI(ActiveScene& rScene);
    ~SysGUI() = default;

    static void draw_GUI(ActiveScene& rScene, ACompCamera const&);

private:
    RenderOrderHandle_t m_drawGUI;
};

} // osp::active
