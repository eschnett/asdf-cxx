#ifndef ASDF_IO_HXX
#define ASDF_IO_HXX

#include <asdf/memoized.hxx>

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
  liblz4,
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

class block_t;
struct block_info_t;

class reader_state {
  YAML::Node tree;
  // TODO: Share "other_files" with other reader_state objects
  string filename;
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
               const string &filename = {});

  memoized<block_t> get_block(int64_t index) const {
    assert(index >= 0);
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

  // Tasks that write the blocks
  // TODO: rename this variable
  vector<function<void(ostream &os)>> tasks;

public:
  writer(const writer &) = delete;
  writer(writer &&) = delete;
  writer &operator=(const writer &) = delete;
  writer &operator=(writer &&) = delete;

  writer(ostream &os, const map<string, string> &tags);
  ~writer();

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

    w << YAML::LocalTag("core/complex-1.0.0") << buf.str();
    return w;
  }

  // TODO: rename this function
  int64_t add_task(function<void(ostream &)> &&task) {
    tasks.push_back(std::move(task));
    return tasks.size() - 1;
  }

  void flush();
};

} // namespace ASDF

#define ASDF_IO_HXX_DONE
#endif // #ifndef ASDF_IO_HXX
#ifndef ASDF_IO_HXX_DONE
#error "Cyclic include depencency"
#endif
