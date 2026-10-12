#!/usr/bin/env bash

set -euo pipefail

CONFIG=Debug
COMPILER=clang
SANITIZER=none
DO_TEST=0
DO_CLEAN=0

usage() {
    echo "usage: $0 [--config Debug|RelDeb|Release|MinSize]"
    echo "          [--compiler gcc|clang] [--sanitizer none|asan|tsan]"
    echo "          [--test] [--clean]"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --config) CONFIG="$2"; shift 2 ;;
        --compiler) COMPILER="$2"; shift 2 ;;
        --sanitizer) SANITIZER="$2"; shift 2 ;;
        --test) DO_TEST=1; shift ;;
        --clean) DO_CLEAN=1; shift ;;
        *) usage ;;
    esac
done

case "$COMPILER" in
    gcc) export CC=gcc CXX=g++ ;;
    clang) export CC=clang CXX=clang++ ;;
    *) usage ;;
esac

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT/bin-int/$COMPILER-$CONFIG-$SANITIZER"

[[ $DO_CLEAN -eq 1 ]] && rm -rf "$BUILD_DIR" && rm -rf "bin"

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DSANITIZER="$SANITIZER" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "$BUILD_DIR"

if [[ $DO_TEST -eq 1 ]]; then
    export ASAN_OPTIONS="detect_leaks=1:strict_string_check=1"
    export UBSAN_OPTIONS="print_stacktrace=1"
    export TSAN_OPTIONS="halt_on_error=1"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi
