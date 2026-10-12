#pragma once

namespace test {
    using TestFn = void (*)();

    struct Registrar {
            Registrar(const char *name, TestFn fn, const char *file, int line);
    };

    void ReportFailure(const char *expr, const char *file, int line);
    int RunAll(int argc, char **argv);
} // namespace test

#define VXTEST_CAT2(a, b) a##b
#define VXTEST_CAT(a, b) VXTEST_CAT2(a, b)

#define VXT_TEST(name)                                                                                                 \
    static void VXTEST_CAT(vxtest_fn_, __LINE__)();                                                                    \
    static ::test::Registrar VXTEST_CAT(vxtest_reg_, __LINE__)(name, VXTEST_CAT(vxtest_fn_, __LINE__), __FILE__,       \
                                                               __LINE__);                                              \
    static void VXTEST_CAT(vxtest_fn_, __LINE__)()

#define VXT_CHECK(expr)                                                                                                \
    do {                                                                                                               \
        if (!(expr))                                                                                                   \
            ::test::ReportFailure(#expr, __FILE__, __LINE__);                                                          \
    } while (0)

#define VXT_REQUIRE(expr)                                                                                              \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            ::test::ReportFailure(#expr, __FILE__, __LINE__);                                                          \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)
