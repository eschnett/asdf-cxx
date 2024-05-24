find_package(PkgConfig)
pkg_check_modules(PC_BLOSC2 QUIET blosc2)

find_path(BLOSC2_INCLUDE_DIR blosc2.h
          HINTS ${PC_BLOSC2_INCLUDEDIR} ${PC_BLOSC2_INCLUDE_DIRS})
find_library(BLOSC2_LIBRARY NAMES blosc2
          HINTS ${PC_BLOSC2_LIBDIR} ${PC_BLOSC2_LIBRARY_DIRS})

set(BLOSC2_LIBRARIES ${BLOSC2_LIBRARY})
set(BLOSC2_INCLUDE_DIRS ${BLOSC2_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(blosc2 DEFAULT_MSG BLOSC2_LIBRARY BLOSC2_INCLUDE_DIR)
mark_as_advanced(BLOSC2_INCLUDE_DIR BLOSC2_LIBRARY)
