#include <asdf/config.hxx>

#include <cstdlib>
#include <iostream>
#include <string>

namespace ASDF {

int version_major() { return ASDF_VERSION_MAJOR; }
int version_minor() { return ASDF_VERSION_MINOR; }
int version_patch() { return ASDF_VERSION_PATCH; }

std::string version() { return ASDF_VERSION; }

void check_version(const char *header_version) {
  if (header_version != version()) {
    std::cerr
        << "Version mismatch detected -- aborting.\n"
        << "  Include headers have version " << header_version << ",\n"
        << "  Linked library has version " << version() << ".\n"
        << "(The versions of the include headers and linked libraries differ.\n"
        << "This points to an improperly installed library or\n"
        << "improperly installed application.)\n";
    std::exit(EXIT_FAILURE);
  }
}

} // namespace ASDF
