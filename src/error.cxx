#include <asdf/error.hxx>

#include <cstring>

namespace ASDF {

namespace {
std::string what_string(const char *file, int line,
                        const std::string &message) {
  // Only the base name of the source file
  const char *base = std::strrchr(file, '/');
  base = base ? base + 1 : file;
  return message + " [" + base + ":" + std::to_string(line) + "]";
}
} // namespace

error::error(const char *file, int line, const std::string &message)
    : std::runtime_error(what_string(file, line, message)), msg(message) {}

} // namespace ASDF
