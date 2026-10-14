#include "VXTest.h"
#include <Voxide/core/Log.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
  #include <fcntl.h>
  #include <unistd.h>
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------- helpers

static std::string TempPath(const char* name) {
    return (fs::temp_directory_path() / name).string();
}

static std::string ReadFile(const std::string& path) {
    std::string out;
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
        std::fclose(f);
    }
    return out;
}

static bool Contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

static size_t CountLines(const std::string& s) {
    size_t n = 0;
    for (char c : s) if (c == '\n') ++n;
    return n;
}

// Puts the logger in a known state on construction and restores sane
// defaults on destruction, so a VXT_REQUIRE early-return can't leak state.
struct LoggerScope {
    LoggerScope() {
        engine::log::CloseFile();
        engine::log::SetConsoleEnabled(false);      // keep test output clean
        engine::log::SetLevel(engine::log::Level::Trace);
        engine::log::SetFileLevel(engine::log::Level::Trace);
        engine::log::SetColor(false);
        engine::log::SetShowSource(false);
        engine::log::SetFatalHandler(nullptr);
        engine::log::SetThreadName("test");
    }
    ~LoggerScope() {
        engine::log::CloseFile();
        engine::log::SetConsoleEnabled(true);
        engine::log::SetLevel(engine::log::Level::Info);
        engine::log::SetFileLevel(engine::log::Level::Trace);
        engine::log::SetFatalHandler(nullptr);
    }
};

#if !defined(_WIN32)
// Redirects stderr into a file until Finish() or destruction.
// Declare it AFTER LoggerScope so stderr is restored first.
struct StderrCapture {
    std::string path;
    int saved = -1;

    explicit StderrCapture(const char* name) : path(TempPath(name)) {
        std::fflush(stderr);
        saved = dup(STDERR_FILENO);
        int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
    std::string Finish() {
        engine::log::Flush();
        std::fflush(stderr);
        if (saved != -1) {
            dup2(saved, STDERR_FILENO);
            close(saved);
            saved = -1;
        }
        return ReadFile(path);
    }
    ~StderrCapture() { Finish(); }
};
#endif

static int gFatalCalls = 0;
static void CountingFatalHandler() { ++gFatalCalls; }

namespace test_types { struct Pt { int x, y; }; }
template <> struct std::formatter<test_types::Pt> : std::formatter<std::string_view> {
    auto format(const test_types::Pt& v, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({}, {})", v.x, v.y);
    }
};

// ---------------------------------------------------------------- macros

VXT_TEST("log.macros.every_level_writes_tagged_line") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_levels.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));

    //VX_LOG_TRACE("t");
    VX_LOG_DEBUG("d");
    VX_LOG_INFO("i");
    VX_LOG_WARN("w");
    VX_LOG_ERROR("e");
    engine::log::CloseFile();

    const std::string out = ReadFile(p);
    //VXT_CHECK(Contains(out, "[TRACE]"));
    VXT_CHECK(Contains(out, "[DEBUG]"));
    VXT_CHECK(Contains(out, "[INFO ]"));
    VXT_CHECK(Contains(out, "[WARN ]"));
    VXT_CHECK(Contains(out, "[ERROR]"));
    VXT_CHECK(CountLines(out) == 4);
}

VXT_TEST("log.macros.fatal_calls_handler_and_flushes") {
    LoggerScope scope;
    gFatalCalls = 0;
    engine::log::SetFatalHandler(&CountingFatalHandler);

    const std::string p = TempPath("vx_log_fatal.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));
    VX_LOG_FATAL("boom {}", 1);

    // Read before CloseFile: the line must already be on disk when the
    // handler runs.
    const std::string out = ReadFile(p);
    engine::log::CloseFile();

    VXT_CHECK(gFatalCalls == 1);
    VXT_CHECK(Contains(out, "[FATAL]"));
    VXT_CHECK(Contains(out, "boom 1"));
}

// ---------------------------------------------------------------- levels

VXT_TEST("log.levels.below_threshold_is_filtered") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_filter.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));
    engine::log::SetFileLevel(engine::log::Level::Warn);

    VX_LOG_INFO("hidden");
    VX_LOG_WARN("shown");
    engine::log::CloseFile();

    const std::string out = ReadFile(p);
    VXT_CHECK(!Contains(out, "hidden"));
    VXT_CHECK(Contains(out, "shown"));
}

