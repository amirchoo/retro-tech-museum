#include "test.h"

int main() {
    int failedCases = 0;
    for (const auto& c : rtmtest::registry()) {
        const int before = rtmtest::failures();
        c.fn();
        const bool ok = rtmtest::failures() == before;
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", c.name);
        failedCases += ok ? 0 : 1;
    }
    std::printf("%zu tests, %d failed\n", rtmtest::registry().size(), failedCases);
    return failedCases == 0 ? 0 : 1;
}
