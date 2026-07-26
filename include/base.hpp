/*
    fast-doc-search-poc
    Copyright © 2026 Xabier Fernández

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#pragma once
#include <cstdint>
#include <chrono>
#include <print>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

#define TIME_FUNCTION_SINGLE(fn, out_time, out_val) { \
    const auto start = std::chrono::high_resolution_clock::now(); \
    out_val = fn; \
    const auto end = std::chrono::high_resolution_clock::now(); \
    const std::chrono::duration<double> elapsed = (end - start); \
    out_time = elapsed.count(); \
}

#ifdef BASE_ENABLE_TRY_FAIL_LOG

#define TRY(...) ({ \
    const auto _tmp_expr = (__VA_ARGS__); \
    if(!_tmp_expr) { \
        std::println("'" #__VA_ARGS__ "' failed!"); \
        return false; \
    } \
}

#else

#define TRY(...) { \
    const auto _tmp_expr = (__VA_ARGS__); \
    if(!_tmp_expr) { \
        return false; \
    } \
}

#endif

#define CONCAT_IMPL(x,y) x##y
#define CONCAT(x,y) CONCAT_IMPL(x,y)
#define UNIQUE_VAR_NAME(prefix) CONCAT(prefix ## _, __LINE__)

template<typename F>
class OnScopeExit {
    private:
        F exit_fn;

    public:
        OnScopeExit(F fn) : exit_fn(fn) {}

        ~OnScopeExit() {
            (this->exit_fn)();
        }
};

#define ON_SCOPE_EXIT(...) ::OnScopeExit UNIQUE_VAR_NAME(on_scope_exit) ( __VA_ARGS__ )
