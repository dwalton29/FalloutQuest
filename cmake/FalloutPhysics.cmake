# Bullet's CPU rigid-body solver only; immutable upstream commit, no demos/GPU.
include(FetchContent)
if(DEFINED ENV{FQ_BULLET_SOURCE_DIR})
    set(FETCHCONTENT_SOURCE_DIR_FQ_BULLET "$ENV{FQ_BULLET_SOURCE_DIR}")
endif()
FetchContent_Declare(fq_bullet
    GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
    GIT_TAG 3.25
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR unused)
FetchContent_MakeAvailable(fq_bullet)
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY ${fq_bullet_SOURCE_DIR}
    OUTPUT_VARIABLE fq_bullet_commit OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT fq_bullet_commit STREQUAL "2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5")
    message(FATAL_ERROR "Bullet source must match the reviewed 3.25 commit")
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BULLET_VERSION 3.25)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${fq_bullet_SOURCE_DIR}/src/LinearMath ${CMAKE_CURRENT_BINARY_DIR}/fq-linear EXCLUDE_FROM_ALL)
add_subdirectory(${fq_bullet_SOURCE_DIR}/src/BulletCollision ${CMAKE_CURRENT_BINARY_DIR}/fq-collision EXCLUDE_FROM_ALL)
add_subdirectory(${fq_bullet_SOURCE_DIR}/src/BulletDynamics ${CMAKE_CURRENT_BINARY_DIR}/fq-dynamics EXCLUDE_FROM_ALL)
foreach(lib LinearMath BulletCollision BulletDynamics)
    target_include_directories(${lib} PUBLIC ${fq_bullet_SOURCE_DIR}/src)
endforeach()
add_library(fq_physics STATIC ${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp/physics/fo3-rigid-world.cpp)
target_compile_features(fq_physics PUBLIC cxx_std_17)
target_include_directories(fq_physics PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp)
target_link_libraries(fq_physics PUBLIC BulletDynamics BulletCollision LinearMath)

foreach(lib LinearMath BulletCollision BulletDynamics fq_physics)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${lib} PRIVATE -O2)
    endif()
endforeach()
