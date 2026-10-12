#include "VXTest.h"

#include <Voxide/core/Version.h>

VXT_TEST("Version string is not empty") {
    const char *v = engine::EngineVersion();
    VXT_REQUIRE(v != nullptr);
    VXT_CHECK(v[0] != '\0');
}
