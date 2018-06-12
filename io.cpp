#include "asdf_io.hpp"
#include "asdf_ndarray.hpp"

#include <yaml-cpp/yaml.h>

namespace ASDF {

const string asdf_format_version = "1.0.0";
const string asdf_standard_version = "1.1.0";

// I/O

reader_state::reader_state(const YAML::Node &doc,
                           const shared_ptr<istream> &pis)
    : doc(doc) {
  for (;;) {
    const auto &block = ndarray::read_block(pis);
    if (!block.valid())
      break;
    blocks.push_back(move(block));
  }
}

YAML::Node
reader_state::resolve_reference(const vector<string> &doc_path) const {
  // We allocate a new YAML node each time we take a step. If we don't
  // do this, yaml-cpp will instead only create a reference (alias) to
  // the new node, thus effectively overwriting the "doc" argument.
  auto node = unique_ptr<YAML::Node>(new YAML::Node(doc));
  assert(node->IsDefined());
  for (const auto &elem : doc_path) {
    if (node->IsSequence()) {
      try {
        int idx = stoi(elem);
        node = unique_ptr<YAML::Node>(new YAML::Node((*node)[idx]));
      } catch (exception &) {
        assert(0);
      }
    } else if (node->IsMap()) {
      node = unique_ptr<YAML::Node>(new YAML::Node((*node)[elem]));
    } else {
      assert(0);
    }
    assert(node->IsDefined());
  }
  return *node;
}

writer::writer(ostream &os, const map<string, string> &tags)
    : os(os), emitter(os) {
  // yaml-cpp does not support comments without leading space
  os << "#ASDF " << asdf_format_version << "\n"
     << "#ASDF_STANDARD " << asdf_standard_version << "\n"
     << "# This is an ASDF file <https://asdf-standard.readthedocs.io/>\n"
     // yaml-cpp does not support writing a YAML tag
     << "%YAML 1.1\n"
     << "%TAG ! tag:stsci.edu:asdf/\n"
     << "%TAG !asdf-cxx! tag:github.com/eschnett/asdf-cxx/\n";
  for (const auto &kv : tags)
    os << "%TAG !" << kv.first << "! " << kv.second << "\n";
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
       // yaml-cpp does not support writing a YAML tag
       << "%YAML 1.1\n"
       << index.c_str();
  }
}

} // namespace ASDF
