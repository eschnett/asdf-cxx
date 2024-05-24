#include <asdf/io.hxx>

#include <asdf/asdf.hxx>
#include <asdf/ndarray.hxx>

#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <fstream>

namespace ASDF {

const string asdf_format_version = "1.0.0";

bool have_int128() {
#ifdef ASDF_HAVE_INT128
  return true;
#else
  return false;
#endif
}
bool have_float16() {
#ifdef ASDF_HAVE_FLOAT16
  return true;
#else
  return false;
#endif
}

bool have_checksum() {
#ifdef ASDF_HAVE_OPENSSL
  return true;
#else
  return false;
#endif
}
bool have_compression_blosc() {
#ifdef ASDF_HAVE_BLOSC
  return true;
#else
  return false;
#endif
}
bool have_compression_blosc2() {
#ifdef ASDF_HAVE_BLOSC2
  return true;
#else
  return false;
#endif
}
bool have_compression_bzip2() {
#ifdef ASDF_HAVE_BZIP2
  return true;
#else
  return false;
#endif
}
bool have_compression_liblz4() {
#ifdef ASDF_HAVE_LIBLZ4
  return true;
#else
  return false;
#endif
}
bool have_compression_libzstd() {
#ifdef ASDF_HAVE_LIBZSTD
  return true;
#else
  return false;
#endif
}
bool have_compression_zlib() {
#ifdef ASDF_HAVE_ZLIB
  return true;
#else
  return false;
#endif
}

// I/O

std::ostream &operator<<(std::ostream &os, block_format_t block_format) {
  switch (block_format) {
  case block_format_t::block:
    return os << "block";
  case block_format_t::inline_array:
    return os << "inline_array";
  default:
    return os << "unknown";
  }
}

std::ostream &operator<<(std::ostream &os, compression_t compression) {
  switch (compression) {
  case compression_t::none:
    return os << "none";
  case compression_t::blosc:
    return os << "blosc";
  case compression_t::blosc2:
    return os << "blosc2";
  case compression_t::bzip2:
    return os << "bzip2";
  case compression_t::liblz4:
    return os << "liblz4";
  case compression_t::libzstd:
    return os << "libzstd";
  case compression_t::zlib:
    return os << "zlib";
  default:
    return os << "unknown";
  }
}

reader_state::reader_state(const YAML::Node &tree,
                           const shared_ptr<istream> &pis,
                           const string &filename)
    : tree(tree), filename(filename) {
  for (;;) {
    const auto [block, block_info] = ndarray::read_block(pis);
    if (!block.valid())
      break;
    blocks.push_back(std::move(block));
    block_infos.push_back(std::move(block_info));
  }
}

block_info_t reader_state::get_block_info(int64_t index) const {
  assert(index >= 0);
  return block_infos.at(index);
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
          std::abort();
        }
      }();
      node = unique_ptr<YAML::Node>(new YAML::Node((*node)[idx]));
    } else if (node->IsMap()) {
      node = unique_ptr<YAML::Node>(new YAML::Node((*node)[elem]));
    } else {
      // Could not resolve reference
      // TODO: Output an actual error message
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
    string ref_filename;
    if (!filename.empty() && filename[0] == '/') {
      // absolute path
      ref_filename = filename;
    } else {
      // preprend current path
      assert(!rs->filename.empty()); // We could allow this
      auto slashpos = rs->filename.rfind('/');
      if (slashpos == string::npos)
        ref_filename = filename;
      else
        ref_filename = rs->filename.substr(0, slashpos + 1) + filename;
    }
    if (!rs->other_files.count(ref_filename)) {
      auto pis = make_shared<ifstream>(ref_filename, ios::binary | ios::in);
      auto doc = asdf::from_yaml((istream &)*pis);
      rs->other_files[ref_filename] =
          make_shared<reader_state>(doc, pis, ref_filename);
    }
    refrs = rs->other_files.at(ref_filename);
  }

  auto node = refrs->resolve_reference(path);
  return make_pair(refrs, node);
}

writer::writer(ostream &os, const map<string, string> &tags)
    : os(os), emitter(os) {
  // yaml-cpp does not support comments without leading space
  os << "#ASDF " << asdf_format_version << "\n"
     << "#ASDF_STANDARD " << asdf_standard_version() << "\n"
     << "# This is an ASDF file <https://asdf-standard.readthedocs.io/>\n"
     // yaml-cpp does not support writing a YAML tag
     << "%YAML 1.1\n"
     << "%TAG ! tag:stsci.edu:asdf/\n";
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
      std::move(task)(os);
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
