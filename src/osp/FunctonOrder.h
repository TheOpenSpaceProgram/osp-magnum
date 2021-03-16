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

template<typename FUNC_T> struct FunctionOrderCall;

template<typename FUNC_T> class FunctionOrderHandle;

template<typename FUNC_T>
using FunctionOrderCallList = std::list<FunctionOrderCall<FUNC_T> >;

template<typename FUNC_T>
struct FunctionOrderCall
{
    //template<class ... ARGS_T>
    FunctionOrderCall(std::string const& name,
                      std::string const& after,
                      std::string const& before,
                      std::function<FUNC_T> function) :
            m_name(name),
            m_after(after),
            m_before(before),
            m_function(function) {}

    //typename FunctionOrderCallList<FUNC_T>::iterator m_after, m_before;

    std::string m_name, m_after, m_before;
    //std::hash<std::string> m_nameHash;
    std::function<FUNC_T> m_function;
    unsigned m_referenceCount;
};



/**
 * A class that calls certain functions in an order based on before/after rules
 * @tparam Func type of function to be called, ie. void(void)
 */
template<typename FUNC_T>
class FunctionOrder
{
public:
    FunctionOrder() = default;

    /**
     * @tparam Args arguments to pass to std::function constructor
     */
    //template<class ... ARGS_T>
    //FunctionOrderHandle<FUNC_T> emplace(
    //        std::string const& name,
    //        std::string const& after,
    //        std::string const& before,
    //        ARGS_T&& ... args);

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
    void add(FunctionOrderHandle<FUNC_T> &rHandle,
             std::string const& name,
             std::string const& after,
             std::string const& before,
             ARGS_T&& ... args);

    /**
     * Iterate through call list and call all of them
     * @tparam ARGS_T arguments to pass to the functions to call
     */
    template<class ... ARGS_T>
    void call(ARGS_T&& ... args);

    constexpr FunctionOrderCallList<FUNC_T>& get_call_list() { return m_calls; }
    constexpr FunctionOrderCallList<FUNC_T> const& get_call_list() const
    { return m_calls; }

private:
    FunctionOrderCallList<FUNC_T> m_calls;
};


template<typename FUNC_T>
class FunctionOrderHandle
{
    friend FunctionOrder<FUNC_T>;
public:
    FunctionOrderHandle(typename FunctionOrderCallList<FUNC_T>::iterator &to);

    template<class ... ARGS_T>
    FunctionOrderHandle(
            FunctionOrder<FUNC_T> &to,
            std::string const& name,
            std::string const& after,
            std::string const& before,
            ARGS_T&& ... args);
    ~FunctionOrderHandle();
private:
    typename FunctionOrderCallList<FUNC_T>::iterator m_to;
};



// TODO: put these somewhere else

//template<typename FUNC_T>
//FunctionOrder<FUNC_T>::FunctionOrder()
//{
//
//}
template<typename FUNC_T>
template<class ... ARGS_T>
void FunctionOrder<FUNC_T>::call(ARGS_T&& ... args)
{
    for (auto &funcToCall : m_calls)
    {
        funcToCall.m_function(args...);
    }
}

template<typename FUNC_T>
FunctionOrderHandle<FUNC_T>::FunctionOrderHandle(
        typename FunctionOrderCallList<FUNC_T>::iterator &to) :
            m_to(to)
{
    to->m_referenceCount ++;
}

template<typename FUNC_T>
template<class ... ARGS_T>
FunctionOrderHandle<FUNC_T>::FunctionOrderHandle(
        FunctionOrder<FUNC_T> &to,
        std::string const& name,
        std::string const& after,
        std::string const& before,
        ARGS_T&& ... args)
{
    to.add(*this, name, after, before, std::forward<ARGS_T>(args)...);
}

template<typename FUNC_T>
FunctionOrderHandle<FUNC_T>::~FunctionOrderHandle()
{
    m_to->m_referenceCount --;
}


template<typename FUNC_T>
template<class ... ARGS_T>
void FunctionOrder<FUNC_T>::add(
        FunctionOrderHandle<FUNC_T> &handle,
        std::string const& name,
        std::string const& after,
        std::string const& before,
        ARGS_T&& ... args)
{
    //std::size_t  ::hash<std::string> m_nameHash;
    typename FunctionOrderCallList<FUNC_T>::iterator minPos = m_calls.begin(),
                                                   maxPos = m_calls.end();

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

    handle.m_to = m_calls.insert(
            maxPos, FunctionOrderCall<FUNC_T>(name, after, before,
                     std::function<FUNC_T>(std::forward<ARGS_T>(args)...) ));

}


}


