#pragma once

#include <functional>
#include <list>
#include <string>
#include <vector>

namespace osp
{

template<typename Func> class FunctionOrderCall;

template<typename Func>
using FunctionOrderCallList = std::list<FunctionOrderCall<Func> >;

template<typename Func>
struct FunctionOrderCall
{
    FunctionOrderCall(std::string const& name,
                      std::string const& after,
                      std::string const& before,
                      std::function<Func> function) :
            m_name(name),
            m_after(after),
            m_before(before),
            m_function(function) {}

    //typename FunctionOrderCallList<Func>::iterator m_after, m_before;

    std::string m_name, m_after, m_before;
    //std::hash<std::string> m_nameHash;
    std::function<Func> m_function;
    unsigned m_referenceCount;
};




template<typename Func>
class FunctionOrderHandle
{
public:
    FunctionOrderHandle(typename FunctionOrderCallList<Func>::iterator &to);
    ~FunctionOrderHandle();
private:
    typename FunctionOrderCallList<Func>::iterator m_to;
};




/**
 * A class that calls certain functions in an order based on before/after rules
 * @tparam Func type of function to be called, ie. void(void)
 */
template<typename Func>
class FunctionOrder
{
public:
    FunctionOrder() = default;

    /**
     * @tparam Args arguments to pass to std::function constructor
     */
    template<class ... Args>
    FunctionOrderHandle<Func> add(std::string const& name,
                                  std::string const& after,
                                  std::string const& before,
                                  Args&& ... args);

    FunctionOrderCallList<Func>& get_call_list() { return m_calls; }

private:
    FunctionOrderCallList<Func> m_calls;
};




// TODO: put these somewhere else

//template<typename Func>
//FunctionOrder<Func>::FunctionOrder()
//{
//
//}

template<typename Func>
FunctionOrderHandle<Func>::FunctionOrderHandle(
        typename FunctionOrderCallList<Func>::iterator &to) :
            m_to(to)
{
    to->m_referenceCount ++;
}

template<typename Func>
FunctionOrderHandle<Func>::~FunctionOrderHandle()
{
    m_to->m_referenceCount --;
}


template<typename Func>
template<class ... Args>
FunctionOrderHandle<Func> FunctionOrder<Func>::add(std::string const& name,
                                                   std::string const& after,
                                                   std::string const& before,
                                                   Args&& ... args)
{
    //std::size_t  ::hash<std::string> m_nameHash;
    typename FunctionOrderCallList<Func>::iterator minPos = m_calls.begin(),
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

    typename FunctionOrderCallList<Func>::iterator functonCall
            = m_calls.insert(maxPos, {name, after, before, std::function<Func>(args...)});

    return FunctionOrderHandle<Func>(functonCall);
}


}


