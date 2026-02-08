cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(stacer-core)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/Info"
  "${CMAKE_CURRENT_SOURCE_DIR}/Tools"
  "${CMAKE_CURRENT_SOURCE_DIR}/Utils"
)

file(GLOB_RECURSE ${PROJECT_NAME}_srcs "${CMAKE_CURRENT_SOURCE_DIR}/**.cpp")

add_definitions(-DSTACERCORE_LIBRARY)

find_package(Qt6 COMPONENTS Core Network REQUIRED)

add_library(${PROJECT_NAME}  STATIC  ${${PROJECT_NAME}_srcs})
target_link_libraries(${PROJECT_NAME} Qt6::Core Qt6::Network)

# macOS-specific frameworks needed for system info APIs
if(APPLE)
  find_library(IOKIT_FRAMEWORK IOKit)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
  target_link_libraries(${PROJECT_NAME} ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
endif()
