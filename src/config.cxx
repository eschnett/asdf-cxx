#include <asdf/config.hxx>

#include <cstdlib>
#include <iostream>
#include <string>

namespace ASDF {

int asdf_standard_version_major() { return ASDF_STANDARD_VERSION_MAJOR; }
int asdf_standard_version_minor() { return ASDF_STANDARD_VERSION_MINOR; }
int asdf_standard_version_patch() { return ASDF_STANDARD_VERSION_PATCH; }

std::string asdf_standard_version() { return ASDF_STANDARD_VERSION; }

int asdf_cxx_version_major() { return ASDF_CXX_VERSION_MAJOR; }
int asdf_cxx_version_minor() { return ASDF_CXX_VERSION_MINOR; }
int asdf_cxx_version_patch() { return ASDF_CXX_VERSION_PATCH; }

std::string asdf_cxx_version() { return ASDF_CXX_VERSION; }

void check_version(const char *header_version) {
  if (header_version != asdf_cxx_version()) {
    std::cerr
        << "Version mismatch detected -- aborting.\n"
        << "  Include headers have version " << header_version << ",\n"
        << "  Linked library has version " << asdf_cxx_version() << ".\n"
        << "(The versions of the include headers and linked libraries differ.\n"
        << "This points to an improperly installed library or\n"
        << "improperly installed application.)\n";
    std::exit(EXIT_FAILURE);
  }
}

} // namespace ASDF
