find_package(PkgConfig)
pkg_check_modules(PC_YAML_CPP QUIET yaml-cpp)

find_path(YAML_CPP_INCLUDE_DIR yaml-cpp/yaml.h
          HINTS ${PC_YAML_CPP_INCLUDEDIR} ${PC_YAML_CPP_INCLUDE_DIRS})
find_library(YAML_CPP_LIBRARY NAMES yaml-cpp
          HINTS ${PC_YAML_CPP_LIBDIR} ${PC_YAML_CPP_LIBRARY_DIRS})

set(YAML_CPP_LIBRARIES ${YAML_CPP_LIBRARY})
set(YAML_CPP_INCLUDE_DIRS ${YAML_CPP_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(yaml-cpp DEFAULT_MSG YAML_CPP_LIBRARY YAML_CPP_INCLUDE_DIR)
mark_as_advanced(YAML_CPP_INCLUDE_DIR YAML_CPP_LIBRARY)
