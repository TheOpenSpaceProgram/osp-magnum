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
#include "console.h"
#include "../feature_interfaces.h"

#include <adera_app/application.h>

#include <osp/core/Resources.h>

#include <iostream>
#include <mutex>
#include <thread>

using namespace adera;
using namespace osp::fw;
using namespace osp;

using namespace ftr_inter;
using namespace ftr_inter::stages;


namespace testapp
{

class NonBlockingCinReader
{
public:
    void start_thread()
    {
        thread = std::thread{[this]
        {
            while(true)
            {
                std::string strIn;
                std::getline(std::cin, strIn);

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

    static NonBlockingCinReader& instance()
    {
        static NonBlockingCinReader thing;
        return thing;
    }

private:
    std::thread                 thread;
    std::mutex                  mutex;
    std::vector<std::string>    messages;
};


FeatureDef const ftrREPL = feature_def("REPL", [] (FeatureBuilder &rFB, Implement<FICinREPL> cinREPL, DependOn<FIMainApp> mainApp)
{
    rFB.data_emplace< std::vector<std::string> >(cinREPL.di.cinLines);
    rFB.pipeline(cinREPL.pl.cinLines).parent(mainApp.pl.mainLoop);

    rFB.task()
        .name       ("Read stdin buffer")
        .run_on     ({mainApp.pl.mainLoop(Run)})
        .sync_with  ({cinREPL.pl.cinLines(Modify_)})
        .args       ({            cinREPL.di.cinLines})
        .func([] (std::vector<std::string> &rCinLines) noexcept
    {
        rCinLines = NonBlockingCinReader::instance().read();
    });

    NonBlockingCinReader::instance().start_thread();
});




} // namespace testapp
