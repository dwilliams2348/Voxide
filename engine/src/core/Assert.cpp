#include "Voxide/core/Assert.h"

#include <cstdio>
#include <cstdlib>

namespace engine {
    namespace {
        bool DefaultHandler(const AssertInfo &info) {
            std::fprintf(stderr, "\n*** ASSERTION FAILED ***\n");
            std::fprintf(stderr, "  expr: %s\n", info.expr ? info.expr : "(none)");
            if (info.msg)
                std::fprintf(stderr, "  msg: %s\n", info.msg);

            std::fprintf(stderr, "  file: %s:%d\n", info.file, info.line);
            std::fprintf(stderr, "  func: %s\n", info.func);
            std::fflush(stderr);
            return true;
        }

        AssertHandler gHandler = DefaultHandler;

        thread_local bool t_inHandler = false;
    } // namespace

    AssertHandler SetAssertHandler(AssertHandler h) {
        AssertHandler old = gHandler;
        gHandler = h ? h : DefaultHandler;
        return old;
    }

    bool AssertFail(const AssertInfo &info) {
        if (t_inHandler) {
            std::fputs("*** Recursive assertion failure, aborting ***\n", stderr);
            std::abort();
        }

        t_inHandler = true;
        const bool shouldBreak = gHandler(info);
        t_inHandler = false;

        return shouldBreak;
    }

    [[noreturn]] void CheckFail(const AssertInfo &info) {
        if (t_inHandler) {
            std::fputs("*** Recursive assertion failure, aborting ***\n", stderr);
            std::abort();
        }

        t_inHandler = true;
        gHandler(info);

        std::fflush(stderr);
        std::abort();
    }
} // namespace engine
