#include "asdf_asdf.hpp"
#include "asdf_io.hpp"
#include "asdf_ndarray.hpp"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace ASDF {

const string asdf_format_version = "1.0.0";
const string asdf_standard_version = "1.1.0";

// I/O

reader_state::reader_state(const YAML::Node &tree,
                           const shared_ptr<istream> &pis)
    : tree(tree) {
  for (;;) {
    const auto &block = ndarray::read_block(pis);
    if (!block.valid())
      break;
    blocks.push_back(move(block));
  }
}

YAML::Node reader_state::resolve_reference(const vector<string> &path) const {
  // We allocate a new YAML node each time we take a step. If we don't
  // do this, yaml-cpp will instead only create a reference (alias) to
  // the new node, thus effectively overwriting the "tree" field.
  auto node = unique_ptr<YAML::Node>(new YAML::Node(tree));
  assert(node->IsDefined());
  for (const auto &elem : path) {
    if (node->IsSequence()) {
      int idx = [=]() {
        try {
          return stoi(elem);
        } catch (exception &) {
          assert(0);
        }
      }();
      node = unique_ptr<YAML::Node>(new YAML::Node((*node)[idx]));
    } else if (node->IsMap()) {
      node = unique_ptr<YAML::Node>(new YAML::Node((*node)[elem]));
    } else {
      assert(0);
    }
    assert(node->IsDefined());
  }
  return *node;
}

pair<shared_ptr<reader_state>, YAML::Node>
reader_state::resolve_reference(const shared_ptr<reader_state> &rs,
                                const string &filename,
                                const vector<string> &path) {
  shared_ptr<reader_state> refrs;
  if (filename.empty()) {
    // Read from same file
    refrs = rs;
  } else {
    // Read from external file
    if (!rs->other_files.count(filename)) {
      auto pis = make_shared<ifstream>(filename, ios::binary | ios::in);
      auto doc = asdf::from_yaml((istream &)*pis);
      rs->other_files[filename] = make_shared<reader_state>(doc, pis);
    }
    refrs = rs->other_files.at(filename);
  }

  auto node = refrs->resolve_reference(path);
  return make_pair(refrs, node);
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
