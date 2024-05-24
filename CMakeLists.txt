# CMake setup

cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0048 NEW)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)

project(asdf-cxx VERSION 8.0.0 LANGUAGES CXX)
set(PROJECT_DESCRIPTION
  "asdf-cxx (Advanced Scientific Data Format), C++ implementation")
set(PROJECT_URL "https://github.com/eschnett/asdf-cxx")

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules")

# Dependencies

find_package(blosc)
if(BLOSC_FOUND)
  include_directories(${BLOSC_INCLUDE_DIR})
  set(LIBS ${LIBS} ${BLOSC_LIBRARIES})
  set(HAVE_BLOSC 1)
else()
  set(HAVE_BLOSC 0)
endif()

find_package(blosc2)
if(BLOSC2_FOUND)
  include_directories(${BLOSC2_INCLUDE_DIR})
  set(LIBS ${LIBS} ${BLOSC2_LIBRARIES})
  set(HAVE_BLOSC2 1)
else()
  set(HAVE_BLOSC2 0)
endif()

find_package(BZip2)
if(BZIP2_FOUND)
  include_directories(${BZIP2_INCLUDE_DIR})
  set(LIBS ${LIBS} ${BZIP2_LIBRARIES})
  set(HAVE_BZIP2 1)
else()
  set(HAVE_BZIP2 0)
endif()

find_package(liblz4)
if(LIBLZ4_FOUND)
  include_directories(${LIBLZ4_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBLZ4_LIBRARIES})
  set(HAVE_LIBLZ4 1)
else()
  set(HAVE_LIBLZ4 0)
endif()

find_package(libzstd)
if(LIBZSTD_FOUND)
  include_directories(${LIBZSTD_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBZSTD_LIBRARIES})
  set(HAVE_LIBZSTD 1)
else()
  set(HAVE_LIBZSTD 0)
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

