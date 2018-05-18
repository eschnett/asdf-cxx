# CMake setup

cmake_minimum_required(VERSION 3.1)
cmake_policy(SET CMP0048 NEW)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)

project(asdf-cxx VERSION 0.0.0 LANGUAGES CXX)
set(PROJECT_DESCRIPTION
  "asdf-cxx (Advanced Scientific Data Format), C++ implementation")
set(PROJECT_URL "https://github.com/eschnett/asdf-cxx")

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules")

# Dependencies

find_package(BZip2)
if(BZIP2_FOUND)
  include_directories(${BZIP2_INCLUDE_DIR})
  set(LIBS ${LIBS} ${BZIP2_LIBRARIES})
  set(HAVE_BZIP2 1)
else()
  set(HAVE_BZIP2 0)
endif()

find_package(OpenSSL)
if(OPENSSL_FOUND)
  include_directories(${OPENSSL_INCLUDE_DIR})
  set(LIBS ${LIBS} ${OPENSSL_CRYPTO_LIBRARY})
  set(HAVE_OPENSSL 1)
else()
  set(HAVE_OPENSSL 0)
endif()

# yaml-cpp: A YAML parser and emitter in C++
find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIR})
set(LIBS ${LIBS} ${YAML_CPP_LIBRARIES})

# message("Finding yaml-cpp...")
# find_package(yaml-cpp REQUIRED)
# if(NOT YAMLCPP_FOUND)
#   message(SEND_ERROR "yaml-cpp not found")
# endif()
# message("Found yaml-cpp in ${YAMLCPP_INCLUDE_DIR}")
# include_directories(${YAMLCPP_INCLUDE_DIR})
# set(LIBS ${LIBS} ${YAMLCPP_LIBRARY})

find_package(ZLIB)
if(ZLIB_FOUND)
  include_directories(${ZLIB_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${ZLIB_LIBRARIES})
  set(HAVE_ZLIB 1)
else()
  set(HAVE_ZLIB 0)
endif()

# Main project

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

string(REPLACE "-DNDEBUG" ""
  CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

configure_file(
  "${PROJECT_SOURCE_DIR}/asdf_config.hpp.in"
  "${PROJECT_BINARY_DIR}/asdf_config.hpp"
  )

include_directories("${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}")

set(ASDF_SOURCES
  asdf.cpp byteorder.cpp config.cpp datatype.cpp group.cpp io.cpp
  ndarray.cpp table.cpp)

add_library(asdf-cxx ${ASDF_SOURCES})
set_property(TARGET asdf-cxx PROPERTY POSITION_INDEPENDENT_CODE TRUE)

add_executable(asdf-copy copy.cpp)
target_link_libraries(asdf-copy asdf-cxx ${LIBS})

add_executable(asdf-demo demo.cpp)
target_link_libraries(asdf-demo asdf-cxx ${LIBS})

add_executable(asdf-demo-large demo-large.cpp)
target_link_libraries(asdf-demo-large asdf-cxx ${LIBS})

add_executable(asdf-ls ls.cpp)
target_link_libraries(asdf-ls asdf-cxx ${LIBS})

# Tests

enable_testing()
add_test(NAME demo COMMAND ./asdf-demo)
add_test(NAME ls COMMAND ./asdf-ls demo.asdf)
add_test(NAME copy COMMAND ./asdf-copy demo.asdf demo2.asdf)
add_test(NAME ls2 COMMAND ./asdf-ls demo2.asdf)

# Coverage

# We don't want this to run on every build
option(COVERALLS "Generate coveralls data" OFF)
if (COVERALLS)
  include(Coveralls)
  coveralls_turn_on_coverage()
  set(COVERAGE_SRCS ${ASDF_SOURCES} copy.cpp demo.cpp ls.cpp)
  coveralls_setup("${COVERAGE_SRCS}" ON)
endif()

# Install

install(FILES asdf.hpp DESTINATION include)
install(FILES "${PROJECT_BINARY_DIR}/asdf_config.hpp" DESTINATION include)
install(TARGETS asdf-cxx DESTINATION lib)
install(TARGETS asdf-copy asdf-demo asdf-demo-large asdf-ls DESTINATION bin)

set(PKG_CONFIG_REQUIRES "yaml-cpp")
set(PKG_CONFIG_INCLUDEDIR "\${prefix}/include")
set(PKG_CONFIG_LIBDIR "\${prefix}/lib")
set(PKG_CONFIG_CFLAGS "-I\${includedir}")
set(PKG_CONFIG_LIBS "-L\${libdir} -lasdf-cxx")

configure_file(
  "${PROJECT_SOURCE_DIR}/pkg-config.pc.in"
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}.pc"
)

install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc"
  DESTINATION lib/pkgconfig)
