
#!/usr/bin/env bash

set -euo pipefail

CONFIG=Debug
COMPILER=clang
SANITIZER=none
DO_TEST=0
DO_CLEAN=0

usage() {
    echo "Usage: $0 [--config Debug|Release|RelDeb|MinSize]"
    echo "          [--compiler gcc|clang]"
    echo "          [--sanitizer none|asan|tsan]"
    echo "          [--test] [--clean]"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --config)
            [[ $# -ge 2 ]] || usage
            CONFIG="$2"
            shift 2
            ;;
        --compiler)
            [[ $# -ge 2 ]] || usage
            COMPILER="$2"
            shift 2
            ;;
        --sanitizer)
            [[ $# -ge 2 ]] || usage
            SANITIZER="$2"
            shift 2
            ;;
        --test)
            DO_TEST=1
            shift
            ;;
        --clean)
            DO_CLEAN=1
            shift
            ;;
        *)
            usage
            ;;
    esac
done

case "$CONFIG" in
    Debug|Release|RelDeb|MinSize) ;;
    *) usage ;;
esac

case "$COMPILER" in
    gcc)
        C_COMPILER=gcc
        CXX_COMPILER=g++
        CMAKE_COMPILER_ID=GNU
        ;;
    clang)
        C_COMPILER=clang
        CXX_COMPILER=clang++
        CMAKE_COMPILER_ID=Clang
        ;;
    *)
        usage
        ;;
esac

case "$SANITIZER" in
    none|asan|tsan) ;;
    *) usage ;;
esac

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT/bin-int/$COMPILER-$CONFIG-$SANITIZER"
OUTPUT_DIR="$ROOT/bin/$CMAKE_COMPILER_ID-$CONFIG-$SANITIZER"

if [[ "$DO_CLEAN" -eq 1 ]]; then
    rm -rf "$ROOT/bin-int" "$ROOT/bin"
fi

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_C_COMPILER="$C_COMPILER" \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DSANITIZER="$SANITIZER" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "$BUILD_DIR"

if [[ "$SANITIZER" == "asan" ]]; then
        export ASAN_OPTIONS="detect_leaks=1:strict_string_check=1"
        export UBSAN_OPTIONS="print_stacktrace=1"
    elif [[ "$SANITIZER" == "tsan" ]]; then
        export TSAN_OPTIONS="halt_on_error=1"
fi

if [[ "$DO_TEST" -eq 1 ]]; then
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi