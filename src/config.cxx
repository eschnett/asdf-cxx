#include <asdf/config.hxx>

#include <cstdlib>
#include <iostream>
#include <sstream>
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

void check_version(const char *const header_version, const bool have_float16,
                   const bool have_int128) {
  if (header_version == asdf_cxx_version() &&
      have_float16 == ASDF_FLOAT16_SUPPORTED &&
      have_int128 == ASDF_INT128_SUPPORTED)
    return;
  std::ostringstream buf;
  if (header_version != asdf_cxx_version())
    buf << "Version mismatch detected:\n"
        << "  Include headers have version " << header_version << ",\n"
        << "  Linked library has version   " << asdf_cxx_version() << ".\n";
  if (have_float16 != ASDF_FLOAT16_SUPPORTED)
    buf << "Datatype mismatch detected:\n"
        << "  Include headers have have_datatype_float16=" << have_float16
        << ",\n"
        << "  Linked library has   have_datatype_float16="
        << bool(ASDF_FLOAT16_SUPPORTED) << ".\n";
  if (have_int128 != ASDF_INT128_SUPPORTED)
    buf << "Datatype mismatch detected:\n"
        << "  Include headers have have_datatype_int128=" << have_int128
        << ",\n"
        << "  Linked library has   have_datatype_int128="
        << bool(ASDF_INT128_SUPPORTED) << ".\n";
  buf << "The versions of the include headers and linked libraries differ.\n"
      << "  This points to an improperly installed library or\n"
      << "  improperly installed application.\n";
  std::cerr << buf.str();
  std::exit(EXIT_FAILURE);
}

} // namespace ASDF
