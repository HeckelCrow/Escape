#pragma once
#include <utility>

#define ANONYMOUS_VARIABLE_2(a, line_num) a##line_num##_
#define ANONYMOUS_VARIABLE(a, line_num) ANONYMOUS_VARIABLE_2(a, line_num)
#define SCOPE_EXIT(lb)                                                         \
    const auto ANONYMOUS_VARIABLE(_scope_exit_guard_, __LINE__) =              \
        ScopeGuardOnExit() + [&]() lb;

enum class ScopeGuardOnExit
{
};

template<typename Fun>
struct ScopeGuard
{
    ScopeGuard(Fun&& fn) : fun(fn) {}
    ~ScopeGuard()
    {
        fun();
    }
    Fun fun;
};

template<typename Fun>
ScopeGuard<Fun>
operator+(ScopeGuardOnExit, Fun&& fn)
{
    return ScopeGuard<Fun>(std::forward<Fun>(fn));
}