cmake_minimum_required(VERSION 3.25...4.4)

# Required for CMAKE_MSVC_RUNTIME_LIBRARY to control /MT vs /MD.
cmake_policy(SET CMP0091 NEW)

# Default; the sanitizer section may override this for clang-cl ASan.
set(CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

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

# sanitizers
set(SANITIZER "none" CACHE STRING "none | asan | tsan")
set_property(CACHE SANITIZER PROPERTY STRINGS none asan tsan)

set(VX_IS_CLANG_CL OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(VX_IS_CLANG_CL ON)
endif()

if(NOT SANITIZER STREQUAL "none")
    if(MSVC)
        if(NOT SANITIZER STREQUAL "asan")
            message(FATAL_ERROR
                "MSVC supports SANITIZER=asan only; got '${SANITIZER}'.")
        endif()

        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
        add_compile_options(/fsanitize=address)

    elseif(VX_IS_CLANG_CL)
        if(NOT SANITIZER STREQUAL "asan")
            message(FATAL_ERROR
                "clang-cl supports SANITIZER=asan only; got '${SANITIZER}'.")
        endif()

        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
        add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
        add_compile_options(/fsanitize=address)

        set(VX_LLVM_ASAN_DIR
        "C:/Program Files/LLVM/lib/clang/18/lib/windows")

        set(VX_ASAN_LIBS
            "${VX_LLVM_ASAN_DIR}/clang_rt.asan-x86_64.lib"
            "${VX_LLVM_ASAN_DIR}/clang_rt.asan_cxx-x86_64.lib"
    )

        message(STATUS "Sanitizer: clang-cl AddressSanitizer")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(SANITIZER STREQUAL "asan")
            set(VX_SANITIZER_FLAGS
                -fsanitize=address,undefined
                -fno-sanitize-recover=undefined
                -fno-sanitize=vptr
                -fno-omit-frame-pointer
            )
        elseif(SANITIZER STREQUAL "tsan")
            set(VX_SANITIZER_FLAGS
                -fsanitize=thread
                -fno-omit-frame-pointer
            )
        else()
            message(FATAL_ERROR "Unknown SANITIZER: '${SANITIZER}'.")
        endif()

        add_compile_options(${VX_SANITIZER_FLAGS})
        add_link_options(${VX_SANITIZER_FLAGS})

    else()
        message(FATAL_ERROR
            "Sanitizers are not configured for compiler "
            "'${CMAKE_CXX_COMPILER_ID}' "
            "with frontend '${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}'.")
    endif()

    message(STATUS "Sanitizer: ${SANITIZER}")
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