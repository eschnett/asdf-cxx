find_package(PkgConfig)
pkg_check_modules(LIBLZ4 QUIET liblz4)

find_path(LIBLZ4_INCLUDE_DIR lz4.h
          HINTS ${PC_LIBLZ4_INCLUDEDIR} ${PC_LIBLZ4_INCLUDE_DIRS})
find_library(LIBLZ4_LIBRARY NAMES lz4
          HINTS ${PC_LIBLZ4_LIBDIR} ${PC_LIBLZ4_LIBRARY_DIRS})

set(LIBLZ4_LIBRARIES ${LIBLZ4_LIBRARY})
set(LIBLZ4_INCLUDE_DIRS ${LIBLZ4_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(liblz4 DEFAULT_MSG LIBLZ4_LIBRARY LIBLZ4_INCLUDE_DIR)
mark_as_advanced(LIBLZ4_INCLUDE_DIR LIBLZ4_LIBRARY)
