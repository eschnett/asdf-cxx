# CMake setup

cmake_minimum_required(VERSION 3.1)
cmake_policy(SET CMP0048 NEW)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)

project(ASDF VERSION 0.0.0 LANGUAGES CXX)
set(PROJECT_DESCRIPTION
  "ASDF (Advanced Scientific Data Format), C++ implementation")
set(PROJECT_URL "https://github.com/eschnett/ASDF")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies

# yaml-cpp: A YAML parser and emitter in C++
find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIR})
set(LIBS ${LIBS} ${YAML_CPP_LIBRARIES})

# Main project

string(REPLACE "-DNDEBUG" ""
  CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

configure_file(
  "${PROJECT_SOURCE_DIR}/config.hpp.in"
  "${PROJECT_BINARY_DIR}/config.hpp"
  )

include_directories("${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}")

add_library(ASDF asdf.cpp config.cpp)
set_property(TARGET ASDF PROPERTY POSITION_INDEPENDENT_CODE TRUE)

add_executable(asdf-demo demo.cpp)
target_link_libraries(asdf-demo ASDF ${LIBS})

add_executable(asdf-ls ls.cpp)
target_link_libraries(asdf-ls ASDF ${LIBS})
