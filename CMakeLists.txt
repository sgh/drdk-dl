cmake_minimum_required(VERSION 3.2)
project( drdk-dl )

set(CMAKE_CXX_STANDARD 11)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "-Wall -Wextra")
endif()

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

set(SOURCES main.cpp)


if (NOT WIN32)
	find_package( PkgConfig )
	pkg_check_modules(PC_LIBJSONC REQUIRED json-c)
	pkg_check_modules(PC_LIBCURL REQUIRED libcurl)
	list(APPEND SOURCES curl.cpp json-c.cpp)
endif()

if (WIN32)
	list(APPEND SOURCES qt.cpp)
endif()


# I assume that libraries are in standard locations. This is just to make sure
# the build bails out if the libraries are not found.

file(GLOB_RECURSE ALL_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h *.hpp)

add_executable( drdk-dl ${SOURCES} ${ALL_HEADERS})

if (NOT WIN32)
	target_link_libraries(drdk-dl curl json-c )
endif()

if (WIN32)
	find_package(Qt5Core REQUIRED)
	find_package(Qt5Network REQUIRED)
	target_link_libraries(drdk-dl Qt5::Core Qt5::Network)
endif()
