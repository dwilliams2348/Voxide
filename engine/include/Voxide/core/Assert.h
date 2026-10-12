#include "Voxide/core/Defines.h"

#include <signal.h>

namespace engine {
    struct AssertInfo {
            const char *expr;
            const char *msg;
            const char *file;
            int line;
            const char *func;
    };

    using AssertHandler = bool (*)(const AssertInfo &);
    AssertHandler SetAssertHandler(AssertHandler h);

    bool AssertFail(const AssertInfo &info);
    [[noreturn]] void CheckFail(const AssertInfo &info);
} // namespace engine

#if defined(_MSC_VER)
#define VX_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#define VX_DEBUG_BREAK() raise(SIGTRAP)
#endif

#define VX_ASSERT_IMPL(expr, msg)                                                                                      \
    do {                                                                                                               \
        if (VX_UNLIKELY(!(expr))) {                                                                                    \
            if (::engine::AssertFail({#expr, msg, __FILE__, __LINE__, __func__}))                                      \
                VX_DEBUG_BREAK();                                                                                      \
        }                                                                                                              \
    } while (0)

#if defined(VX_DEBUG) || defined(VX_RELDEB)
#define VX_ASSERT(expr) VX_ASSERT_IMPL(expr, nullptr)
#define VX_ASSERT_MSG(expr, msg) VX_ASSERT_IMPL(expr, msg)
#define VX_VERIFY(expr) VX_ASSERT_IMPL(expr, nullptr)
#else
#define VX_ASSERT(expr) ((void)sizeof(!(expr)))
#define VX_ASSERT_MSG(expr, msg) ((void)sizeof(!(expr)))
#define VX_VERIFY(expr) ((void)(expr))
#endif

#define VX_CHECK(expr)                                                                                                 \
    do {                                                                                                               \
        if (VX_UNLIKELY(!(expr))) {                                                                                    \
            ::engine::CheckFail({#expr, nullptr, __FILE__, __LINE__, __func__});                                       \
        }                                                                                                              \
    } while (0)
