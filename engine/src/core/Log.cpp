#include <Voxide/core/Log.h>

#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>

#if defined(VX_PLATFORM_WIN32)
#include <io.h>
#include <windows.h>

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
            detail::gMinLevel.store(min(gConsoleLevel, gFileLevel));
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
            i32 fd fileno(stderr);
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

    bool SetFile(const char* path, bool append) {
        gWriteMutex.lock();

        if(gFile) {
            CloseFile();
        }

        gFile = fopen(path, append ? "a" : "w");

        gWriteMutex.unlock();
        return gFile ? true : false;
    }

    void CloseFile() {
        gWriteMutex.lock();

        if(gFile)
            fclose(gFile);

        gFile = nullptr;

        gWriteMutex.unlock();
    }

    void SetThreadName(const char* name) {
        // guarntees that the copied string will be null terminated
        // TODO: give some sort of warning of thread name being too long
        strncpy(tName, name, detail::kThreadNameLen);

        if(strlen(tName) > detail::kThreadNameLen - 1) {
            tName[detail::kThreadNameLen - 1] = '\0';
        }
    }

    void SetFatalHandler(FatalHandler handler) {
        if(handler)
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
        bool wantConsole = gConsoleEnabled && ((u8)level >= gConsoleLevel);
        bool wantFile = ((u8)level >= gFileLevel); // recheck after locking with mutex

        if(!wantConsole && !wantFile)
            return;

        // build plain body in stack buffer outside of lock
        // "<timestamp> [LEVEL] [thread] message" + optional "(file:line)" + \n;
        i8 size = 0;
        char buf[detail::kLineBufferSize] = {0};
        bool colorEnabled = ColorEnabled() && (level != Level::Off);
        u8 bufIdx = colorEnabled ? strlen(kColors[(u8)level].start) : 0; // index into buffer stack

        buf[bufIdx++] = '[';
        size = (i8)FormatTimestamp(buf + bufIdx, 32);
        if(size == 0) {
            // error within formatting time stamp
            return;
        }

        bufIdx += (u8)size;
        buf[bufIdx++] = ']';

        // append level and thread name
        size = snprintf(buf + bufIdx, detail::kLineBufferSize - bufIdx, " [%s][%s] %s", kLevelNames[(u8)level], tName, message.data());
        if(size < 0) {
            perror("snprintf");
            return;
        }

        bufIdx += (u8)size;

        if(gShowSource) {
            if((size = snprintf(buf + bufIdx, detail::kLineBufferSize - bufIdx, "(%s:%d)", BasenameOf(file), line)) < 0) {
                perror("snprintf");
                return;
            }

            bufIdx += (u8)size;
        }

        if(colorEnabled) {
            
        }

        gWriteMutex.lock();
        if(wantFile && gFile) {
            fwrite(buf, sizeof(char), bufIdx, gFile);
            
            if(level >= Level::Warn) {
                fflush(gFile);
            }
        }

        if(colorEnabled) {
            size = strlen(kColors[(u8)level].start);
            for(u8 i = 0; i < size; i++) {
                if(buf[i] == '\0') {
                    buf[i] = kColors[(u8)level].start[i];
                }
            }

            size = strlen(kReset);
            for(u8 i = 0; i < size;) {
                if(buf[bufIdx] == '\0') {
                    buf[bufIdx++] = kReset[i++];
                }
            }
        }

        buf[bufIdx++] = '\n';

        if(wantConsole) {
            fwrite(buf, sizeof(char), bufIdx, stderr);
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
            } else {
                abort();
            }
        }
    }
}

//#undef _CRT_SECURE_NO_WARNINGS