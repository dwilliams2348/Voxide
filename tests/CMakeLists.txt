cmake_minimum_required(VERSION 3.25...4.4)

add_executable(VoxideTests main.cpp VXTest.cpp TestVersion.cpp TestAssert.cpp)

target_link_libraries(VoxideTests PRIVATE VoxideEngine)

enable_testing()
add_test(NAME unit COMMAND VoxideTests)
