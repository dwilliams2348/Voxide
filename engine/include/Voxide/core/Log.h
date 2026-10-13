#pragma once

#include <Voxide/core/Types.h>

#include <format>
#include <string_view>
#include <utility>
#include <atomic>

namespace engine::log {
    enum class Level : u8 {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
        Off
    };

    using FatalHandler = void(*)();

    // configuration (any thread)

    void SetLevel(Level level); // console threshold
    void SetFileLevel(Level level); // file threshold
    Level GetLevel(); // gets current console level
    void SetColor(bool enabled); // overrides auto detection
    void SetConsoleEnabled(bool enabled); 
    void SetShowSource(bool enabled); // appends file:line to log
    bool SetFile(const char* path, bool append = true);
    void CloseFile();
    void SetThreadName(const char* name); // copied into thread-local storage
    void SetFatalHandler(FatalHandler handler); // nullptr = abort()
    void Flush();

    // backend

    void Write(Level level, const char* file, int line, std::string_view message);

    namespace detail{
        // lower of console/file thresholds, constant initualized in log.cpp
        // so it is safe to read during static initialization
        extern std::atomic<u8> gMinLevel;

        inline bool ShouldLog(Level level){
            return (u8)level >= gMinLevel.load(std::memory_order_relaxed);
        }

        inline constexpr usize kLineBufferSize = 1024;
        inline constexpr usize kThreadNameLen = 32;

        template<class... Args>
        void LogFmt(Level level, const char* file, int line, std::format_string<Args...> fmt, Args&&... args){
            if(!ShouldLog(level))
                return;
            
            char buf[kLineBufferSize];
            auto r = std::format_to_n(buf, kLineBufferSize - 1, fmt, std::forward<Args>(args)...);
            usize len = static_cast<usize>(r.size);
            if(len > kLineBufferSize - 1){
                len = kLineBufferSize - 1;
                buf[len - 3] = buf[len - 2] = buf[len - 1] = '.'; // truncation marker
            }

            Write(level, file, line, std::string_view(buf, len));
        }
    }
}

// Macros
// VX_LOG_MIN_LEVEL, will be a compile time flag based off of the config
// so certain calls vanish

#define VX_LOG_IMPL(lvl, ...) \
    ::engine::log::detail::LogFmt(::engine::log::Level::lvl, __FILE__, __LINE__, __VA_ARGS__)

#if defined(VX_DEBUG)
#define VX_LOG_MIN_LEVEL 0 //(u8)::engine::log::Level::Trace
#elif defined(VX_RELDEB)
#define VX_LOG_MIN_LEVEL 1 //(u8)::engine::log::Level::Debug
#elif defined(VX_RELEASE)
#define VX_LOG_MIN_LEVEL 3 //(u8)::engine::log::Level::Warn
#elif defined(VX_MINSIZE)
#define VX_LOG_MIN_LEVEL 4 //(u8)::engine::log::Level::Error
#endif

#if VX_LOG_MIN_LEVEL <= 0 //(u8)engine::log::Level::Trace
#define VX_LOG_TRACE(...) VX_LOG_IMPL(Trace, __VA_ARGS__)
#else
#define VX_LOG_TRACE(...) ((void)0)
#endif

#if VX_LOG_MIN_LEVEL <= 1 //(u8)engine::log::Level::Debug
#define VX_LOG_DEBUG(...) VX_LOG_IMPL(Debug, __VA_ARGS__)
#else
#define VX_LOG_DEBUG(...) ((void)0)
#endif

#if VX_LOG_MIN_LEVEL <= 3 //(u8)engine::log::Level::Warn
#define VX_LOG_INFO(...) VX_LOG_IMPL(Info, __VA_ARGS__)
#define VX_LOG_WARN(...) VX_LOG_IMPL(Warn, __VA_ARGS__)
#else
#define VX_LOG_INFO(...) ((void)0)
#define VX_LOG_WARN(...) ((void)0)
#endif

#if VX_LOG_MIN_LEVEL == 6 //(u8)engine::log::Level::Off
#define VX_LOG_ERROR(...) ((void)0)
#define VX_LOG_FATAL(...) ((void)0)
#else
#define VX_LOG_ERROR(...) VX_LOG_IMPL(Error, __VA_ARGS__)
#define VX_LOG_FATAL(...) VX_LOG_IMPL(Fatal, __VA_ARGS__)
#endif

/*
    when adding custom types ie Vec3 the std::formatter gets specialized next to the type
    template <> struct std::formatter<fw::IVec3> : std::formatter<std::string_view> {
    auto format(const fw::IVec3& v, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({}, {}, {})", v.x, v.y, v.z);
    }
};
*/