#ifndef ASDF_IO_HXX
#define ASDF_IO_HXX

#include <asdf/error.hxx>
#include <asdf/memoized.hxx>
#include <asdf/version.hxx>

#include <yaml-cpp/yaml.h>

#include <cassert>
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
    // see `yaml_encode(const complex<T> &val)`
    YAML::Emitter re;
    re << value.real();
    YAML::Emitter im;
    im << value.imag();
    ostringstream buf;
    buf << re.c_str();
    if (im.c_str()[0] != '-')
      buf << "+";
    buf << im.c_str() << "i";

    w << YAML::LocalTag(w.standard().complex_tag) << buf.str();
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

} // namespace ASDF

#define ASDF_IO_HXX_DONE
#endif // #ifndef ASDF_IO_HXX
#ifndef ASDF_IO_HXX_DONE
#error "Cyclic include depencency"
#endif
