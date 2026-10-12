cmake_minimum_required(VERSION 3.25...4.4)

project(Sandbox LANGUAGES CXX)

add_executable(Sandbox src/main.cpp)

set_target_properties(Sandbox PROPERTIES
    CXX_SCAN_FOR_MODULES OFF)

target_link_libraries(Sandbox PRIVATE VoxideEngine)
