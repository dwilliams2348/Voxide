#include <Voxide/core/Log.h>

#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <mutex>

#if defined(VX_PLATFORM_WIN32)
#include <io.h>
#include <windows.h>
#undef min

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#else
#include <unistd.h>
#include <time.h>
#endif

namespace engine::log {
    namespace detail {
        std::atomic<u8> gMinLevel{(u8)Level::Info};
    }

    // private state
    namespace {
        // keep free of members with nontrivial destructors(no std::string/vector)

        std::mutex gWriteMutex;
        std::atomic<u8> gConsoleLevel{(u8)Level::Info};
        std::atomic<u8> gFileLevel{(u8)Level::Trace};
        std::atomic<bool> gConsoleEnabled{true};
        std::atomic<bool> gShowSource{false};
        std::atomic<i8> gColorMode{-1}; // -1 = auto-detect, 0 = off, 1 = on
        FILE* gFile = nullptr; // guarded by gWriteMutex
        FatalHandler gFatalHandler = nullptr;

        thread_local char tName[detail::kThreadNameLen] = {};
        thread_local bool tInFatal = false; //recursion guard

        struct ColorCode{
            const char* start;
        };

        constexpr ColorCode kColors[] = {
            {"\x1b[90m"},        // Trace  (dim)
            {"\x1b[36m"},        // Debug  (cyan)
            {"\x1b[32m"},        // Info   (green)
            {"\x1b[33m"},        // Warn   (yellow)
            {"\x1b[31m"},        // Error  (red)
            {"\x1b[97;41m"},     // Fatal  (bold white on red)
        };
        
        constexpr const char* kReset = "\x1b[0m";
        constexpr const char* kLevelNames[] = {"TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"};

        void RecomputeMinLevel(){
            detail::gMinLevel.store(std::min(gConsoleLevel.load(), gFileLevel.load()));
        }

        bool StderrIsTerminal() {
        #if defined(VX_PLATFORM_WIN32)
            i32 fd = _fileno(stderr);
            if(fd < 0 || !_isatty(fd)){
                return false;
            }

            intptr_t rawHandle = _get_osfhandle(fd);
            if(rawHandle == (intptr_t)-1) {
                return false;
            }

            HANDLE handle = (HANDLE)rawHandle;
            DWORD mode;

            if(!GetConsoleMode(handle, &mode)) {
                return false;
            }

            DWORD wanted = mode
                | ENABLE_PROCESSED_OUTPUT
                | ENABLE_VIRTUAL_TERMINAL_PROCESSING;

            return wanted == mode || SetConsoleMode(handle, wanted) != 0;
        #else
            i32 fd = fileno(stderr);
            return fd >= 0 && isatty(fd) != 0;
        #endif
        }

        bool ColorEnabled() {
            // resolves gColorMode lazily
            
            return (gColorMode == -1) ? StderrIsTerminal() : (bool)gColorMode;
        }

        const char* BasenameOf(const char* path){
            if(!path){
                return nullptr;
            }

            // look for last forward or backward slash
            const char* base = strrchr(path, '/');

        #ifdef VX_PLATFORM_WIN32
            const char* win_base = strrchr(path, '\\');
            if(win_base > base){
                base = win_base;
            }
        #endif

            return base ? base + 1 : path;
        }

        // Writes "YYYY-MM-DD HH:MM:SS.mmm" into out, returns length.
        // Per-thread cache of the seconds part; format only the ms each call.
        usize FormatTimestamp(char* out, usize cap){
            if(!out || cap == 0) {
                return 0;
            }

            out[0] = '\0';

            struct timespec ts;
            if(timespec_get(&ts, TIME_UTC) != TIME_UTC){
                return 0;
            }

            static thread_local i32 cacheValid = 0;
            static thread_local time_t cachedSecond = 0;
            static thread_local char cachedText[32];
            static thread_local usize cachedLen = 0;

            if(!cacheValid || cachedSecond != ts.tv_sec) {
                // invalidate first so a failed refresh cannot leave a valid cache
                cacheValid = 0;
                struct tm timeInfo;

            #ifdef VX_PLATFORM_WIN32
                if (localtime_s(&timeInfo, &ts.tv_sec) != 0) {
                    return 0;
                }
            #else
                if (localtime_r(&ts.tv_sec, &timeInfo) == NULL) {
                    return 0;
                }
            #endif

                usize len = strftime(cachedText, sizeof(cachedText), "%Y-%m-%d %H:%M:%S", &timeInfo);

                if(len == 0) {
                    return 0;
                }

                cachedLen = len;
                cachedSecond = ts.tv_sec;
                cacheValid = 1;
            }

            // check if theres enough space for ".mmm\0"
            if(cap < cachedLen + 5) {
                return 0;
            }

            memcpy(out, cachedText, cachedLen);

            i32 millis = (i32)(ts.tv_nsec / 1000000);
            i32 written = snprintf(out + cachedLen, cap - cachedLen, ".%03d", millis);

            if(written < 0 || (usize)written >= cap - cachedLen) {
                out[0] = '\0';
                return 0;
            }

            return cachedLen + (usize)written;
        }
    }

