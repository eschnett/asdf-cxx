#include "asdf_io.hpp"

#include <yaml-cpp/yaml.h>

namespace ASDF {

const string asdf_format_version = "1.0.0";
const string asdf_standard_version = "1.1.0";

// I/O

reader_state::reader_state(istream &is) {
  for (;;) {
    const auto &block = read_block(is);
    if (!block)
      break;
    blocks.push_back(move(block));
  }
}

writer::writer(ostream &os) : os(os), emitter(os) {
  // yaml-cpp does not support comments without leading space
  os << "#ASDF " << asdf_format_version << "\n"
     << "#ASDF_STANDARD " << asdf_standard_version << "\n"
     << "# This is an ASDF file <https://asdf-standard.readthedocs.io/>\n"
     // yaml-cpp does not support writing a YAML tab
     << "%YAML 1.1\n"
     << "%TAG ! tag:stsci.edu:asdf/\n"
     << "%TAG !asdf-cxx! tag:github.com/eschnett/asdf-cxx/\n";
  emitter << YAML::BeginDoc;
}

writer::~writer() { assert(tasks.empty()); }

void writer::flush() {
  emitter << YAML::EndDoc;
  if (!tasks.empty()) {
    YAML::Emitter index;
    index << YAML::BeginDoc << YAML::Flow << YAML::BeginSeq;
    for (auto &&task : tasks) {
      index << os.tellp();
      move(task)(os);
    }
    tasks.clear();
    index << YAML::EndSeq << YAML::EndDoc;
    // yaml-cpp does not support comments without leading space
    os << "#ASDF BLOCK INDEX\n"
       // yaml-cpp does not support writing a YAML tab
       << "%YAML 1.1\n"
       << index.c_str();
  }
}

} // namespace ASDF
