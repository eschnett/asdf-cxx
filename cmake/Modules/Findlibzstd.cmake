find_package(PkgConfig)
pkg_check_modules(PC_LIBZSTD QUIET libzstd)

find_path(LIBZSTD_INCLUDE_DIR zstd.h
          HINTS ${PC_LIBZSTD_INCLUDEDIR} ${PC_LIBZSTD_INCLUDE_DIRS})
find_library(LIBZSTD_LIBRARY NAMES zstd
          HINTS ${PC_LIBZSTD_LIBDIR} ${PC_LIBZSTD_LIBRARY_DIRS})

set(LIBZSTD_LIBRARIES ${LIBZSTD_LIBRARY})
set(LIBZSTD_INCLUDE_DIRS ${LIBZSTD_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libzstd DEFAULT_MSG LIBZSTD_LIBRARY LIBZSTD_INCLUDE_DIR)
mark_as_advanced(LIBZSTD_INCLUDE_DIR LIBZSTD_LIBRARY)
