cmake_minimum_required(VERSION 3.16)
project(falloutquest_player_tests LANGUAGES CXX)
enable_testing()
find_package(ZLIB REQUIRED)
add_executable(player_state_tests player_state_tests.cpp
    ../../app/src/main/cpp/player/fo3-player-state.cpp
    ../../app/src/main/cpp/player/fo3-pipboy-player.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-data.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-session.cpp
    ../../app/src/main/cpp/pipboy/fo3-radio-runtime.cpp
    ../../app/src/main/cpp/player/fo3-container-state.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(player_state_tests PRIVATE cxx_std_17)
target_compile_options(player_state_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(player_state_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(player_state_tests PRIVATE ZLIB::ZLIB)
add_test(NAME player_state_tests COMMAND player_state_tests)
add_executable(interaction_runtime_tests interaction_runtime_tests.cpp
    ../../app/src/main/cpp/player/fo3-player-state.cpp
    ../../app/src/main/cpp/player/fo3-pipboy-player.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-data.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-session.cpp
    ../../app/src/main/cpp/pipboy/fo3-radio-runtime.cpp
    ../../app/src/main/cpp/player/fo3-container-state.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(interaction_runtime_tests PRIVATE cxx_std_17)
target_compile_options(interaction_runtime_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(interaction_runtime_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(interaction_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME interaction_runtime_tests COMMAND interaction_runtime_tests)
add_executable(shoulder_notification_tests shoulder_notification_tests.cpp)
target_compile_features(shoulder_notification_tests PRIVATE cxx_std_17)
target_compile_options(shoulder_notification_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(shoulder_notification_tests PRIVATE ../../app/src/main/cpp)
add_test(NAME shoulder_notification_tests COMMAND shoulder_notification_tests)
add_executable(pipboy_tests pipboy_tests.cpp
    ../../app/src/main/cpp/player/fo3-player-state.cpp
    ../../app/src/main/cpp/player/fo3-pipboy-player.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-data.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-session.cpp
    ../../app/src/main/cpp/pipboy/fo3-radio-runtime.cpp
    ../../app/src/main/cpp/player/fo3-container-state.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(pipboy_tests PRIVATE cxx_std_17)
target_compile_options(pipboy_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(pipboy_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(pipboy_tests PRIVATE ZLIB::ZLIB)
add_test(NAME pipboy_tests COMMAND pipboy_tests)
add_executable(pipboy_render_tests pipboy_render_tests.cpp
    ../../app/src/main/cpp/player/fo3-player-state.cpp
    ../../app/src/main/cpp/player/fo3-pipboy-player.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-data.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-session.cpp
    ../../app/src/main/cpp/pipboy/fo3-radio-runtime.cpp
    ../../app/src/main/cpp/player/fo3-container-state.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(pipboy_render_tests PRIVATE cxx_std_17)
target_compile_options(pipboy_render_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(pipboy_render_tests PRIVATE ../pipboy/mock ../render/mock ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(pipboy_render_tests PRIVATE ZLIB::ZLIB)
add_test(NAME pipboy_render_tests COMMAND pipboy_render_tests)
add_executable(vr_body_tests vr_body_tests.cpp)
target_compile_features(vr_body_tests PRIVATE cxx_std_17)
target_compile_options(vr_body_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(vr_body_tests PRIVATE ../../app/src/main/cpp)
add_test(NAME vr_body_tests COMMAND vr_body_tests)
add_executable(pipboy_data_tests pipboy_data_tests.cpp
    ../../app/src/main/cpp/player/fo3-player-state.cpp
    ../../app/src/main/cpp/player/fo3-container-state.cpp
    ../../app/src/main/cpp/player/fo3-pipboy-player.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-data.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-session.cpp
    ../../app/src/main/cpp/pipboy/fo3-radio-runtime.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(pipboy_data_tests PRIVATE cxx_std_17)
target_compile_options(pipboy_data_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(pipboy_data_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(pipboy_data_tests PRIVATE ZLIB::ZLIB)
add_test(NAME pipboy_data_tests COMMAND pipboy_data_tests)
foreach(target player_state_tests interaction_runtime_tests pipboy_tests pipboy_render_tests pipboy_data_tests)
    target_sources(${target} PRIVATE ../../app/src/main/cpp/weapons/fo3-weapon-data.cpp ../../app/src/main/cpp/weapons/fo3-weapon-player.cpp)
endforeach()
add_executable(weapon_tests weapon_tests.cpp
    ../../app/src/main/cpp/player/fo3-player-state.cpp
    ../../app/src/main/cpp/player/fo3-pipboy-player.cpp
    ../../app/src/main/cpp/player/fo3-container-state.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-data.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-session.cpp
    ../../app/src/main/cpp/pipboy/fo3-radio-runtime.cpp
    ../../app/src/main/cpp/weapons/fo3-weapon-data.cpp
    ../../app/src/main/cpp/weapons/fo3-weapon-player.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(weapon_tests PRIVATE cxx_std_17)
target_compile_options(weapon_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(weapon_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(weapon_tests PRIVATE ZLIB::ZLIB)
add_test(NAME weapon_tests COMMAND weapon_tests)

foreach(target player_state_tests interaction_runtime_tests pipboy_tests pipboy_render_tests pipboy_data_tests weapon_tests)
 target_sources(${target} PRIVATE ../../app/src/main/cpp/dialogue/fo3-dialogue-conditions.cpp ../../app/src/main/cpp/dialogue/fo3-dialogue-results.cpp ../../app/src/main/cpp/dialogue/fo3-dialogue-session.cpp)
endforeach()

add_executable(dialogue_tests dialogue_tests.cpp)
target_compile_features(dialogue_tests PRIVATE cxx_std_17)
target_compile_options(dialogue_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(dialogue_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(dialogue_tests PRIVATE ZLIB::ZLIB)
get_target_property(dialogue_shared player_state_tests SOURCES)
list(REMOVE_ITEM dialogue_shared player_state_tests.cpp)
target_sources(dialogue_tests PRIVATE ${dialogue_shared})
add_test(NAME dialogue_tests COMMAND dialogue_tests)

add_executable(npc_package_runtime_tests npc_package_runtime_tests.cpp ${dialogue_shared}
    ../../app/src/main/cpp/npc/fo3-npc.cpp)
target_compile_features(npc_package_runtime_tests PRIVATE cxx_std_17)
target_compile_options(npc_package_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(npc_package_runtime_tests PRIVATE ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(npc_package_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_package_runtime_tests COMMAND npc_package_runtime_tests)
