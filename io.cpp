#include "asdf_io.hpp"

#include <yaml-cpp/yaml.h>

namespace ASDF {

// I/O

reader_state::reader_state(istream &is) {
  for (;;) {
    const auto &block = read_block(is);
    if (!block)
      break;
    blocks.push_back(move(block));
  }
}

writer_state::writer_state() {}

writer_state::~writer_state() { assert(tasks.empty()); }

void writer_state::flush(ostream &os) {
  if (tasks.empty())
    return;
  YAML::Emitter index;
  index << YAML::Flow;
  index << YAML::BeginSeq;
  for (auto &&task : tasks) {
    index << os.tellp();
    move(task)(os);
  }
  tasks.clear();
  index << YAML::EndSeq;
  os << "#ASDF BLOCK INDEX\n"
     << "%YAML 1.1\n"
     << "---\n"
     << index.c_str() << "\n"
     << "...\n";
}

} // namespace ASDF
