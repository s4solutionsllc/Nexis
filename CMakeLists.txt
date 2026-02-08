cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(Stacer)

# Adding features(build cache + faster linkers) and reasonable defaults(Debug build by default)
include("${CMAKE_CURRENT_SOURCE_DIR}/shared/cmake/cxxbasics/CXXBasics.cmake")

# Setting a cleaner directory structure for the generated binaries
set(CMAKE_BINARY_DIR        "${CMAKE_BINARY_DIR}/output")
set(EXECUTABLE_OUTPUT_PATH  "${CMAKE_BINARY_DIR}/")
set(LIBRARY_OUTPUT_PATH     "${CMAKE_BINARY_DIR}/lib")

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SHARED_DIR   "${PROJECT_ROOT}/shared")

# Activating MOC and searching for the Qt6 dependencies
set(CMAKE_AUTOMOC ON)
find_package(Qt6 COMPONENTS Core Gui Widgets Charts Svg Concurrent REQUIRED)

# Setting the minimum C++ standard and passing the Qt-specific define
set(CMAKE_CXX_STANDARD           17)
set(CMAKE_CXX_EXTENSIONS         YES)
set(CMAKE_CXX_STANDARD_REQUIRED  YES)
add_definitions(-DQT_DEPRECATED_WARNINGS)

# Platform dispatch: include the platform-specific CMakeLists
if(APPLE)
  add_subdirectory(macos)
else()
  add_subdirectory(linux)
endif()
