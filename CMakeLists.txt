cmake_minimum_required(VERSION 3.0.0)
project(FileExample VERSION 0.1.0)
set(device_compiler_cxx_standard "-std=c++20")


add_executable(FileExample src/main.cpp)
target_link_libraries(FileExample PUBLIC gtest_main gtest)
