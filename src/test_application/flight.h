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

#include "OSPMagnum.h"
#include "universes/common.h"

#include <thread>

namespace testapp
{

/**
 * @brief Start flight scene
 *
 * This will create an ActiveArea in the universe and start a Magnum application
 * with an ActiveScene setup for in-universe flight.
 *
 * This function blocks execution until the window is closed.
 *
 * @param pMagnumApp [out] Container for Magnum application to create for
 *                         external access
 * @param rOspApp    [ref] OSP Application containing needed resources
 * @param rUni       [ref] Universe the flight scene will be connected to
 * @param rUniUpd    [ref] Universe update function to be called per frame
 * @param args       [in] Arguments to pass to Magnum
 */
void test_flight(std::unique_ptr<OSPMagnum>& pMagnumApp,
                 osp::OSPApplication& rOspApp,
                 osp::universe::Universe& rUni, universe_update_t& rUniUpd,
                 OSPMagnum::Arguments args);

}
