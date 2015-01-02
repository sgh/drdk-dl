cmake_minimum_required(VERSION 2.8)
project( drdk-dl )

set(CMAKE_BUILD_TYPE "Release")

link_libraries( curl json-c )

add_executable( ${CMAKE_PROJECT_NAME} main.cpp )

