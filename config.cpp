#include <config.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

namespace ASDF {

int versionMajor() { return ASDF_VERSION_MAJOR; }
int versionMinor() { return ASDF_VERSION_MINOR; }
int versionPatch() { return ASDF_VERSION_PATCH; }

std::string Version() { return ASDF_VERSION; }

void checkVersion(const char *header_version) {
  if (header_version != Version()) {
    std::cerr
        << "Version mismatch detected -- aborting.\n"
        << "  Include headers have version " << header_version << ",\n"
        << "  Linked library has version " << Version() << ".\n"
        << "(The versions of the include headers and linked libraries differ.\n"
        << "This points to an improperly installed library or\n"
        << "improperly installed application.)\n";
    std::exit(EXIT_FAILURE);
  }
}
} // namespace SimulationIO
