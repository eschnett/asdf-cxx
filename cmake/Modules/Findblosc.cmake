find_package(PkgConfig)
pkg_check_modules(PC_BLOSC QUIET blosc)

find_path(BLOSC_INCLUDE_DIR blosc.h
          HINTS ${PC_BLOSC_INCLUDEDIR} ${PC_BLOSC_INCLUDE_DIRS})
find_library(BLOSC_LIBRARY NAMES blosc
          HINTS ${PC_BLOSC_LIBDIR} ${PC_BLOSC_LIBRARY_DIRS})

set(BLOSC_LIBRARIES ${BLOSC_LIBRARY})
set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(blosc DEFAULT_MSG BLOSC_LIBRARY BLOSC_INCLUDE_DIR)
mark_as_advanced(BLOSC_INCLUDE_DIR BLOSC_LIBRARY)
