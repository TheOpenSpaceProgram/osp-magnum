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
#include <list>
#include <string>
#include <iostream>
#include <vector>

namespace osp
{

template<typename FUNC_T>
struct FunctionOrderCall
{
    std::string m_name;
    std::string m_after;
    std::string m_before;
    std::function<FUNC_T> m_function;
};

template<typename FUNC_T>
using FunctionOrderCallList = std::list< FunctionOrderCall<FUNC_T> >;

/**
 * A class that calls certain functions in an order based on before/after rules
 * @tparam Func type of function to be called, ie. void(void)
 */
template<typename FUNC_T>
class FunctionOrder
{
public:
    FunctionOrder() = default;
    FunctionOrder(FunctionOrder &&) = default;
    FunctionOrder(FunctionOrder const&) = delete;
    FunctionOrder& operator=(FunctionOrder &&) = default;
    FunctionOrder& operator=(FunctionOrder const&) = delete;

    using iterator_t = typename FunctionOrderCallList<FUNC_T>::iterator;

    /**
     * Emplaces a new FunctionOrderCall, inserts it in the right place, then
     * binds it to the passes FunctionOrderHandle
     * @param rHandle [out] Handle to bind the newly created call to
     * @param name [in] Name used to identify the new call
     * @param after [in] After rule, the new call must be placed after the call
     *              that has this name
     * @param before [in] Before rule, the new call must be places before the
     *               call that has this name
     * @param args [in] arguments to pass to std::function constructor
     */
    template<class ... ARGS_T>
    iterator_t add(std::string name,
                   std::string after,
                   std::string before,
                   ARGS_T&& ... args);

    /**
     * Iterate through call list and call all of them
     * @tparam ARGS_T arguments to pass to the functions to call
     */
    template<class ... ARGS_T>
    void call(ARGS_T && ... args);

    constexpr FunctionOrderCallList<FUNC_T>& get_call_list() { return m_calls; }
    constexpr FunctionOrderCallList<FUNC_T> const& get_call_list() const
    { return m_calls; }

private:
    FunctionOrderCallList<FUNC_T> m_calls;
};

template<typename FUNC_T>
template<class ... ARGS_T>
void FunctionOrder<FUNC_T>::call(ARGS_T && ... args)
{
    // TODO: Range-for only until end()-2.
    // Then std::forward into the last.
    for (auto &funcToCall : m_calls)
    {
        std::invoke(funcToCall.m_function, args...);
    }
}

template<typename FUNC_T>
template<class ... ARGS_T>
auto FunctionOrder<FUNC_T>::add(
        std::string name,
        std::string after,
        std::string before,
        ARGS_T&& ... args) -> iterator_t
{
    iterator_t minPos = m_calls.begin();
    iterator_t maxPos = m_calls.end();

    // some algorithm for adding calls in order:
    // loop through existing calls:
    // * if iterated's name matches after, then set minPos to iterated
    // * if iterated's name matches before, then set maxPos to iterated, never
    //   touch maxPos ever again
    // * if iterated's before matches name, then set minPos to iterated
    // * if iterated's after matches name, then set maxPos to iterated, never
    //   touch maxPos ever again

    // TODO: not important yet, but eventually deal with preventing things that
    //       look like [{after: "Foo", ...}, {before: "Foo", ...}]
    //       where it will be impossible to insert "Foo"

    // TOOD: hash the strings because that's a lot of string comparisons

    for (auto it = m_calls.begin(); it != m_calls.end(); it ++)
    {
        if (it->m_name == after || it->m_before == name)
        {
            minPos = it;
        }

        // if maxpos not set yet and other stuff
        if (maxPos == m_calls.end()
            && (it->m_name == before || it->m_after == name))
        {
            maxPos = it;
        }
    }

    // new FunctionOrderCall call can be emplaces somewhere between
    // minPos or maxPos

    return m_calls.insert(maxPos,
                          { std::move(name),
                            std::move(after),
                            std::move(before),
                            std::forward<ARGS_T>(args)...});
}

}

