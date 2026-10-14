#include "VXTest.h"
#include <Voxide/core/Assert.h>

#include <cstring>

namespace {
    int gCalls = 0;
    const char *gLastExpr = nullptr;

    bool TestHandler(const engine::AssertInfo &info) {
        ++gCalls;
        gLastExpr = info.expr;
        return false;
    }
} // namespace

VXT_TEST("assert.calls_handler_on_fail") {
    auto old = engine::SetAssertHandler(TestHandler);
    gCalls = 0;

    VX_ASSERT(1 == 2);

    engine::SetAssertHandler(old);
    VXT_CHECK(gCalls == 1);
    VXT_REQUIRE(gLastExpr != nullptr);
    VXT_CHECK(std::strcmp(gLastExpr, "1 == 2") == 0);
}

VXT_TEST("assert.does_nothing_when_condition_holds") {
    auto old = engine::SetAssertHandler(TestHandler);
    gCalls = 0;

    VX_ASSERT(1 == 1);
    engine::SetAssertHandler(old);
    VXT_CHECK(gCalls == 0);
}
