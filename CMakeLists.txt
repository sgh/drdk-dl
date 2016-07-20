cmake_minimum_required(VERSION 2.8)
project( drdk-dl )

set(CMAKE_BUILD_TYPE "Release")

if (NOT WIN32)
	find_package( PkgConfig )
	pkg_check_modules(PC_LIBJSONC REQUIRED json-c)
	pkg_check_modules(PC_LIBCURL REQUIRED libcurl)
endif()

# I assume that libraries are in standard locations. This is just to make sure
# the build bails out if the libraries are not found.

link_libraries( curl json-c )

add_executable( ${CMAKE_PROJECT_NAME} main.cpp )
