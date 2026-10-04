cmake_minimum_required(VERSION 3.16)
project(falloutquest_asset_tests LANGUAGES CXX)
enable_testing()
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
add_executable(bsa_archive_tests
    bsa_archive_tests.cpp
    ../../app/src/main/cpp/data/fo3-bsa-archive.cpp
    ../../app/src/main/cpp/data/fo3-asset-store.cpp)
target_compile_features(bsa_archive_tests PRIVATE cxx_std_17)
target_compile_options(bsa_archive_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(bsa_archive_tests PRIVATE ../../app/src/main/cpp/data)
target_link_libraries(bsa_archive_tests PRIVATE ZLIB::ZLIB Threads::Threads)
add_test(NAME bsa_archive_tests COMMAND bsa_archive_tests)
add_executable(install_paths_tests install_paths_tests.cpp
    ../../app/src/main/cpp/data/fo3-asset-store.cpp
    ../../app/src/main/cpp/data/fo3-bsa-archive.cpp)
target_compile_features(install_paths_tests PRIVATE cxx_std_17)
target_compile_options(install_paths_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(install_paths_tests PRIVATE ../../app/src/main/cpp/data)
target_link_libraries(install_paths_tests PRIVATE ZLIB::ZLIB Threads::Threads)
add_test(NAME install_paths_tests COMMAND install_paths_tests)