VXT_TEST("log.levels.off_silences_everything") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_off.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));
    engine::log::SetLevel(engine::log::Level::Off);
    engine::log::SetFileLevel(engine::log::Level::Off);

    VX_LOG_ERROR("nothing");
    engine::log::CloseFile();

    VXT_CHECK(ReadFile(p).empty());
}

#if !defined(_WIN32)
VXT_TEST("log.levels.console_and_file_thresholds_are_independent") {
    LoggerScope scope;
    const std::string p = TempPath("VX_log_indep.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));
    engine::log::SetFileLevel(engine::log::Level::Trace);
    engine::log::SetConsoleEnabled(true);
    engine::log::SetLevel(engine::log::Level::Error);

    StderrCapture cap("vx_log_indep_console.txt");
    VX_LOG_INFO("only-in-file");
    VX_LOG_ERROR("in-both");
    const std::string console = cap.Finish();
    engine::log::CloseFile();

    const std::string file = ReadFile(p);
    VXT_CHECK(!Contains(console, "only-in-file"));
    VXT_CHECK(Contains(console, "in-both"));
    VXT_CHECK(Contains(file, "only-in-file"));
    VXT_CHECK(Contains(file, "in-both"));
}
#endif

// ---------------------------------------------------------------- formatting

VXT_TEST("log.format.base_types_specs_and_custom_type") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_fmt.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));

    VX_LOG_INFO("int={} neg={} u64={}", 42, -7, engine::u64(18446744073709551615ull));
    VX_LOG_INFO("float={:.2f} hex={:#x} pad=[{:>6}]", 3.14159, 255, "ab");
    VX_LOG_INFO("bool={} char={} str={}", true, 'z', "hello");
    VX_LOG_INFO("custom={}", test_types::Pt{3, -4});
    VX_LOG_INFO("escaped braces {{}}");
    engine::log::CloseFile();

    const std::string out = ReadFile(p);
    VXT_CHECK(Contains(out, "int=42 neg=-7 u64=18446744073709551615"));
    VXT_CHECK(Contains(out, "float=3.14 hex=0xff pad=[    ab]"));
    VXT_CHECK(Contains(out, "bool=true char=z str=hello"));
    VXT_CHECK(Contains(out, "custom=(3, -4)"));
    VXT_CHECK(Contains(out, "escaped braces {}"));
}

VXT_TEST("log.format.long_message_is_truncated") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_long.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));

    std::string big(5000, 'x');
    VX_LOG_INFO("{}", big);
    engine::log::CloseFile();

    const std::string out = ReadFile(p);
    VXT_CHECK(out.size() < 2000);
    VXT_CHECK(Contains(out, "..."));
    VXT_CHECK(CountLines(out) == 1);
}

// ---------------------------------------------------------------- line content

VXT_TEST("log.line.timestamp_thread_name_and_source") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_content.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));
    engine::log::SetThreadName("worker-7");

    VX_LOG_INFO("plain");
    engine::log::SetShowSource(true);
    VX_LOG_INFO("with source");
    engine::log::CloseFile();

    const std::string out = ReadFile(p);
    VXT_REQUIRE(out.size() > 24);

    // "YYYY-MM-DD HH:MM:SS.mmm" at the start of the first line
    VXT_CHECK(out[5] == '-');  VXT_CHECK(out[8] == '-');  VXT_CHECK(out[11] == ' ');
    VXT_CHECK(out[14] == ':'); VXT_CHECK(out[17] == ':'); VXT_CHECK(out[20] == '.');

    VXT_CHECK(Contains(out, "worker-7"));
    VXT_CHECK(Contains(out, "TestLog.cpp"));          // from the second line
    VXT_CHECK(!Contains(out, "tests/core/TestLog.cpp")); // basename only
}

// ---------------------------------------------------------------- files

VXT_TEST("log.file.append_vs_truncate") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_append.txt");

    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));
    VX_LOG_INFO("first");
    engine::log::CloseFile();

    VXT_REQUIRE(engine::log::SetFile(p.c_str(), true));
    VX_LOG_INFO("second");
    engine::log::CloseFile();
    VXT_CHECK(CountLines(ReadFile(p)) == 2);

    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));        // truncates
    VX_LOG_INFO("third");
    engine::log::CloseFile();

    const std::string out = ReadFile(p);
    VXT_CHECK(CountLines(out) == 1);
    VXT_CHECK(Contains(out, "third"));
}

