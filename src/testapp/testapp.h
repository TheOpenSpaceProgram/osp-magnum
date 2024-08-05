/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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

#include <osp/core/keyed_vector.h>
#include <osp/core/resourcetypes.h>

#include <osp/util/logging.h>

#include <osp/framework/framework.h>
#include <osp/framework/builder.h>


#include <entt/core/any.hpp>

#include <mutex>

namespace testapp
{

class NonBlockingStdInReader
{
public:
    void start_thread()
    {
        thread = std::thread{[this]
        {
            while(true)
            {
                std::string strIn;
                std::cin >> strIn;

                std::lock_guard<std::mutex> guard(mutex);
                messages.emplace_back(std::move(strIn));
            }
        }};
    }

    //void stop(); // TODO

    [[nodiscard]] std::vector<std::string> read()
    {
        std::lock_guard<std::mutex> guard(mutex);
        return std::exchange(messages, {});
    }

    static NonBlockingStdInReader& instance()
    {
        static NonBlockingStdInReader thing;
        return thing;
    }

private:
    std::thread                 thread;
    std::mutex                  mutex;
    std::vector<std::string>    messages;
};




using MainLoopFunc_t = bool(*)();

inline std::vector<MainLoopFunc_t>& main_loop_stack()
{
    static std::vector<MainLoopFunc_t> instance;
    return instance;
}

struct TestApp;

using RendererSetupFunc_t   = void(*)(TestApp&);
using SceneSetupFunc_t      = RendererSetupFunc_t(*)(TestApp&);

struct MainLoopControl
{
    bool doUpdate;
};


extern osp::fw::FeatureDef const ftrMain;


struct TestApp
{

//    void close_sessions(osp::ArrayView<osp::Session> sessions);

//    void close_session(osp::Session &rSession);

    /**
     * @brief Deal with resource reference counts for a clean termination
     */
    void clear_resource_owners();

    void drive_main_loop();

    void init();

    osp::fw::Framework m_framework;

    osp::fw::ContextId m_mainContext;


//    osp::SessionGroup               m_applicationGroup;
//    osp::Session                    m_application;

//    osp::SessionGroup               m_scene;

//    osp::Session                    m_windowApp;
//    osp::Session                    m_magnum;
//    osp::SessionGroup               m_renderer;

//    RendererSetupFunc_t             m_rendererSetup { nullptr };

    osp::fw::IExecutor              *m_pExecutor { nullptr };

    osp::PkgId                      m_defaultPkg    { lgrn::id_null<osp::PkgId>() };
};


//-----------------------------------------------------------------------------



} // namespace testapp
