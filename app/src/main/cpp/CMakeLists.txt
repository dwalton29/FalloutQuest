cmake_minimum_required(VERSION 3.22.1)
project(falloutquest LANGUAGES CXX)

add_library(falloutquest SHARED native-lib.cpp)

find_library(log-lib log)
target_link_libraries(falloutquest ${log-lib})
