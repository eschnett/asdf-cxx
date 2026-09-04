#ifndef ASDF_IO_HXX
#define ASDF_IO_HXX

#include <asdf/error.hxx>
#include <asdf/memoized.hxx>
#include <asdf/version.hxx>

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <cmath>
#include <complex>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ASDF {
using namespace std;

// I/O

enum class block_format_t { undefined, block, inline_array };
enum class compression_t {
  undefined,
  none,
  blosc,
  blosc2,
  bzip2,
  lz4,           // ASDF standard lz4 encoding (block token "lz4"), read and
                 // written by the Python reference implementation
  lz4f,          // LZ4 frame format (block token "lz4f"), an asdf-cxx extension
  liblz4 = lz4f, // deprecated name for lz4f
  libzstd,
  zlib
};

bool have_float16();
bool have_int128();

bool have_checksum();
bool have_compression_blosc();
bool have_compression_blosc2();
bool have_compression_bzip2();
bool have_compression_liblz4();
bool have_compression_libzstd();
bool have_compression_zlib();

std::ostream &operator<<(std::ostream &os, block_format_t block_format);
std::ostream &operator<<(std::ostream &os, compression_t compression);

// The two comment lines a file begins with. They are recorded while reading
// and never rejected: a file may declare a format or standard version this
// library does not know.
struct file_header {
  string asdf_version;
  string standard_version;
};

// What the content of a tree requires of the standard version it is written
// as. Collected by a pre-pass over the tree (`asdf::requirements()`) that
// touches only metadata, never block data.
struct content_requirements {
  // The tree holds `float16` data, which needs standard 1.6.0. This is a
  // legitimate feature of that version, not a nonstandard extension.
  bool needs_float16 = false;
  // Content no version of the standard describes, as "<path>: <what>"
  vector<string> nonstandard;

  // The lowest standard version that can represent this content. This is
  // the floor the requested version has to clear; the version a new file is
  // written as is `max(default_standard_version(), minimum_version())`.
  version_t minimum_version() const;
};

// How `asdf::write` chooses the standard version, and whether it accepts
// content no version of the standard describes
struct write_options {
  enum class version_mode_t {
    minimal,         // the lowest version that fits the content
    latest,          // the most recent version this library knows
    input,           // the version the file was read from declared
    explicit_version // `explicit_version`
  };
  version_mode_t version_mode = version_mode_t::minimal;
  version_t explicit_version{};
  bool allow_nonstandard = false;
};

// Set `version_mode` and `explicit_version` from a
// "minimal"|"latest"|"input"|"X.Y.Z" specification
void set_standard_version(write_options &options, const string &spec);

// Scalar spellings
//
// yaml-cpp emits a floating-point value the way C++ streams do, which loses
// two distinctions ASDF cares about: a value with no fractional part comes
// out as an integer (`1.0` as `1`), and non-finite values come out in YAML's
// own spelling (`.inf`, `.nan`), which the `core/complex-1.0.0` grammar does
// not accept.

namespace detail {
// yaml-cpp's spelling of a floating-point value, with the sign of a NaN
// normalised so that every platform writes the same file
template <typename T> string emit_float(T value) {
  if (std::isnan(value))
    return ".nan";
  YAML::Emitter em;
  em << value;
  return string(em.c_str());
}
} // namespace detail

// The YAML spelling of a floating-point value. YAML 1.1 resolves a scalar to
// a float only if its mantissa carries a decimal point, so a mantissa of
// digits only gains a `.0`: `1` becomes `1.0` and `1e+17` becomes `1.0e+17`.
// Without it a metadata scalar written `1.0` copies out as `1` and is read
// back as an integer, and one written `3.0e-10` copies out as `3e-10` and is
// read back as a string.
template <typename T> string format_float(T value) {
  string str = detail::emit_float(value);
  // Everything before an `e`/`E` exponent; the whole string when there is
  // none. `.inf` and `.nan` have a dot and are left alone.
  const size_t mantissa = str.find_first_of("eE");
  const size_t end = mantissa == string::npos ? str.size() : mantissa;
  const size_t start = end > 0 && (str[0] == '-' || str[0] == '+') ? 1 : 0;
  bool integral = start < end;
  for (size_t i = start; i < end; ++i)
    integral = integral && str[i] >= '0' && str[i] <= '9';
  if (integral)
    str.insert(end, ".0");
  return str;
}

