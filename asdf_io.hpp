#ifndef ASDF_IO_HPP
#define ASDF_IO_HPP

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <vector>

namespace ASDF {
using namespace std;

// I/O

enum class block_format_t { undefined, block, inline_array };
enum class compression_t { undefined, none, bzip2, zlib };

class generic_blob_t;
shared_future<shared_ptr<generic_blob_t>>
read_block(const shared_ptr<istream> &pis);

class reader_state {
  YAML::Node doc;
  // TODO: Store only the file position
  vector<shared_future<shared_ptr<generic_blob_t>>> blocks;

  friend YAML::Node resolve_reference(const reader_state &rs,
                                      const vector<string> &doc_path);

public:
  reader_state() = delete;
  reader_state(const reader_state &) = delete;
  reader_state(reader_state &&) = delete;
  reader_state &operator=(const reader_state &) = delete;
  reader_state &operator=(reader_state &&) = delete;

  reader_state(const YAML::Node &doc, const shared_ptr<istream> &pis);

  shared_future<shared_ptr<generic_blob_t>> get_block(int64_t index) const {
    assert(index >= 0);
    return blocks.at(index);
  }
};

struct copy_state {
  bool set_block_format;
  block_format_t block_format;
  bool set_compression;
  compression_t compression;
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

  // TODO: rename this function
  int64_t add_task(function<void(ostream &)> &&task) {
    tasks.push_back(move(task));
    return tasks.size() - 1;
  }

  void flush();
};

} // namespace ASDF

#define ASDF_IO_HPP_DONE
#endif // #ifndef ASDF_IO_HPP
#ifndef ASDF_IO_HPP_DONE
#error "Cyclic include depencency"
#endif
