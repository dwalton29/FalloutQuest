# Q14.5 link bridge: q1450's LAND renderer is a separate translation unit.
# CMakeLists creates the falloutquest target later, so defer adding the tiny
# external state bridge until directory configuration is complete.
cmake_language(DEFER CALL target_sources falloutquest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fo3-fog-ab-q1450.cpp")

message(STATUS "Q14.5 LAND fog-state external bridge queued")
