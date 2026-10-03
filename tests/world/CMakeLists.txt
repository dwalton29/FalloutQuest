cmake_minimum_required(VERSION 3.16)
project(falloutquest_world_tests LANGUAGES CXX)
enable_testing()
add_executable(world_streaming_tests
    world_streaming_tests.cpp
    ../../app/src/main/cpp/world/fo3-world-streaming.cpp)
target_compile_features(world_streaming_tests PRIVATE cxx_std_17)
target_compile_options(world_streaming_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(world_streaming_tests PRIVATE ../../app/src/main/cpp/world)
add_test(NAME world_streaming_tests COMMAND world_streaming_tests)
