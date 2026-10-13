cmake_minimum_required(VERSION 3.25...4.4)

project(VoxideEngine 
    VERSION 0.0.1
    LANGUAGES CXX)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp")

set(PRIVATE_INCLUDE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core")

# set platform dependant source files, headers and macros
if(WIN32)
    file(GLOB_RECURSE PLATFORM_SRC CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/Windows/*.cpp")

    list(APPEND PRIVATE_INCLUDE 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/Windows")

    set(PLATFORM_DEFINITION VX_PLATFORM_WIN32=1 _CRT_SECURE_NO_WARNINGS=1)
elseif(APPLE)
    message(FATAL_ERROR "MacOS is not supported at this time.")
    file(GLOB_RECURSE PLATFORM_SRC CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/MacOS/*.cpp")

    list(APPEND PRIVATE_INCLUDE 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/MacOS")

    set(PLATFORM_DEFINITION VX_PLATFORM_MACOS=1)
elseif(UNIX)
    file(GLOB_RECURSE PLATFORM_SRC CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/Linux/*.cpp")

    list(APPEND PRIVATE_INCLUDE 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/Linux")

    set(PLATFORM_DEFINITION VX_PLATFORM_LINUX=1)
else()
    message(FATAL_ERROR "Unsupported OS")
endif()

list(APPEND SOURCES ${PLATFORM_SRC})

add_library(VoxideEngine STATIC ${SOURCES})

target_include_directories(VoxideEngine
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE ${PRIVATE_INCLUDE})

target_compile_features(VoxideEngine PUBLIC cxx_std_20)

target_compile_definitions(VoxideEngine PRIVATE ${PLATFORM_DEFINITION})

target_compile_definitions(VoxideEngine PUBLIC
    $<$<CONFIG:Debug>:VX_DEBUG>
    $<$<CONFIG:RelDeb>:VX_RELDEB>
    $<$<CONFIG:Release>:VX_RELEASE>
    $<$<CONFIG:MinSize>:VX_MINSIZE>)

# Compiler warnings
if(MSVC)
    target_compile_options(VoxideEngine PRIVATE 
        /W4
        /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(VoxideEngine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror)
endif()

# Platform specific library linking
if(WIN32)
    #target_link_libraries(VoxEngine PRIVATE ...)
elseif(APPLE)

elseif(UNIX)
endif()

# External libraries, likely only one will be Vulkan