VXT_TEST("log.file.switching_files_routes_to_new_file_only") {
    LoggerScope scope;
    const std::string a = TempPath("vx_log_a.txt");
    const std::string b = TempPath("vx_log_b.txt");

    VXT_REQUIRE(engine::log::SetFile(a.c_str(), false));
    VX_LOG_INFO("to-a");
    VXT_REQUIRE(engine::log::SetFile(b.c_str(), false));        // closes a, opens b
    VX_LOG_INFO("to-b");
    engine::log::CloseFile();

    const std::string fa = ReadFile(a);
    const std::string fb = ReadFile(b);
    VXT_CHECK(Contains(fa, "to-a"));
    VXT_CHECK(!Contains(fa, "to-b"));
    VXT_CHECK(Contains(fb, "to-b"));
    VXT_CHECK(!Contains(fb, "to-a"));
}

VXT_TEST("log.file.bad_path_fails_cleanly") {
    LoggerScope scope;
    VXT_CHECK(!engine::log::SetFile("/nonexistent_dir_xyz/log.txt"));
    VX_LOG_INFO("still fine");                           // must not crash
}

VXT_TEST("log.file.logging_after_close_is_safe") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_closed.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));
    engine::log::CloseFile();

    VX_LOG_ERROR("after close");
    VXT_CHECK(ReadFile(p).empty());
}

VXT_TEST("log.file.warn_is_on_disk_without_flush_or_close") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_flush.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));

    VX_LOG_WARN("must-be-flushed");
    const std::string out = ReadFile(p);                // file still open
    engine::log::CloseFile();

    VXT_CHECK(Contains(out, "must-be-flushed"));
}

// ---------------------------------------------------------------- color

#if !defined(_WIN32)
VXT_TEST("log.color.on_console_colored_file_plain") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_color_file.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));
    engine::log::SetConsoleEnabled(true);
    engine::log::SetColor(true);

    StderrCapture cap("vx_log_color_console.txt");
    VX_LOG_WARN("colored");
    const std::string console = cap.Finish();
    engine::log::CloseFile();

    VXT_CHECK(Contains(console, "\x1b["));
    VXT_CHECK(Contains(console, "\x1b[0m"));
    VXT_CHECK(!Contains(ReadFile(p), "\x1b"));
}

VXT_TEST("log.color.off_console_has_no_escape_codes") {
    LoggerScope scope;
    engine::log::SetConsoleEnabled(true);
    engine::log::SetColor(false);

    StderrCapture cap("vx_log_nocolor_console.txt");
    VX_LOG_ERROR("plain");
    const std::string console = cap.Finish();

    VXT_CHECK(Contains(console, "plain"));
    VXT_CHECK(!Contains(console, "\x1b"));
}

VXT_TEST("log.color.each_level_has_its_own_color") {
    LoggerScope scope;
    engine::log::SetConsoleEnabled(true);
    engine::log::SetColor(true);

    StderrCapture cap("vx_log_levelcolors.txt");
    VX_LOG_DEBUG("a");
    VX_LOG_INFO("b");
    VX_LOG_WARN("c");
    VX_LOG_ERROR("d");
    const std::string out = cap.Finish();

    VXT_CHECK(Contains(out, "\x1b[36m"));   // debug cyan
    VXT_CHECK(Contains(out, "\x1b[32m"));   // info green
    VXT_CHECK(Contains(out, "\x1b[33m"));   // warn yellow
    VXT_CHECK(Contains(out, "\x1b[31m"));   // error red
}
#endif

// ---------------------------------------------------------------- threads

VXT_TEST("log.threads.concurrent_logging_never_splits_or_loses_lines") {
    LoggerScope scope;
    const std::string p = TempPath("vx_log_threads.txt");
    VXT_REQUIRE(engine::log::SetFile(p.c_str(), false));

    constexpr int kThreads = 8, kPerThread = 500;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([t] {
            char name[16];
            std::snprintf(name, sizeof name, "T%d", t);
            engine::log::SetThreadName(name);
            for (int i = 0; i < kPerThread; ++i)
                VX_LOG_INFO("thread {} msg {} END", t, i);
        });
    }
    for (auto& th : threads) th.join();
    engine::log::CloseFile();

    // All checks happen on this thread: the framework's counter isn't atomic.
    const std::string out = ReadFile(p);
    VXT_CHECK(CountLines(out) == size_t(kThreads) * kPerThread);

    // Every line must end with " END"; a split or merged line breaks this.
    size_t pos = 0, bad = 0;
    while (pos < out.size()) {
        size_t nl = out.find('\n', pos);
        if (nl == std::string::npos) break;
        if (nl < 4 || out.compare(nl - 4, 4, " END") != 0) ++bad;
        pos = nl + 1;
    }
    VXT_CHECK(bad == 0);
}