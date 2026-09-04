#ifndef ASDF_VERSION_HXX
#define ASDF_VERSION_HXX

#include <asdf/error.hxx>

#include <string>
#include <vector>

// Some systems define `major` and `minor` as macros in <sys/sysmacros.h>,
// which would mangle the members of `version_t`
#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

namespace ASDF {
using namespace std;

// ASDF standard versions
//
// This is the one place that knows which tag versions belong to which
// version of the ASDF standard. Nothing else may spell a `core/...` tag.

struct version_t {
  int major = 0;
  int minor = 0;
  int patch = 0;

  version_t() = default;
  constexpr version_t(int major, int minor, int patch)
      : major(major), minor(minor), patch(patch) {}

  // Parse "X.Y.Z"; ASDF_ERROR on anything else
  static version_t parse(const string &str);

  string str() const;

  friend bool operator==(const version_t &x, const version_t &y) {
    return x.major == y.major && x.minor == y.minor && x.patch == y.patch;
  }
  friend bool operator!=(const version_t &x, const version_t &y) {
    return !(x == y);
  }
  friend bool operator<(const version_t &x, const version_t &y) {
    if (x.major != y.major)
      return x.major < y.major;
    if (x.minor != y.minor)
      return x.minor < y.minor;
    return x.patch < y.patch;
  }
  friend bool operator>(const version_t &x, const version_t &y) {
    return y < x;
  }
  friend bool operator<=(const version_t &x, const version_t &y) {
    return !(y < x);
  }
  friend bool operator>=(const version_t &x, const version_t &y) {
    return !(x < y);
  }
};

// The tags a file of a given standard version uses. The tags are stored in
// the local form (`core/asdf-1.1.0`), i.e. relative to `asdf_tag_prefix`.
struct standard_info_t {
  version_t version;
  const char *asdf_tag;
  const char *ndarray_tag;
  const char *software_tag;
  const char *complex_tag;
  const char *history_entry_tag;
  // `core/extension_metadata-1.0.0`, or null before standard 1.2.0
  const char *extension_metadata_tag;
  // The `float16` datatype exists since standard 1.6.0 (ndarray-1.1.0)
  bool has_float16;
  // `history` is a mapping with an `extensions` list since standard 1.2.0
  bool has_history_extensions;
};

// The prefix the `%TAG !` directive of every file this library writes maps
// to, and the prefix of every core tag
constexpr const char asdf_tag_prefix[] = "tag:stsci.edu:asdf/";

// All standard versions this library knows, in increasing order
const vector<standard_info_t> &standard_versions();

// The entry for `version`; ASDF_ERROR listing the supported versions
const standard_info_t &standard_info(const version_t &version);

// The version new files are written as unless their content needs more
version_t default_standard_version();
// The most recent version this library knows
version_t latest_standard_version();

// Which core tag a full tag URI is, in any standard version this library
// knows. `core_tag_t::none` for everything else, including a tag that only
// differs in its version number.
enum class core_tag_t { none, asdf, ndarray, software, complex_ };
core_tag_t classify_core_tag(const string &full_tag);

// Whether a full tag URI is a `core/asdf` tag of *any* version, including one
// this library does not know. The reader accepts such a root tag: a file
// written against a newer standard is still worth reading.
bool is_core_asdf_tag(const string &full_tag);

} // namespace ASDF

#define ASDF_VERSION_HXX_DONE
#endif // #ifndef ASDF_VERSION_HXX
#ifndef ASDF_VERSION_HXX_DONE
#error "Cyclic include depencency"
#endif
