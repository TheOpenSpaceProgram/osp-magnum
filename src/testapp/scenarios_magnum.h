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

#include <adera_app/application.h>

#include <osp/framework/framework.h>
#include <osp/core/resourcetypes.h>

namespace testapp
{

class MagnumMainLoop : public adera::IMainLoopFunc
{
public:
    MagnumMainLoop(osp::PkgId defaultPkg, osp::fw::ContextId mainCtx)
     : m_defaultPkg{defaultPkg}, m_mainCtx{mainCtx} {};
    IMainLoopFunc::Status run(osp::fw::Framework &rFW, osp::fw::IExecutor &rExecutor) override;

private:
    osp::PkgId          m_defaultPkg;
    osp::fw::ContextId  m_mainCtx;
};



class FWMCStartMagnumRenderer : public adera::IFrameworkModifyCommand
{
public:
    FWMCStartMagnumRenderer(int argc, char** argv, osp::PkgId defaultPkg, osp::fw::ContextId mainCtx)
     : m_argc{argc}, m_argv{argv}, m_defaultPkg{defaultPkg}, m_mainCtx{mainCtx} { }

    ~FWMCStartMagnumRenderer() {};

    void run(osp::fw::Framework &rFW) override;

    std::unique_ptr<adera::IMainLoopFunc> main_loop() override
    {
        return std::make_unique<MagnumMainLoop>(m_defaultPkg, m_mainCtx);
    };

private:
    int                 m_argc;
    char**              m_argv;
    osp::PkgId          m_defaultPkg;
    osp::fw::ContextId  m_mainCtx;
};


} // namespace testapp
