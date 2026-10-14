#include "VXTest.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace test {
    namespace {
        struct TestCase {
                const char *name;
                TestFn fn;
                const char *file;
                int line;
        };

        std::vector<TestCase> &Registry() {
            static std::vector<TestCase> tests;
            return tests;
        }

        int gFailuresThisTest = 0;
    } // namespace

    Registrar::Registrar(const char *name, TestFn fn, const char *file, int line) {
        Registry().push_back({name, fn, file, line});
    }

    void ReportFailure(const char *expr, const char *file, int line) {
        std::printf("\t  FAILED: %s\n\t  at %s:%d\n", expr, file, line);
        ++gFailuresThisTest;
    }

    int RunAll(int argc, char **argv) {
        const char *filter = (argc > 1) ? argv[1] : nullptr;
        int ran = 0, failed = 0;

        for (const TestCase &t : Registry()) {
            if (filter && !std::strstr(t.name, filter))
                continue;

            gFailuresThisTest = 0;
            std::printf("[  RUN  ] %s\n", t.name);
            t.fn();
            ++ran;
            if (gFailuresThisTest) {
                ++failed;
                std::printf("\t[  FAIL  ] %s\n", t.name);
            } else {
                std::printf("\t[   OK   ] %s\n", t.name);
            }
        }

        std::printf("\n%d test(s) ran, %d test(s) failed.\n", ran, failed);
        return failed ? 1 : 0;
    }
} // namespace test
