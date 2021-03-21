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
#include "SysGUI.h"
#include "ActiveScene.h"
#include <Magnum/GL/Renderer.h>
using namespace osp::active;

SysGUI::SysGUI(ActiveScene& rScene)
    : m_updateGUI(rScene.get_update_order(), "gui", "physics", "",
        [](ActiveScene& rScene) { update_GUI(rScene); })
    , m_drawGUI(rScene.get_render_order(), "gui", "debug", "",
        [&rScene](ACompCamera const& camera) { render_GUI(rScene, camera); })
{}

void SysGUI::update_GUI(ActiveScene& rScene)
{
    // Fetch ImGui context from scene root
    ActiveEnt sceneRoot = rScene.hier_get_root();
    auto* imgui = rScene.reg_try_get<ACompImGuiContext>(sceneRoot);
    if (imgui == nullptr) { return; }
    ImGui::SetCurrentContext(imgui->m_imgui.context());

    // Fetch ImPlot context from scene root, if it exists
    auto* implot = rScene.reg_try_get<ACompImPlotContext>(sceneRoot);
    if (implot != nullptr)
    {
        ImPlot::SetCurrentContext(implot->m_implot.get());
    }

    // Initialize new GUI frame
    imgui->m_imgui.newFrame();

    // Process GUI elements
    for (auto [ent, describeElement]
        : rScene.get_registry().view<ACompGUIWindow>().each())
    {
        describeElement.m_function(rScene, describeElement.m_visible);
    }
}

void SysGUI::render_GUI(ActiveScene& rScene, ACompCamera const& camera)
{
    ActiveEnt sceneRoot = rScene.hier_get_root();
    auto* imgui = rScene.reg_try_get<ACompImGuiContext>(sceneRoot);
    if (imgui == nullptr) { return; }
    ImGui::SetCurrentContext(imgui->m_imgui.context());

    using namespace Magnum::GL;

    Renderer::enable(Renderer::Feature::Blending);
    Renderer::enable(Renderer::Feature::ScissorTest);
    Renderer::disable(Renderer::Feature::FaceCulling);
    Renderer::disable(Renderer::Feature::DepthTest);
    imgui->m_imgui.drawFrame();
    Renderer::enable(Renderer::Feature::DepthTest);
    Renderer::enable(Renderer::Feature::FaceCulling);
    Renderer::disable(Renderer::Feature::ScissorTest);
    Renderer::disable(Renderer::Feature::Blending);
}