    // configurations

    void SetLevel(Level level) {
        gConsoleLevel.store((u8)level);
        RecomputeMinLevel();
    }

    void SetFileLevel(Level level) {
        gFileLevel.store((u8)level);
        RecomputeMinLevel();
    }

    Level GetLevel() {
        return (Level)gConsoleLevel.load();
    }

    void SetColor(bool enabled) {
        gColorMode.store(enabled ? 1 : 0);
    }

    void SetConsoleEnabled(bool enabled) {
        gConsoleEnabled.store(enabled);
    }

    void SetShowSource(bool enabled) {
        gShowSource.store(enabled);
    }

    void CloseFileLocked() {
        if(gFile)
            fclose(gFile);

        gFile = nullptr;
    }

    bool SetFile(const char* path, bool append) {
        gWriteMutex.lock();

        if(gFile) {
            CloseFileLocked();
        }

        gFile = fopen(path, append ? "ab" : "wb"); // opens in binary mode so windows bs does not add \r before \n

        gWriteMutex.unlock();
        return gFile ? true : false;
    }

    void CloseFile() {
        gWriteMutex.lock();

        CloseFileLocked();

        gWriteMutex.unlock();
    }

    void SetThreadName(const char* name) {
        // guarntees that the copied string will be null terminated
        // TODO: give some sort of warning of thread name being too long
        strncpy(tName, name, detail::kThreadNameLen);
        tName[detail::kThreadNameLen - 1] = '\0';
    }

    void SetFatalHandler(FatalHandler handler) {
        gFatalHandler = handler;
    }

    void Flush() {
        gWriteMutex.lock();

        fflush(stderr);

        if(gFile)
            fflush(gFile);

        gWriteMutex.unlock();
    }

    // backend
    void Write(Level level, const char* file, int line, std::string_view message) {
        if(level == Level::Off) {
            return;
        }

        const u8 lvl = (u8)level;
        bool wantConsole = gConsoleEnabled && (lvl >= gConsoleLevel);
        bool wantFile = (lvl >= gFileLevel); // recheck after locking with mutex

        if(!wantConsole && !wantFile) {
            return;
        }

        // build plain body in stack buffer outside of lock
        // "<timestamp> [LEVEL] [thread] message" + optional "(file:line)" + \n;
        constexpr usize kCap = detail::kLineBufferSize + 256; // extra space for prefix and suffix of log
        char buf[kCap] = {0};
        usize bufIdx = 0; // index into buffer stack

        // snprintf returns the length it would have written; clamp to what fit.
        auto advance = [&](i32 written) {
            if(written <= 0) {
                return;
            }

            bufIdx = ((usize)written < kCap - bufIdx) ? bufIdx + (usize)written : kCap - 1;
        };

        buf[bufIdx++] = '[';
        usize ts = FormatTimestamp(buf + bufIdx, 32);
        if(ts == 0) {
            buf[bufIdx++] = '?';
        } else {
            bufIdx += ts;
        }
        buf[bufIdx++] = ']';

        // append level and thread name
        advance(snprintf(buf + bufIdx, kCap - bufIdx, " [%s][%s] %.*s", kLevelNames[lvl], tName, (i32)message.size(), message.data()));

        if(gShowSource.load()) {
            advance(snprintf(buf + bufIdx, kCap - bufIdx, " (%s:%d)", BasenameOf(file), line));
        }

        const bool useColor = wantConsole && ColorEnabled();

        gWriteMutex.lock();
        if(wantFile && gFile) {
            fwrite(buf, sizeof(char), bufIdx, gFile);
            fputc('\n', gFile);
            
            if(level >= Level::Warn) {
                fflush(gFile);
            }
        }

        if(wantConsole) {
            if(useColor) {
                fputs(kColors[lvl].start, stderr);
            }

            fwrite(buf, sizeof(char), bufIdx, stderr);

            if(useColor) {
                fputs(kReset, stderr);
            }

            fputc('\n', stderr);
        }

        gWriteMutex.unlock();

        if(level == Level::Fatal) {
            if(tInFatal) {
                abort();
            }

            tInFatal = true;
            Flush();

            if(gFatalHandler) {
                gFatalHandler();
                tInFatal = false;
            } else {
                abort();
            }
        }
    }
}

//#undef _CRT_SECURE_NO_WARNINGS