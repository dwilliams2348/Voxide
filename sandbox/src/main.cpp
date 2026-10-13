#include <cstdio>

#include <Voxide/core/Version.h>
#include <Voxide/core/Log.h>

void Handle() {
    return;
}

int main() {
    std::printf("Engine online (v%s)\n", engine::EngineVersion());

    engine::log::SetFatalHandler(Handle);
    engine::log::SetLevel(engine::log::Level::Trace);
    engine::log::SetThreadName("main");
    engine::log::SetShowSource(true);
    //engine::log::SetColor(false);

    VX_LOG_TRACE("This is a formatted string with numbers '{0}' '{2}' strings '{1}' chars '{3}'", 69, "hello", 3.1459, 'T');
    VX_LOG_INFO("This is a formatted string with numbers '{0}' '{2}' strings '{1}' chars '{3}'", 69, "hello", 3.1459, 'T');
    VX_LOG_DEBUG("This is a formatted string with numbers '{0}' '{2}' strings '{1}' chars '{3}'", 69, "hello", 3.1459, 'T');
    VX_LOG_WARN("This is a formatted string with numbers '{0}' '{2}' strings '{1}' chars '{3}'", 69, "hello", 3.1459, 'T');
    VX_LOG_ERROR("This is a formatted string with numbers '{0}' '{2}' strings '{1}' chars '{3}'", 69, "hello", 3.1459, 'T');
    VX_LOG_FATAL("This is a formatted string with numbers '{0}' '{2}' strings '{1}' chars '{3}'", 69, "hello", 3.1459, 'T');

    return 0;
}
