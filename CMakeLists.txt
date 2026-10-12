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

# sanitizers
set(SANITIZER "none" CACHE STRING "none | asan | tsan")
set_property(CACHE SANITIZER PROPERTY STRINGS none asan tsan)

set(SAN_COMPILE "")
set(SAN_LINK "")

if(NOT SANITIZER STREQUAL "none")
    if(MSVC)
        # msvc only supports asan
        if(SANITIZER STREQUAL "asan")
            set(SAN_COMPILE /fsanitize=address)
        else()
            message(FATAL_ERROR "MSVC supports SANITIZER=asan only")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(SANITIZER STREQUAL "asan")
            set(SAN_COMPILE -fsanitize=address,undefined
                -fno-sanitize-recover=undefined
                -fno-sanitize=vptr
                -fno-omit-frame-pointer)
            set(SAN_LINK -fsanitize=address,undefined)
        elseif(SANITIZER STREQUAL "tsan")
            set(SAN_COMPILE -fsanitize=thread -fno-omit-frame-pointer)
            set(SAN_LINK -fsanitize=thread)
        else()
            message(FATAL_ERROR "Unknown SANITIZER: ${SANITIZER}")
        endif()
    else()
        message(FATAL_ERROR "Sanitizers not supported for this compiler")
    endif()

    add_compile_options(${SAN_COMPILE})
    add_link_options(${SAN_LINK})
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
    "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# targets
add_subdirectory(engine)
add_subdirectory(sandbox)
add_subdirectory(tests)
