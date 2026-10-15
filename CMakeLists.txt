cmake_minimum_required(VERSION 3.25...4.4)

project(Voxide LANGUAGES CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Available build configurations
set(VX_CONFIGS Debug Release RelDeb MinSize)

get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(IS_MULTI_CONFIG)
    # visual studio, xcode, ninja multi-config: configs chosen at build time
    set(CMAKE_CONFIGURATION_TYPES "${VX_CONFIGS}" CACHE STRING "" FORCE)
else()
    # ninja, makefiles: one config per build dir
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${VX_CONFIGS})
endif()

file(GLOB VX_ASAN_RUNTIME_CANDIDATES
    "${VX_CLANG_RESOURCE_DIR}/lib/windows/*asan*x86_64*.lib"
)

message(STATUS "Clang ASan runtime candidates:")
foreach(_lib IN LISTS VX_ASAN_RUNTIME_CANDIDATES)
    message(STATUS "  ${_lib}")
endforeach()

# Sanitizers
set(SANITIZER "none" CACHE STRING "none | asan | tsan")
set_property(CACHE SANITIZER PROPERTY STRINGS none asan tsan)

set(VX_IS_CLANG_CL OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(VX_IS_CLANG_CL ON)
endif()

# Sanitizers
# Detect clang-cl explicitly. clang-cl identifies as Clang, but uses the
# MSVC command-line and linker conventions.
set(VX_IS_CLANG_CL FALSE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(VX_IS_CLANG_CL TRUE)
endif()

if(SANITIZER STREQUAL "asan")

    if(VX_IS_CLANG_CL)

        message(STATUS "Sanitizer: clang-cl AddressSanitizer")

        # Use the dynamic MSVC runtime (/MD, /MDd).
        # CMP0091 must be NEW, and this must be set before targets are created.
        set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )

        # Compile every target with AddressSanitizer.
        add_compile_options(/fsanitize=address)

        # Required for compatibility with the Windows ASan runtime.
        add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)

        # Discover the runtime libraries from the active compiler installation.
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" --print-resource-dir
            OUTPUT_VARIABLE VX_CLANG_RESOURCE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE VX_RESOURCE_DIR_RESULT
        )

        if(NOT VX_RESOURCE_DIR_RESULT EQUAL 0
           OR NOT EXISTS "${VX_CLANG_RESOURCE_DIR}/lib/windows")
            message(FATAL_ERROR
                "Could not locate the clang-cl runtime directory. "
                "Compiler: ${CMAKE_CXX_COMPILER}"
            )
        endif()

        find_library(VX_ASAN_DYNAMIC_LIB
            NAMES clang_rt.asan_dynamic-x86_64
            PATHS "${VX_CLANG_RESOURCE_DIR}/lib/windows"
            NO_DEFAULT_PATH
        )

        find_library(VX_ASAN_THUNK_LIB
            NAMES clang_rt.asan_dynamic_runtime_thunk-x86_64
            PATHS "${VX_CLANG_RESOURCE_DIR}/lib/windows"
            NO_DEFAULT_PATH
        )

        if(NOT VX_ASAN_DYNAMIC_LIB OR NOT VX_ASAN_THUNK_LIB)
            message(FATAL_ERROR
                "Could not find the required Windows ASan runtime libraries.\n"
                "Runtime directory: ${VX_CLANG_RESOURCE_DIR}/lib/windows\n"
                "Dynamic runtime: ${VX_ASAN_DYNAMIC_LIB}\n"
                "Dynamic thunk: ${VX_ASAN_THUNK_LIB}"
            )
        endif()

        message(STATUS "ASan dynamic runtime: ${VX_ASAN_DYNAMIC_LIB}")
        message(STATUS "ASan dynamic thunk:   ${VX_ASAN_THUNK_LIB}")

        # Represent the thunk archive as an imported static library.
        add_library(VX_ASAN_THUNK STATIC IMPORTED GLOBAL)

        set_target_properties(VX_ASAN_THUNK PROPERTIES
            IMPORTED_LOCATION "${VX_ASAN_THUNK_LIB}"
        )

        # Apply to targets in this directory and its subdirectories.
        # CMake generates the correct whole-archive syntax for the linker.
        link_libraries(
            "${VX_ASAN_DYNAMIC_LIB}"
            "$<LINK_LIBRARY:WHOLE_ARCHIVE,VX_ASAN_THUNK>"
        )

    elseif(MSVC)

        message(STATUS "Sanitizer: MSVC AddressSanitizer")

        add_compile_options(/fsanitize=address)

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

        message(STATUS "Sanitizer: AddressSanitizer + UndefinedBehaviorSanitizer")

        add_compile_options(
            -fsanitize=address,undefined
            -fno-sanitize-recover=undefined
            -fno-sanitize=vptr
            -fno-omit-frame-pointer
        )

        add_link_options(
            -fsanitize=address,undefined
        )

    else()

        message(FATAL_ERROR
            "AddressSanitizer is not configured for "
            "${CMAKE_CXX_COMPILER_ID}"
        )

    endif()