// The `core/complex-1.0.0` spelling of a complex number,
// `<real><sign><imag>i`. That schema's grammar writes the non-finite
// components `inf`, `-inf` and `nan`, without the leading dot YAML itself
// uses, and takes the imaginary part's sign from the separator.
template <typename T> string format_complex(const complex<T> &value) {
  const auto component = [](T val) {
    string str = detail::emit_float(val);
    const size_t dot = str.find('.');
    if (dot != string::npos && dot + 1 < str.size() &&
        (str[dot + 1] == 'i' || str[dot + 1] == 'n'))
      str.erase(dot, 1);
    return str;
  };
  const string re = component(value.real());
  const string im = component(value.imag());
  return re + (!im.empty() && im[0] == '-' ? "" : "+") + im + "i";
}

class block_t;
struct block_info_t;

class reader_state {
  YAML::Node tree;
  // TODO: Share "other_files" with other reader_state objects
  string filename;
  file_header header;
  map<string, shared_ptr<reader_state>> other_files;

  // TODO: Store only the file position
  vector<memoized<block_t>> blocks;
  vector<block_info_t> block_infos;

public:
  reader_state() = delete;
  reader_state(const reader_state &) = delete;
  reader_state(reader_state &&) = default;
  reader_state &operator=(const reader_state &) = delete;
  reader_state &operator=(reader_state &&) = default;

  reader_state(const YAML::Node &tree, const shared_ptr<istream> &pis,
               const string &filename = {}, const file_header &header = {});

  const file_header &get_input_header() const { return header; }

  memoized<block_t> get_block(int64_t index) const {
    ASDF_CHECK(index >= 0 && size_t(index) < blocks.size(),
               "Block index " + std::to_string(index) +
                   " is out of range; the file has " +
                   std::to_string(blocks.size()) + " blocks");
    return blocks.at(index);
  }

  block_info_t get_block_info(int64_t index) const;

  YAML::Node resolve_reference(const vector<string> &path) const;

  static pair<shared_ptr<reader_state>, YAML::Node>
  resolve_reference(const shared_ptr<reader_state> &rs, const string &filename,
                    const vector<string> &path);
};

struct copy_state {
  bool set_block_format;
  block_format_t block_format;
  bool set_compression;
  compression_t compression;
  bool set_compression_level;
  int compression_level;
};

class writer {

  ostream &os;
  YAML::Emitter emitter;
  const standard_info_t *standard_;
  bool allow_nonstandard_;

  // Tasks that write the blocks
  // TODO: rename this variable
  vector<function<void(ostream &os)>> tasks;

public:
  writer(const writer &) = delete;
  writer(writer &&) = delete;
  writer &operator=(const writer &) = delete;
  writer &operator=(writer &&) = delete;

  writer(ostream &os, const map<string, string> &tags,
         const standard_info_t &standard, bool allow_nonstandard = false);
  ~writer();

  // The standard version being written. Every core tag comes from here.
  const standard_info_t &standard() const { return *standard_; }
  bool allow_nonstandard() const { return allow_nonstandard_; }

  template <typename T> friend writer &operator<<(writer &w, const T &value) {
    w.emitter << value;
    return w;
  }

  template <typename T>
  friend writer &operator<<(writer &w, const std::complex<T> &value) {
    // see `yaml_encode(const complex<T> &val)`, which spells the number the
    // same way but has no writer to take the tag from
    w << YAML::LocalTag(w.standard().complex_tag) << format_complex(value);
    return w;
  }

  // TODO: rename this function
  int64_t add_task(function<void(ostream &)> &&task) {
    tasks.push_back(std::move(task));
    return tasks.size() - 1;
  }

  void flush();
};

// A tag that carries no information: absent, yaml-cpp's markers for an
// untagged (`?`) or non-specifically tagged (`!`) node, or one of YAML's own
// types. Such a tag is never stored and never emitted.
bool is_trivial_tag(const string &full_tag);

// Emit a tag that was read from a file, given as its full resolved URI.
// Trivial tags emit nothing; a tag under `asdf_tag_prefix` becomes a local
// tag (`!core/constant-1.0.0`), which needs the `%TAG !` directive every file
// this library writes has; everything else becomes a verbatim tag
// (`!<asdf://example.org/foo-1.0.0>`).
writer &emit_tag(writer &w, const string &full_tag);

// Emit a whole `YAML::Node` tree through the Emitter. yaml-cpp's own
// `Emitter << Node` goes through `EmitFromEvents`, which spells every tag
// verbatim (`!<tag:stsci.edu:asdf/core/complex-1.0.0>`); this uses
// `emit_tag` instead, so a core tag comes out in the local form the
// standard's examples use. Flow and block style are preserved.
writer &emit_node(writer &w, const YAML::Node &node);

} // namespace ASDF

#define ASDF_IO_HXX_DONE
#endif // #ifndef ASDF_IO_HXX
#ifndef ASDF_IO_HXX_DONE
#error "Cyclic include depencency"
#endif
