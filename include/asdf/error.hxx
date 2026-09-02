#ifndef ASDF_ERROR_HXX
#define ASDF_ERROR_HXX

#include <stdexcept>
#include <string>

namespace ASDF {

// Thrown for malformed input, unsupported features, and failures reported by
// compression or checksum libraries. Internal invariants use `assert`.
class error : public std::runtime_error {
  std::string msg;

public:
  error(const char *file, int line, const std::string &message);
  // The message without the source location
  const std::string &message() const { return msg; }
};

} // namespace ASDF

// Throw an ASDF::error. `msg` is a std::string or a string literal.
#define ASDF_ERROR(msg) throw ::ASDF::error(__FILE__, __LINE__, (msg))

// Throw an ASDF::error unless `cond` holds. Unlike `assert`, this is checked
// in all build types; use it for input validation, not for invariants.
#define ASDF_CHECK(cond, msg)                                                  \
  do {                                                                         \
    if (!(cond))                                                               \
      ASDF_ERROR(msg);                                                         \
  } while (0)

#define ASDF_ERROR_HXX_DONE
#endif // #ifndef ASDF_ERROR_HXX
#ifndef ASDF_ERROR_HXX_DONE
#error "Cyclic include depencency"
#endif
