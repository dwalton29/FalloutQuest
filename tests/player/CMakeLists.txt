cmake_minimum_required(VERSION 3.16)
project(falloutquest_player_tests LANGUAGES CXX)
enable_testing()
add_executable(paged_state_tests paged_state_tests.cpp)
target_compile_features(paged_state_tests PRIVATE cxx_std_17)
target_compile_options(paged_state_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(paged_state_tests PRIVATE ../../app/src/main/cpp)
add_test(NAME paged_state_tests COMMAND paged_state_tests)
add_executable(facial_tests facial_tests.cpp)
target_compile_features(facial_tests PRIVATE cxx_std_17)
target_compile_options(facial_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(facial_tests PRIVATE ../../app/src/main/cpp)
add_test(NAME facial_tests COMMAND facial_tests)
find_package(ZLIB REQUIRED)
add_executable(npc_statistics_tests npc_statistics_tests.cpp
    ../../app/src/main/cpp/weapons/fo3-weapon-data.cpp ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(npc_statistics_tests PRIVATE cxx_std_17)
target_compile_options(npc_statistics_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(npc_statistics_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(npc_statistics_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_statistics_tests COMMAND npc_statistics_tests)
# Vanilla progression maths is independently testable without OpenXR.
# Perk level/attribute prerequisites and skill-point allocation from PERK/GMST.
add_executable(perk_eligibility_tests perk_eligibility_tests.cpp)
target_compile_features(perk_eligibility_tests PRIVATE cxx_std_17)
target_compile_options(perk_eligibility_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(perk_eligibility_tests PRIVATE ../../app/src/main/cpp)
add_test(NAME perk_eligibility_tests COMMAND perk_eligibility_tests)

add_executable(experience_progression_tests experience_progression_tests.cpp)
target_compile_features(experience_progression_tests PRIVATE cxx_std_17)
target_compile_options(experience_progression_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(experience_progression_tests PRIVATE ../../app/src/main/cpp)
add_test(NAME experience_progression_tests COMMAND experience_progression_tests)

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
target_compile_options(weapon_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
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
add_executable(event_script_tests event_script_tests.cpp ${dialogue_shared})
target_compile_features(event_script_tests PRIVATE cxx_std_17)
target_compile_options(event_script_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(event_script_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(event_script_tests PRIVATE ZLIB::ZLIB)
add_test(NAME event_script_tests COMMAND event_script_tests)

add_executable(transaction_benchmark EXCLUDE_FROM_ALL ../../tools/performance/benchmark_transactions.cpp ${dialogue_shared})
target_compile_features(transaction_benchmark PRIVATE cxx_std_17)
target_include_directories(transaction_benchmark PRIVATE ../../app/src/main/cpp)
target_link_libraries(transaction_benchmark PRIVATE ZLIB::ZLIB)

add_executable(reference_mutation_plan_tests reference_mutation_plan_tests.cpp ${dialogue_shared})
target_compile_features(reference_mutation_plan_tests PRIVATE cxx_std_17)
target_compile_options(reference_mutation_plan_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(reference_mutation_plan_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(reference_mutation_plan_tests PRIVATE ZLIB::ZLIB)
add_test(NAME reference_mutation_plan_tests COMMAND reference_mutation_plan_tests)

add_executable(script_transaction_tests script_transaction_tests.cpp ${dialogue_shared}
    ../../app/src/main/cpp/scripting/fo3-script-master-match.cpp)
target_compile_features(script_transaction_tests PRIVATE cxx_std_17)
target_compile_options(script_transaction_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(script_transaction_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(script_transaction_tests PRIVATE ZLIB::ZLIB)
add_test(NAME script_transaction_tests COMMAND script_transaction_tests)
add_executable(script_master_match_tests script_master_match_tests.cpp
    ../../app/src/main/cpp/scripting/fo3-script-master-match.cpp
    ../../app/src/main/cpp/world/fo3-resident-authored-admission.cpp
    ../../app/src/main/cpp/pipboy/fo3-pipboy-data.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(script_master_match_tests PRIVATE cxx_std_17)
target_compile_options(script_master_match_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(script_master_match_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(script_master_match_tests PRIVATE ZLIB::ZLIB)
add_test(NAME script_master_match_tests COMMAND script_master_match_tests)




# Authored NPC-to-NPC ambient HELLO selector, without executing quest
# results or substituting the player as the conversation partner.
add_executable(npc_conversation_tests npc_conversation_tests.cpp
    ../../app/src/main/cpp/npc/fo3-npc-conversation.cpp
    ../../app/src/main/cpp/dialogue/fo3-dialogue-conditions.cpp
    ${dialogue_shared})
target_compile_features(npc_conversation_tests PRIVATE cxx_std_17)
target_compile_options(npc_conversation_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(npc_conversation_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(npc_conversation_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_conversation_tests COMMAND npc_conversation_tests)

add_executable(npc_conversation_runtime_tests npc_conversation_runtime_tests.cpp
    ${dialogue_shared}
    ../../app/src/main/cpp/npc/fo3-npc.cpp
    ../../app/src/main/cpp/npc/fo3-actor-animation.cpp)
target_compile_features(npc_conversation_runtime_tests PRIVATE cxx_std_17)
target_compile_options(npc_conversation_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-unused-function)
target_include_directories(npc_conversation_runtime_tests PRIVATE ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(npc_conversation_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_conversation_runtime_tests COMMAND npc_conversation_runtime_tests)

add_executable(npc_package_runtime_tests npc_package_runtime_tests.cpp ${dialogue_shared}
    ../../app/src/main/cpp/npc/fo3-actor-animation.cpp
    ../../app/src/main/cpp/npc/fo3-npc.cpp)
target_compile_features(npc_package_runtime_tests PRIVATE cxx_std_17)
target_compile_options(npc_package_runtime_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(npc_package_runtime_tests PRIVATE ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(npc_package_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_package_runtime_tests COMMAND npc_package_runtime_tests)
add_executable(npc_crowd_runtime_tests npc_crowd_runtime_tests.cpp ${dialogue_shared}
    ../../app/src/main/cpp/npc/fo3-actor-animation.cpp ../../app/src/main/cpp/npc/fo3-npc.cpp)
target_compile_features(npc_crowd_runtime_tests PRIVATE cxx_std_17)
target_compile_options(npc_crowd_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(npc_crowd_runtime_tests PRIVATE ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(npc_crowd_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_crowd_runtime_tests COMMAND npc_crowd_runtime_tests)

# Full production package executor using a synthetic canonical ESM
# with two XTEL doors; checks a real host-side actor handoff and lock denial.
# Pure tracked-unloaded scheduler uses authored-format XTEL fixtures, with
# game-time schedule/door/condition gating and no render or player transition.
add_executable(npc_unloaded_runtime_tests npc_unloaded_runtime_tests.cpp
    ../../app/src/main/cpp/npc/fo3-unloaded-ai.cpp
    ../../app/src/main/cpp/data/fo3-xtel-index.cpp
    ../../app/src/main/cpp/data/fo3-esm-reader.cpp)
target_compile_features(npc_unloaded_runtime_tests PRIVATE cxx_std_17)
target_compile_options(npc_unloaded_runtime_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(npc_unloaded_runtime_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(npc_unloaded_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_unloaded_runtime_tests COMMAND npc_unloaded_runtime_tests)

add_executable(npc_xtel_runtime_tests npc_xtel_runtime_tests.cpp ${dialogue_shared}
    ../../app/src/main/cpp/npc/fo3-npc.cpp
    ../../app/src/main/cpp/npc/fo3-actor-animation.cpp
    ../../app/src/main/cpp/data/fo3-xtel-index.cpp)
target_compile_features(npc_xtel_runtime_tests PRIVATE cxx_std_17)
target_compile_options(npc_xtel_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-unused-function)
target_include_directories(npc_xtel_runtime_tests PRIVATE ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(npc_xtel_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_xtel_runtime_tests COMMAND npc_xtel_runtime_tests)

add_executable(npc_combat_tests npc_combat_tests.cpp ${dialogue_shared})
target_compile_features(npc_combat_tests PRIVATE cxx_std_17)
target_compile_options(npc_combat_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(npc_combat_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(npc_combat_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_combat_tests COMMAND npc_combat_tests)

add_executable(npc_combat_runtime_tests npc_combat_runtime_tests.cpp ${dialogue_shared}
    ../../app/src/main/cpp/npc/fo3-npc.cpp ../../app/src/main/cpp/npc/fo3-actor-animation.cpp)
target_compile_features(npc_combat_runtime_tests PRIVATE cxx_std_17)
target_compile_options(npc_combat_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(npc_combat_runtime_tests PRIVATE ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(npc_combat_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_combat_runtime_tests COMMAND npc_combat_runtime_tests)

add_executable(npc_door_runtime_tests npc_door_runtime_tests.cpp ${dialogue_shared}
    ../../app/src/main/cpp/npc/fo3-actor-animation.cpp
    ../../app/src/main/cpp/npc/fo3-npc.cpp)
target_compile_features(npc_door_runtime_tests PRIVATE cxx_std_17)
target_compile_options(npc_door_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(npc_door_runtime_tests PRIVATE ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(npc_door_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_door_runtime_tests COMMAND npc_door_runtime_tests)

add_executable(dialogue_runtime_tests dialogue_runtime_tests.cpp ${dialogue_shared}
    ../../app/src/main/cpp/npc/fo3-actor-animation.cpp
    ../../app/src/main/cpp/npc/fo3-npc.cpp)
target_compile_features(dialogue_runtime_tests PRIVATE cxx_std_17)
target_compile_options(dialogue_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-unused-function)
target_include_directories(dialogue_runtime_tests PRIVATE ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(dialogue_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME dialogue_runtime_tests COMMAND dialogue_runtime_tests)

add_executable(npc_furniture_runtime_tests npc_furniture_runtime_tests.cpp ${dialogue_shared}
    ../../app/src/main/cpp/npc/fo3-npc.cpp ../../app/src/main/cpp/npc/fo3-actor-animation.cpp)
target_compile_features(npc_furniture_runtime_tests PRIVATE cxx_std_17)
target_compile_options(npc_furniture_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-unused-function)
target_include_directories(npc_furniture_runtime_tests PRIVATE ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data ../../app/src/main/cpp/npc)
target_link_libraries(npc_furniture_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME npc_furniture_runtime_tests COMMAND npc_furniture_runtime_tests)

# HUD reveal timing is independent of OpenGL and uses canonical health samples.
add_executable(health_bar_state_tests health_bar_state_tests.cpp)
target_compile_features(health_bar_state_tests PRIVATE cxx_std_17)
target_compile_options(health_bar_state_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(health_bar_state_tests PRIVATE ../../app/src/main/cpp)
add_test(NAME health_bar_state_tests COMMAND health_bar_state_tests)

# Every consumer of the quest Player API links the shared script implementation.
get_property(player_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS player_targets)
 get_target_property(sources ${target} SOURCES)
 if("../../app/src/main/cpp/player/fo3-pipboy-player.cpp" IN_LIST sources AND NOT "../../app/src/main/cpp/dialogue/fo3-dialogue-results.cpp" IN_LIST sources)
  target_sources(${target} PRIVATE ../../app/src/main/cpp/dialogue/fo3-dialogue-conditions.cpp ../../app/src/main/cpp/dialogue/fo3-dialogue-results.cpp)
 endif()
endforeach()

add_executable(ownership_tests ownership_tests.cpp ${dialogue_shared} ../../app/src/main/cpp/audio/fo3-audio-catalog.cpp)
target_compile_features(ownership_tests PRIVATE cxx_std_17)
target_compile_options(ownership_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(ownership_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(ownership_tests PRIVATE ZLIB::ZLIB)
add_test(NAME ownership_tests COMMAND ownership_tests)

add_executable(lockpick_tests lockpick_tests.cpp ${dialogue_shared} ../../app/src/main/cpp/audio/fo3-audio-catalog.cpp)
target_compile_features(lockpick_tests PRIVATE cxx_std_17)
target_compile_options(lockpick_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(lockpick_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(lockpick_tests PRIVATE ZLIB::ZLIB)
add_test(NAME lockpick_tests COMMAND lockpick_tests)

add_executable(lockpick_runtime_tests lockpick_runtime_tests.cpp ${dialogue_shared})
target_compile_features(lockpick_runtime_tests PRIVATE cxx_std_17)
target_compile_options(lockpick_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(lockpick_runtime_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(lockpick_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME lockpick_runtime_tests COMMAND lockpick_runtime_tests)

add_executable(barter_tests barter_tests.cpp ${dialogue_shared})
target_compile_features(barter_tests PRIVATE cxx_std_17)
target_compile_options(barter_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(barter_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(barter_tests PRIVATE ZLIB::ZLIB)
add_test(NAME barter_tests COMMAND barter_tests)

add_executable(mq01_progression_tests mq01_progression_tests.cpp ${dialogue_shared})
target_compile_features(mq01_progression_tests PRIVATE cxx_std_17)
target_compile_options(mq01_progression_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(mq01_progression_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(mq01_progression_tests PRIVATE ZLIB::ZLIB)
add_test(NAME mq01_progression_tests COMMAND mq01_progression_tests)
set_tests_properties(mq01_progression_tests PROPERTIES SKIP_RETURN_CODE 77)

add_executable(ms11_completion_tests ms11_completion_tests.cpp ${dialogue_shared})
target_compile_features(ms11_completion_tests PRIVATE cxx_std_17)
target_compile_options(ms11_completion_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(ms11_completion_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(ms11_completion_tests PRIVATE ZLIB::ZLIB)
add_test(NAME ms11_completion_tests COMMAND ms11_completion_tests)

add_executable(pause_debug_tests pause_debug_tests.cpp ${dialogue_shared})
target_compile_features(pause_debug_tests PRIVATE cxx_std_17)
target_compile_options(pause_debug_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(pause_debug_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(pause_debug_tests PRIVATE ZLIB::ZLIB)
add_test(NAME pause_debug_tests COMMAND pause_debug_tests)

add_executable(pause_render_tests ../render/pause_render_tests.cpp)
target_compile_features(pause_render_tests PRIVATE cxx_std_17)
target_compile_options(pause_render_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(pause_render_tests PRIVATE ../pipboy/mock ../render/mock ../nif ../../app/src/main/cpp ../../app/src/main/cpp/data)
add_test(NAME pause_render_tests COMMAND pause_render_tests)

# Opt-in original-data microbenchmark; excluded from routine CI builds.
get_target_property(loading_catalogue_sources pause_debug_tests SOURCES)
list(REMOVE_ITEM loading_catalogue_sources pause_debug_tests.cpp)
add_executable(loading_catalog_benchmark EXCLUDE_FROM_ALL
    ../../tools/loading/benchmark_catalogue.cpp ${loading_catalogue_sources})
target_compile_features(loading_catalog_benchmark PRIVATE cxx_std_17)
target_include_directories(loading_catalog_benchmark PRIVATE ../../app/src/main/cpp)
target_link_libraries(loading_catalog_benchmark PRIVATE ZLIB::ZLIB)

add_executable(terminal_tests terminal_tests.cpp ${dialogue_shared} ../../app/src/main/cpp/scripting/fo3-script-master-match.cpp)
target_compile_features(terminal_tests PRIVATE cxx_std_17)
target_compile_options(terminal_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(terminal_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(terminal_tests PRIVATE ZLIB::ZLIB)
add_test(NAME terminal_tests COMMAND terminal_tests)
add_executable(terminal_runtime_tests terminal_runtime_tests.cpp ${dialogue_shared})
target_compile_features(terminal_runtime_tests PRIVATE cxx_std_17)
target_compile_options(terminal_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(terminal_runtime_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(terminal_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME terminal_runtime_tests COMMAND terminal_runtime_tests)

add_executable(world_script_tests world_script_tests.cpp ${dialogue_shared} ../../app/src/main/cpp/world/fo3-worldspace-runtime.cpp)
target_compile_features(world_script_tests PRIVATE cxx_std_17)
target_compile_options(world_script_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(world_script_tests PRIVATE ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(world_script_tests PRIVATE ZLIB::ZLIB)
add_test(NAME world_script_tests COMMAND world_script_tests)
add_executable(world_script_runtime_tests world_script_runtime_tests.cpp ${dialogue_shared} ../../app/src/main/cpp/npc/fo3-actor-animation.cpp)
target_compile_features(world_script_runtime_tests PRIVATE cxx_std_17)
target_compile_options(world_script_runtime_tests PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(world_script_runtime_tests PRIVATE ../../app/src/main/cpp ../../app/src/main/cpp/data)
target_link_libraries(world_script_runtime_tests PRIVATE ZLIB::ZLIB)
add_test(NAME world_script_runtime_tests COMMAND world_script_runtime_tests)
add_executable(wasteland_npc_tests wasteland_npc_tests.cpp ${dialogue_shared})
target_compile_features(wasteland_npc_tests PRIVATE cxx_std_17)
target_compile_options(wasteland_npc_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(wasteland_npc_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(wasteland_npc_tests PRIVATE ZLIB::ZLIB)
add_test(NAME wasteland_npc_tests COMMAND wasteland_npc_tests)
add_executable(spacewarp_history_tests spacewarp_history_tests.cpp)
target_compile_features(spacewarp_history_tests PRIVATE cxx_std_17)
target_compile_options(spacewarp_history_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(spacewarp_history_tests PRIVATE ../../app/src/main/cpp)
add_test(NAME spacewarp_history_tests COMMAND spacewarp_history_tests)

add_executable(menu_layout_tests menu_layout_tests.cpp ../../app/src/main/cpp/data/fo3-bsa-archive.cpp)
target_compile_features(menu_layout_tests PRIVATE cxx_std_17)
target_compile_options(menu_layout_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(menu_layout_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(menu_layout_tests PRIVATE ZLIB::ZLIB)
add_test(NAME menu_layout_tests COMMAND menu_layout_tests)
target_sources(pipboy_render_tests PRIVATE ../../app/src/main/cpp/data/fo3-bsa-archive.cpp)

add_executable(ms03_progression_tests ms03_progression_tests.cpp ${dialogue_shared})
target_compile_features(ms03_progression_tests PRIVATE cxx_std_17)
target_compile_options(ms03_progression_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(ms03_progression_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(ms03_progression_tests PRIVATE ZLIB::ZLIB)
add_test(NAME ms03_progression_tests COMMAND ms03_progression_tests)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FalloutPhysics.cmake)
add_executable(npc_ragdoll_runtime_tests npc_ragdoll_runtime_tests.cpp ${dialogue_shared} ../../app/src/main/cpp/npc/fo3-actor-animation.cpp)
target_compile_features(npc_ragdoll_runtime_tests PRIVATE cxx_std_17)
target_compile_options(npc_ragdoll_runtime_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror -Wno-misleading-indentation)
target_include_directories(npc_ragdoll_runtime_tests PRIVATE ../../app/src/main/cpp)
target_link_libraries(npc_ragdoll_runtime_tests PRIVATE ZLIB::ZLIB fq_physics)
add_test(NAME npc_ragdoll_runtime_tests COMMAND npc_ragdoll_runtime_tests)