find_package(ZLIB)
if(ZLIB_FOUND)
  include_directories(${ZLIB_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${ZLIB_LIBRARIES})
  set(HAVE_ZLIB 1)
else()
  set(HAVE_ZLIB 0)
endif()

#TODO set(Python_ADDITIONAL_VERSIONS 3.0)
#TODO # This sets PYTHONINTERP_FOUND
#TODO find_package(PythonInterp)
#TODO # This sets PYTHONLIBS_FOUND
#TODO find_package(PythonLibs)
#TODO if(PYTHONINTERP_FOUND AND PYTHONLIBS_FOUND)
#TODO   include_directories(${PYTHON_INCLUDE_DIRS})
#TODO endif()

if(PYTHONINTERP_FOUND AND PYTHONLIBS_FOUND)
  find_package(SWIG)
endif()
if(SWIG_FOUND)
  include(${SWIG_USE_FILE})
endif()

# Main project

string(REPLACE "-DNDEBUG" ""
  CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "-DNDEBUG" ""
  CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

include(CheckCXXSourceCompiles)
  check_cxx_source_compiles(
    "
      int main() {
        _Float16 f16;
      }
    "
    ASDF_HAVE_FLOAT16)
  check_cxx_source_compiles(
    "
      int main() {
        __int128 i128;
      }
    "
    ASDF_HAVE_INT128)

configure_file(
  "${PROJECT_SOURCE_DIR}/include/asdf/config.hxx.in"
  "${PROJECT_BINARY_DIR}/include/asdf/config.hxx"
  )

include_directories("${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include")

set(ASDF_HEADERS
  include/asdf/asdf.hxx
  include/asdf/byteorder.hxx
  include/asdf/datatype.hxx
  include/asdf/entry.hxx
  include/asdf/io.hxx
  include/asdf/memoized.hxx
  include/asdf/ndarray.hxx
  include/asdf/reference.hxx
  include/asdf/stl.hxx
  include/asdf/table.hxx
)
set(ASDF_SOURCES
  src/asdf.cxx
  src/byteorder.cxx
  src/config.cxx
  src/datatype.cxx
  src/entry.cxx
  src/io.cxx
  src/ndarray.cxx
  src/reference.cxx
  src/table.cxx
)

add_library(asdf-cxx ${ASDF_HEADERS} ${ASDF_SOURCES})
set_property(TARGET asdf-cxx PROPERTY POSITION_INDEPENDENT_CODE TRUE)
target_compile_features(asdf-cxx PUBLIC cxx_std_17)
target_include_directories(asdf-cxx PUBLIC include/asdf)
target_link_libraries(asdf-cxx ${LIBS})

add_executable(asdf-copy utils/copy.cxx)
target_link_libraries(asdf-copy asdf-cxx ${LIBS})

add_executable(asdf-ls utils/ls.cxx)
target_link_libraries(asdf-ls asdf-cxx ${LIBS})

add_executable(asdf-demo demo/demo.cxx)
target_link_libraries(asdf-demo asdf-cxx ${LIBS})

add_executable(asdf-demo-compression demo/demo-compression.cxx)
target_link_libraries(asdf-demo-compression asdf-cxx ${LIBS})

add_executable(asdf-demo-external demo/demo-external.cxx)
target_link_libraries(asdf-demo-external asdf-cxx ${LIBS})

add_executable(asdf-demo-large demo/demo-large.cxx)
target_link_libraries(asdf-demo-large asdf-cxx ${LIBS})

add_executable(asdf-demo-nonstandard demo/demo-nonstandard.cxx)
target_link_libraries(asdf-demo-nonstandard asdf-cxx ${LIBS})

# SWIG bindings

if(PYTHONINTERP_FOUND AND PYTHONLIBS_FOUND AND SWIG_FOUND)
  set_source_files_properties(asdf.i PROPERTIES CPLUSPLUS ON)
  swig_add_library(asdf LANGUAGE python SOURCES asdf.i)
  swig_link_libraries(asdf asdf-cxx ${LIBS} ${PYTHON_LIBRARIES})
endif()

# Tests

enable_testing()
add_test(NAME demo COMMAND ./asdf-demo)
add_test(NAME ls COMMAND ./asdf-ls demo.asdf)
add_test(NAME demo-nonstandard COMMAND ./asdf-demo-nonstandard)
add_test(NAME ls2 COMMAND ./asdf-ls nonstandard.asdf)
add_test(NAME copy COMMAND ./asdf-copy demo.asdf demo2.asdf)
add_test(NAME ls3 COMMAND ./asdf-ls demo2.asdf)
add_test(NAME compare-demo
  COMMAND ${CMAKE_SOURCE_DIR}/diff-commands.sh
  "./asdf-ls demo.asdf" "./asdf-ls demo2.asdf")
add_test(NAME external COMMAND ./asdf-demo-external)

# These tests are broken in Python 3:
# SWIG does not translate between numpy integer arrays and C++ std::vector
# if(PYTHONINTERP_FOUND AND PYTHONLIBS_FOUND AND SWIG_FOUND)
#   add_test(NAME demo-python
#     COMMAND
#     env "PYTHONPATH=${PROJECT_BINARY_DIR}:$ENV{PYTHONPATH}"
#     ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/asdf-demo-python.py")
#   add_test(NAME compare-demo-python
#     COMMAND ${CMAKE_SOURCE_DIR}/diff-commands.sh
#     "./asdf-ls demo.asdf" "./asdf-ls demo-python.asdf")
#   add_test(NAME demo-external-python
#     COMMAND
#     env "PYTHONPATH=${PROJECT_BINARY_DIR}:$ENV{PYTHONPATH}"
#     ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/asdf-demo-external-python.py")
#   add_test(NAME compare-demo-external-python
#     COMMAND ${CMAKE_SOURCE_DIR}/diff-commands.sh
#     "./asdf-ls demo-external.asdf" "./asdf-ls demo-external-python.asdf")
# endif()

# Coverage

# See <https://github.com/codecov/example-cpp11-cmake>
option(CODE_COVERAGE "Enable coverage reporting" OFF)
if(CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  foreach(target asdf-cxx asdf-copy asdf-ls asdf-demo asdf-demo-compression asdf-demo-external asdf-demo-large asdf-demo-nonstandard)
    # Add required flags (GCC & LLVM/Clang)
    target_compile_options(${target} INTERFACE
      -O0        # no optimization
      -g         # generate debug info
      --coverage # sets all required flags
    )
    target_link_options(${target} INTERFACE --coverage)
  endforeach()
endif()

# Install

install(FILES ${ASDF_HEADERS} DESTINATION include/asdf)
install(FILES "${PROJECT_BINARY_DIR}/include/asdf/config.hxx" DESTINATION include/asdf)
install(TARGETS asdf-cxx DESTINATION lib)
install(TARGETS asdf-copy asdf-demo asdf-demo-external asdf-demo-large asdf-demo-compression asdf-ls
  DESTINATION bin)
if(PYTHONINTERP_FOUND AND PYTHONLIBS_FOUND AND SWIG_FOUND)
  install(PROGRAMS asdf-demo-python.py asdf-demo-external-python.py
    DESTINATION bin)
endif()

# `bz2` is also required
set(PKG_CONFIG_REQUIRES "yaml-cpp")
set(PKG_CONFIG_INCLUDEDIR "\${prefix}/include")
set(PKG_CONFIG_LIBDIR "\${prefix}/lib")
set(PKG_CONFIG_CFLAGS "-I\${includedir}")
set(PKG_CONFIG_LIBS "-L\${libdir} -lasdf-cxx -lyaml-cpp")
if(BLOSC_FOUND)
  set(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} blosc")
  set(PKG_CONFIG_LIBS "${PKG_CONFIG_LIBS} -lblosc")
endif()
if(BLOSC2_FOUND)
  set(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} blosc2")
  set(PKG_CONFIG_LIBS "${PKG_CONFIG_LIBS} -lblosc2")
endif()
if(BZip2_FOUND)
  # set(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} bz2")
  set(PKG_CONFIG_LIBS "${PKG_CONFIG_LIBS} -lbz2")
endif()
if(LIBLZ4_FOUND)
  set(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} lz4")
  set(PKG_CONFIG_LIBS "${PKG_CONFIG_LIBS} -llz4")
endif()
if(LIBZSTD_FOUND)
  set(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} libzstd")
  set(PKG_CONFIG_LIBS "${PKG_CONFIG_LIBS} -lzstd")
endif()
if(OPENSSL_FOUND)
  set(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} libssl")
  set(PKG_CONFIG_LIBS "${PKG_CONFIG_LIBS} -lssl")
endif()
if(ZLIB_FOUND)
  set(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} zlib")
  set(PKG_CONFIG_LIBS "${PKG_CONFIG_LIBS} -lz")
endif()

configure_file(
  "${PROJECT_SOURCE_DIR}/pkg-config.pc.in"
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}.pc"
)

install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc"
  DESTINATION lib/pkgconfig)
