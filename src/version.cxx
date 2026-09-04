#include <asdf/version.hxx>

#include <cstdlib>
#include <sstream>
#include <string>

namespace ASDF {

namespace {

// Parse a run of decimal digits; `pos` is advanced past them
bool parse_component(const string &str, size_t &pos, int &value) {
  const size_t begin = pos;
  long result = 0;
  while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
    if (result > 1000000)
      return false;
    result = 10 * result + (str[pos] - '0');
    ++pos;
  }
  if (pos == begin)
    return false;
  value = int(result);
  return true;
}

} // namespace

version_t version_t::parse(const string &str) {
  version_t version;
  size_t pos = 0;
  const bool ok = parse_component(str, pos, version.major) &&
                  pos < str.size() && str[pos++] == '.' &&
                  parse_component(str, pos, version.minor) &&
                  pos < str.size() && str[pos++] == '.' &&
                  parse_component(str, pos, version.patch) && pos == str.size();
  ASDF_CHECK(ok, "Malformed version \"" + str + "\"; expected \"X.Y.Z\"");
  return version;
}

string version_t::str() const {
  ostringstream buf;
  buf << major << "." << minor << "." << patch;
  return buf.str();
}

const vector<standard_info_t> &standard_versions() {
  // core/software-1.0.0, core/complex-1.0.0 and core/history_entry-1.0.0 are
  // the same in every standard version. core/asdf changed in 1.2.0, which is
  // also where `history` became a mapping with `extensions`; core/ndarray
  // changed in 1.6.0, which is where `float16` was added.
  static const vector<standard_info_t> versions{
      {{1, 0, 0},
       "core/asdf-1.0.0",
       "core/ndarray-1.0.0",
       "core/software-1.0.0",
       "core/complex-1.0.0",
       "core/history_entry-1.0.0",
       nullptr,
       false,
       false},
      {{1, 1, 0},
       "core/asdf-1.0.0",
       "core/ndarray-1.0.0",
       "core/software-1.0.0",
       "core/complex-1.0.0",
       "core/history_entry-1.0.0",
       nullptr,
       false,
       false},
      {{1, 2, 0},
       "core/asdf-1.1.0",
       "core/ndarray-1.0.0",
       "core/software-1.0.0",
       "core/complex-1.0.0",
       "core/history_entry-1.0.0",
       "core/extension_metadata-1.0.0",
       false,
       true},
      {{1, 3, 0},
       "core/asdf-1.1.0",
       "core/ndarray-1.0.0",
       "core/software-1.0.0",
       "core/complex-1.0.0",
       "core/history_entry-1.0.0",
       "core/extension_metadata-1.0.0",
       false,
       true},
      {{1, 4, 0},
       "core/asdf-1.1.0",
       "core/ndarray-1.0.0",
       "core/software-1.0.0",
       "core/complex-1.0.0",
       "core/history_entry-1.0.0",
       "core/extension_metadata-1.0.0",
       false,
       true},
      {{1, 5, 0},
       "core/asdf-1.1.0",
       "core/ndarray-1.0.0",
       "core/software-1.0.0",
       "core/complex-1.0.0",
       "core/history_entry-1.0.0",
       "core/extension_metadata-1.0.0",
       false,
       true},
      {{1, 6, 0},
       "core/asdf-1.1.0",
       "core/ndarray-1.1.0",
       "core/software-1.0.0",
       "core/complex-1.0.0",
       "core/history_entry-1.0.0",
       "core/extension_metadata-1.0.0",
       true,
       true},
  };
  return versions;
}

const standard_info_t &standard_info(const version_t &version) {
  for (const auto &info : standard_versions())
    if (info.version == version)
      return info;
  ostringstream buf;
  buf << "Unknown ASDF standard version \"" << version.str()
      << "\"; the supported standard versions are";
  for (const auto &info : standard_versions())
    buf << " " << info.version.str();
  ASDF_ERROR(buf.str());
}

version_t default_standard_version() { return {1, 2, 0}; }

version_t latest_standard_version() {
  return standard_versions().back().version;
}

core_tag_t classify_core_tag(const string &full_tag) {
  const size_t prefix_length = sizeof asdf_tag_prefix - 1;
  if (full_tag.compare(0, prefix_length, asdf_tag_prefix) != 0)
    return core_tag_t::none;
  const string tag = full_tag.substr(prefix_length);
  for (const auto &info : standard_versions()) {
    if (tag == info.asdf_tag)
      return core_tag_t::asdf;
    if (tag == info.ndarray_tag)
      return core_tag_t::ndarray;
    if (tag == info.software_tag)
      return core_tag_t::software;
    if (tag == info.complex_tag)
      return core_tag_t::complex_;
  }
  return core_tag_t::none;
}

} // namespace ASDF