elseif(SANITIZER STREQUAL "tsan")

    if(VX_IS_CLANG_CL OR MSVC)

        message(FATAL_ERROR
            "ThreadSanitizer is not supported by this Windows configuration."
        )

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

        message(STATUS "Sanitizer: ThreadSanitizer")

        add_compile_options(
            -fsanitize=thread
            -fno-omit-frame-pointer
        )

        add_link_options(
            -fsanitize=thread
        )

    else()

        message(FATAL_ERROR
            "ThreadSanitizer is not configured for "
            "${CMAKE_CXX_COMPILER_ID}"
        )

    endif()

elseif(NOT SANITIZER STREQUAL "none")

    message(FATAL_ERROR "Unknown sanitizer: ${SANITIZER}")

else()

    message(STATUS "Sanitizer: none")

endif()

# Reuse CMake's standard configuration flags.
# These standard variables are initialized by project() above.
foreach(_lang C CXX)
    set(CMAKE_${_lang}_FLAGS_RELDEB
        "${CMAKE_${_lang}_FLAGS_RELWITHDEBINFO}"
        CACHE STRING "Flags for RelDeb configuration"
    )

    set(CMAKE_${_lang}_FLAGS_MINSIZE
        "${CMAKE_${_lang}_FLAGS_MINSIZEREL}"
        CACHE STRING "Flags for MinSize configuration"
    )
endforeach()

# Reuse standard linker flags too.
foreach(_kind EXE SHARED MODULE)
    set(CMAKE_${_kind}_LINKER_FLAGS_RELDEB
        "${CMAKE_${_kind}_LINKER_FLAGS_RELWITHDEBINFO}"
        CACHE STRING "Linker flags for RelDbg configuration"
    )

    set(CMAKE_${_kind}_LINKER_FLAGS_MINSIZE
        "${CMAKE_${_kind}_LINKER_FLAGS_MINSIZEREL}"
        CACHE STRING "Linker flags for MinSize configuration"
    )
endforeach()

add_compile_options(
    "$<$<CONFIG:RelDeb>:/Zi>"
)

add_link_options(
    "$<$<CONFIG:RelDeb>:/DEBUG>"
)

enable_testing()

# output dirs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_SOURCE_DIR}/bin/${CMAKE_CXX_COMPILER_ID}-$<CONFIG>-${SANITIZER}")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    "${CMAKE_SOURCE_DIR}/bin/${CMAKE_CXX_COMPILER_ID}-$<CONFIG>-${SANITIZER}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    "${CMAKE_SOURCE_DIR}/bin/${CMAKE_CXX_COMPILER_ID}-$<CONFIG>-${SANITIZER}")

# targets
add_subdirectory(engine)
add_subdirectory(sandbox)
add_subdirectory(tests)