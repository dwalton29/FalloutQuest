# Native Bink decoding; LGPL-only FFmpeg components, dynamically linked.
include(FetchContent)
include(ExternalProject)
if(DEFINED ENV{FQ_FFMPEG_SOURCE_DIR})
    set(FETCHCONTENT_SOURCE_DIR_FQ_FFMPEG "$ENV{FQ_FFMPEG_SOURCE_DIR}")
endif()
FetchContent_Declare(fq_ffmpeg
    GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
    GIT_TAG n7.1.1 GIT_SHALLOW TRUE SOURCE_SUBDIR unused)
FetchContent_MakeAvailable(fq_ffmpeg)
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY ${fq_ffmpeg_SOURCE_DIR}
    OUTPUT_VARIABLE fq_ffmpeg_commit OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT fq_ffmpeg_commit STREQUAL "db69d06eeeab4f46da15030a80d539efb4503ca8")
    message(FATAL_ERROR "FFmpeg must match the reviewed n7.1.1 commit")
endif()
find_program(FQ_MOVIE_MAKE NAMES gmake make REQUIRED)
set(fq_movie_prefix "${CMAKE_CURRENT_BINARY_DIR}/fq-movie-install")
file(MAKE_DIRECTORY "${fq_movie_prefix}/include")
set(fq_movie_config --prefix=${fq_movie_prefix} --disable-everything
    --disable-programs --disable-doc --disable-network --disable-autodetect
    --disable-asm --enable-pic --enable-shared --disable-static
    --disable-gpl --disable-nonfree --disable-version3
    --enable-avformat --enable-avcodec --enable-avutil --enable-swscale
    --disable-avfilter --disable-avdevice --disable-swresample --disable-postproc
    --enable-demuxer=bink --enable-decoder=bink,binkaudio_dct,binkaudio_rdft
    --enable-protocol=file)
if(ANDROID)
    if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR "Movie decoder currently supports Android arm64-v8a")
    endif()
    if(CMAKE_HOST_APPLE)
        set(fq_movie_host darwin-x86_64)
    else()
        set(fq_movie_host linux-x86_64)
    endif()
    set(fq_movie_bin "${ANDROID_NDK}/toolchains/llvm/prebuilt/${fq_movie_host}/bin")
    list(APPEND fq_movie_config --enable-cross-compile --target-os=android --arch=aarch64
        --cc=${fq_movie_bin}/aarch64-linux-android29-clang
        --cxx=${fq_movie_bin}/aarch64-linux-android29-clang++
        --ar=${fq_movie_bin}/llvm-ar --ranlib=${fq_movie_bin}/llvm-ranlib
        --strip=${fq_movie_bin}/llvm-strip)
endif()
set(fq_movie_outputs)
foreach(lib avformat avcodec swscale avutil)
    list(APPEND fq_movie_outputs "${fq_movie_prefix}/lib/lib${lib}.so")
endforeach()
ExternalProject_Add(fq_movie_build SOURCE_DIR ${fq_ffmpeg_SOURCE_DIR}
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/fq-movie-build"
    DOWNLOAD_COMMAND "" UPDATE_COMMAND ""
    CONFIGURE_COMMAND ${fq_ffmpeg_SOURCE_DIR}/configure ${fq_movie_config}
    BUILD_COMMAND ${FQ_MOVIE_MAKE} -j2
    INSTALL_COMMAND ${FQ_MOVIE_MAKE} install
    BUILD_BYPRODUCTS ${fq_movie_outputs}
    LOG_CONFIGURE TRUE LOG_BUILD TRUE LOG_INSTALL TRUE)
foreach(lib avformat avcodec swscale avutil)
    add_library(fq_${lib} SHARED IMPORTED GLOBAL)
    set_target_properties(fq_${lib} PROPERTIES
        IMPORTED_LOCATION "${fq_movie_prefix}/lib/lib${lib}.so"
        INTERFACE_INCLUDE_DIRECTORIES "${fq_movie_prefix}/include")
    add_dependencies(fq_${lib} fq_movie_build)
endforeach()
add_library(fq_movie_decoder STATIC ${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp/video/fo3-bink-decoder.cpp)
target_compile_features(fq_movie_decoder PUBLIC cxx_std_17)
set_target_properties(fq_movie_decoder PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(fq_movie_decoder PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp)
target_link_libraries(fq_movie_decoder PUBLIC fq_avformat fq_avcodec fq_swscale fq_avutil)
add_dependencies(fq_movie_decoder fq_movie_build)
