cmake_minimum_required(VERSION 3.16)
project(falloutquest_world_tests LANGUAGES CXX)
enable_testing()
find_package(Threads REQUIRED)
add_executable(world_streaming_tests
    world_streaming_tests.cpp
    ../../app/src/main/cpp/world/fo3-world-streaming.cpp)
target_compile_features(world_streaming_tests PRIVATE cxx_std_17)
target_compile_options(world_streaming_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(world_streaming_tests PRIVATE ../../app/src/main/cpp/world)
add_test(NAME world_streaming_tests COMMAND world_streaming_tests)

add_executable(scene_preparation_tests scene_preparation_tests.cpp)
target_compile_features(scene_preparation_tests PRIVATE cxx_std_17)
target_compile_options(scene_preparation_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(scene_preparation_tests PRIVATE ../../app/src/main/cpp/world)
target_link_libraries(scene_preparation_tests PRIVATE Threads::Threads)
add_test(NAME scene_preparation_tests COMMAND scene_preparation_tests)

add_executable(loading_presentation_tests loading_presentation_tests.cpp)
target_compile_features(loading_presentation_tests PRIVATE cxx_std_17)
target_compile_options(loading_presentation_tests PRIVATE -Wall -Wextra -Werror)
find_package(ZLIB REQUIRED)
target_link_libraries(loading_presentation_tests PRIVATE ZLIB::ZLIB)
add_test(NAME loading_presentation_tests COMMAND loading_presentation_tests)

add_executable(interior_lighting_tests interior_lighting_tests.cpp
    ../../app/src/main/cpp/rendering/environment/fo3-interior-lighting.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(interior_lighting_tests PRIVATE cxx_std_17)
target_compile_options(interior_lighting_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(interior_lighting_tests PRIVATE ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(interior_lighting_tests PRIVATE ZLIB::ZLIB)
add_test(NAME interior_lighting_tests COMMAND interior_lighting_tests)
