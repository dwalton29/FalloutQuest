cmake_minimum_required(VERSION 3.16)
project(falloutquest_world_tests LANGUAGES CXX)
enable_testing()
find_package(Threads REQUIRED)
# Immutable authored CELL / XTEL adjacency, independent of any player or NPC
# scene-transition runtime. Synthetic inputs run in CI; installed Fallout3.esm
# additionally enables original-data Megaton and whole-master verification.
find_package(ZLIB REQUIRED)
add_executable(resident_authored_admission_tests resident_authored_admission_tests.cpp
    ../../app/src/main/cpp/world/fo3-resident-authored-admission.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(resident_authored_admission_tests PRIVATE cxx_std_17)
target_compile_options(resident_authored_admission_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(resident_authored_admission_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(resident_authored_admission_tests PRIVATE ZLIB::ZLIB)
add_test(NAME resident_authored_admission_tests COMMAND resident_authored_admission_tests)
add_executable(xtel_index_tests xtel_index_tests.cpp
    ../../app/src/main/cpp/data/fo3-xtel-index.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(xtel_index_tests PRIVATE cxx_std_17)
target_compile_options(xtel_index_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(xtel_index_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(xtel_index_tests PRIVATE ZLIB::ZLIB)
add_test(NAME xtel_index_tests COMMAND xtel_index_tests)

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

add_executable(reference_placement_tests reference_placement_tests.cpp
    ../../app/src/main/cpp/world/fo3-worldspace-runtime.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(reference_placement_tests PRIVATE cxx_std_17)
target_compile_options(reference_placement_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(reference_placement_tests PRIVATE ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(reference_placement_tests PRIVATE ZLIB::ZLIB Threads::Threads)
add_test(NAME reference_placement_tests COMMAND reference_placement_tests)

add_executable(visual_exchange_tests visual_exchange_tests.cpp)
target_compile_features(visual_exchange_tests PRIVATE cxx_std_17)
target_compile_options(visual_exchange_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(visual_exchange_tests PRIVATE ../../app/src/main/cpp/world)
add_test(NAME visual_exchange_tests COMMAND visual_exchange_tests)

add_executable(resident_resource_tests resident_resource_tests.cpp)
target_compile_features(resident_resource_tests PRIVATE cxx_std_17)
target_compile_options(resident_resource_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(resident_resource_tests PRIVATE ../../app/src/main/cpp/world)
add_test(NAME resident_resource_tests COMMAND resident_resource_tests)

add_executable(resident_gpu_tests resident_gpu_tests.cpp)
target_compile_features(resident_gpu_tests PRIVATE cxx_std_17)
target_compile_options(resident_gpu_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(resident_gpu_tests PRIVATE mock ../../app/src/main/cpp/world)
add_test(NAME resident_gpu_tests COMMAND resident_gpu_tests)

add_executable(resident_lane_tests resident_lane_tests.cpp)
target_compile_features(resident_lane_tests PRIVATE cxx_std_17)
target_compile_options(resident_lane_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(resident_lane_tests PRIVATE ../../app/src/main/cpp/world)
target_link_libraries(resident_lane_tests PRIVATE Threads::Threads)
add_test(NAME resident_lane_tests COMMAND resident_lane_tests)

add_executable(resource_exchange_tests resource_exchange_tests.cpp)
target_compile_features(resource_exchange_tests PRIVATE cxx_std_17)
target_compile_options(resource_exchange_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(resource_exchange_tests PRIVATE ../../app/src/main/cpp/world)
add_test(NAME resource_exchange_tests COMMAND resource_exchange_tests)

add_executable(door_resource_tests door_resource_tests.cpp
    ../../app/src/main/cpp/world/interaction/fo3-door-resources.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(door_resource_tests PRIVATE cxx_std_17)
target_compile_options(door_resource_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(door_resource_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(door_resource_tests PRIVATE ZLIB::ZLIB)
add_test(NAME door_resource_tests COMMAND door_resource_tests)

add_executable(resident_interaction_tests resident_interaction_tests.cpp)
target_compile_features(resident_interaction_tests PRIVATE cxx_std_17)
target_compile_options(resident_interaction_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(resident_interaction_tests PRIVATE ../../app/src/main/cpp/world)
add_test(NAME resident_interaction_tests COMMAND resident_interaction_tests)

add_executable(reference_exchange_tests reference_exchange_tests.cpp)
target_compile_features(reference_exchange_tests PRIVATE cxx_std_17)
target_compile_options(reference_exchange_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(reference_exchange_tests PRIVATE ../../app/src/main/cpp/world)
add_test(NAME reference_exchange_tests COMMAND reference_exchange_tests)

add_executable(resident_exchange_tests resident_exchange_tests.cpp)
target_compile_features(resident_exchange_tests PRIVATE cxx_std_17)
target_compile_options(resident_exchange_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(resident_exchange_tests PRIVATE ../../app/src/main/cpp/world)
add_test(NAME resident_exchange_tests COMMAND resident_exchange_tests)

add_executable(resident_workflow_tests resident_workflow_tests.cpp)
target_compile_features(resident_workflow_tests PRIVATE cxx_std_17)
target_compile_options(resident_workflow_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(resident_workflow_tests PRIVATE ../../app/src/main/cpp/world)
target_link_libraries(resident_workflow_tests PRIVATE Threads::Threads)
add_test(NAME resident_workflow_tests COMMAND resident_workflow_tests)

add_executable(resident_reconcile_runtime_tests resident_reconcile_runtime_tests.cpp)
target_compile_features(resident_reconcile_runtime_tests PRIVATE cxx_std_17)
target_compile_options(resident_reconcile_runtime_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(resident_reconcile_runtime_tests PRIVATE ../../app/src/main/cpp ../../app/src/main/cpp/world)
target_link_libraries(resident_reconcile_runtime_tests PRIVATE Threads::Threads)
add_test(NAME resident_reconcile_runtime_tests COMMAND resident_reconcile_runtime_tests)
