#include <cstdio>

#include <Voxide/core/Version.h>

int main() {
    std::printf("Engine online (v%s)\n", engine::EngineVersion());

    return 0;
}
