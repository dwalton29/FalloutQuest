cmake_minimum_required(VERSION 3.16)
project(falloutquest_audio_tests LANGUAGES CXX)
enable_testing()
find_package(ZLIB REQUIRED)
add_executable(audio_catalog_tests audio_catalog_tests.cpp
 ../../app/src/main/cpp/audio/fo3-audio-catalog.cpp
 ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(audio_catalog_tests PRIVATE cxx_std_17)
target_compile_options(audio_catalog_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(audio_catalog_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(audio_catalog_tests PRIVATE ZLIB::ZLIB)
add_test(NAME audio_catalog_tests COMMAND audio_catalog_tests)
add_executable(audio_assets_tests audio_assets_tests.cpp
 ../../app/src/main/cpp/audio/fo3-audio-catalog.cpp
 ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(audio_assets_tests PRIVATE cxx_std_17)
target_compile_options(audio_assets_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(audio_assets_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(audio_assets_tests PRIVATE ZLIB::ZLIB)
add_test(NAME audio_assets_tests COMMAND audio_assets_tests)
