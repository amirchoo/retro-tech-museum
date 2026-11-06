#pragma once
// Tiny test framework: no dependency. TEST(name) { CHECK(cond); }
#include <cstdio>
#include <functional>
#include <vector>

namespace rtmtest {
struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
struct Register { Register(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };
}  // namespace rtmtest

#define RTM_CAT2(a, b) a##b
#define RTM_CAT(a, b) RTM_CAT2(a, b)
#define TEST(name)                                                                        \
    static void name();                                                                   \
    static ::rtmtest::Register RTM_CAT(reg_, name)(#name, name);                          \
    static void name()
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            ++::rtmtest::failures();                                                      \
        }                                                                                 \
    } while (0)
