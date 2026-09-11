cmake_minimum_required(VERSION 3.20)
project(Fallout3D3D9Logger LANGUAGES CXX)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Fallout 3 is 32-bit. Configure this project with -A Win32.")
endif()

add_library(d3d9 SHARED
    d3d9_logger.cpp
    d3d9.def
)

target_compile_features(d3d9 PRIVATE cxx_std_17)
target_link_libraries(d3d9 PRIVATE user32)

if(MSVC)
    # Keep the proxy self-contained. Fallout 3 predates the VC++ 2015+ runtime,
    # so depending on MSVCP140/VCRUNTIME140 can make the game fail at process
    # startup before our proxy gets a chance to run.
    set_property(TARGET d3d9 PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(d3d9 PRIVATE /W4 /EHsc /permissive-)
endif()

set_target_properties(d3d9 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "d3d9"
)
