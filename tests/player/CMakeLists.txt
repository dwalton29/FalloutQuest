cmake_minimum_required(VERSION 3.16)
project(falloutquest_player_tests LANGUAGES CXX)
enable_testing()
find_package(ZLIB REQUIRED)
add_executable(player_state_tests player_state_tests.cpp
    ../../app/src/main/cpp/player/fo3-player-state.cpp
    ../../app/src/main/cpp/player/fo3-container-state.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(player_state_tests PRIVATE cxx_std_17)
target_compile_options(player_state_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(player_state_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(player_state_tests PRIVATE ZLIB::ZLIB)
add_test(NAME player_state_tests COMMAND player_state_tests)
add_executable(interaction_runtime_tests interaction_runtime_tests.cpp
    ../../app/src/main/cpp/player/fo3-player-state.cpp
    ../../app/src/main/cpp/player/fo3-container-state.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(interaction_runtime_tests PRIVATE cxx_std_17)
target_compile_options(interaction_runtime_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(interaction_runtime_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(interaction_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME interaction_runtime_tests COMMAND interaction_runtime_tests)
