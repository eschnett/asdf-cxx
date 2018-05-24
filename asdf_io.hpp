#ifndef ASDF_IO_HPP
#define ASDF_IO_HPP

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

namespace ASDF {
using namespace std;

// I/O

enum class block_format_t { undefined, block, inline_array };
enum class compression_t { undefined, none, bzip2, zlib };

class generic_blob_t;
shared_ptr<generic_blob_t> read_block(istream &is);

class reader_state {
  // TODO: Store only the file position
  vector<shared_ptr<generic_blob_t>> blocks;

public:
  reader_state() = delete;
  reader_state(const reader_state &) = delete;
  reader_state(reader_state &&) = delete;
  reader_state &operator=(const reader_state &) = delete;
  reader_state &operator=(reader_state &&) = delete;

  reader_state(istream &is);

  shared_ptr<generic_blob_t> get_block(int64_t index) const {
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

class writer_state {

  ostream &os;
  YAML::Emitter emitter;
  // Tasks that write the blocks
  vector<function<void(ostream &os)>> tasks;

public:
  writer_state(const writer_state &) = delete;
  writer_state(writer_state &&) = delete;
  writer_state &operator=(const writer_state &) = delete;
  writer_state &operator=(writer_state &&) = delete;

  writer_state(ostream &os);
  ~writer_state();

  // YAML::Emitter &e() { return emitter; }
  // template <typename T>
  // friend writer_state &operator<<(writer_state &ws, const T &value);
  template <typename T>
  friend writer_state &operator<<(writer_state &ws, const T &value) {
    ws.emitter << value;
    return ws;
  }

  int64_t add_task(function<void(ostream &)> &&task) {
    tasks.push_back(move(task));
    return tasks.size() - 1;
  }

  void flush();
};

// template <typename T>
// writer_state &operator<<(writer_state &ws, const T &value) {
//   ws.e() << value;
//   return ws;
// }

} // namespace ASDF

#define ASDF_IO_HPP_DONE
#endif // #ifndef ASDF_IO_HPP
#ifndef ASDF_IO_HPP_DONE
#error "Cyclic include depencency"
#endif
