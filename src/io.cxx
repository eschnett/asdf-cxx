#include <asdf/io.hxx>

#include <asdf/asdf.hxx>
#include <asdf/ndarray.hxx>

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
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
  case compression_t::lz4:
    return os << "lz4";
  case compression_t::lz4f:
    return os << "lz4f";
  case compression_t::libzstd:
    return os << "libzstd";
  case compression_t::zlib:
    return os << "zlib";
  default:
    return os << "unknown";
  }
}

// Tags

namespace {
// The characters yaml-cpp accepts in the suffix of a local tag (`!suffix`).
// Flow indicators and `!` would make the shorthand ambiguous, and yaml-cpp
// puts the emitter into its error state when it sees one.
bool is_tag_char(unsigned char ch) {
  return isalnum(ch) || strchr("-;/?:@&=+$_.~*'()%", ch) != nullptr;
}
} // namespace

bool is_trivial_tag(const string &full_tag) {
  static const string yaml_prefix = "tag:yaml.org,2002:";
  return full_tag.empty() || full_tag == "?" || full_tag == "!" ||
         full_tag.compare(0, yaml_prefix.size(), yaml_prefix) == 0;
}

writer &emit_tag(writer &w, const string &full_tag) {
  if (is_trivial_tag(full_tag))
    return w;
  const size_t prefix_length = sizeof asdf_tag_prefix - 1;
  if (full_tag.compare(0, prefix_length, asdf_tag_prefix) == 0) {
    const string suffix = full_tag.substr(prefix_length);
    bool local = !suffix.empty();
    for (const unsigned char ch : suffix)
      local = local && is_tag_char(ch);
    if (local)
      return w << YAML::LocalTag(suffix);
  }
  // An unresolved shorthand arrives as `!foo` and becomes `!<!foo>`, which is
  // a local tag in verbatim form and thus valid YAML
  return w << YAML::VerbatimTag(full_tag);
}

// Standard version selection

version_t content_requirements::minimum_version() const {
  if (needs_float16) {
    // `float16` exists only since standard 1.6.0
    for (const auto &info : standard_versions())
      if (info.has_float16)
        return info.version;
  }
  // Everything else this library writes fits into every known version
  return standard_versions().front().version;
}

void set_standard_version(write_options &options, const string &spec) {
  if (spec == "minimal") {
    options.version_mode = write_options::version_mode_t::minimal;
  } else if (spec == "latest") {
    options.version_mode = write_options::version_mode_t::latest;
  } else if (spec == "input") {
    options.version_mode = write_options::version_mode_t::input;
  } else {
    const version_t version = [&]() {
      try {
        return version_t::parse(spec);
      } catch (const error &) {
        ASDF_ERROR("Unknown standard version \"" + spec +
                   "\"; expected \"minimal\", \"latest\", \"input\", or "
                   "\"X.Y.Z\"");
      }
    }();
    // Rejects a well-formed version this library does not know
    standard_info(version);
    options.version_mode = write_options::version_mode_t::explicit_version;
    options.explicit_version = version;
  }
}

reader_state::reader_state(const YAML::Node &tree,
                           const shared_ptr<istream> &pis,
                           const string &filename, const file_header &header)
    : tree(tree), filename(filename), header(header) {
  for (;;) {
    const auto [block, block_info] = ndarray::read_block(pis);
    if (!block.valid())
      break;
    blocks.push_back(std::move(block));
    block_infos.push_back(std::move(block_info));
  }
}

block_info_t reader_state::get_block_info(int64_t index) const {
  ASDF_CHECK(index >= 0 && size_t(index) < block_infos.size(),
             "Block index " + std::to_string(index) +
                 " is out of range; the file has " +
                 std::to_string(block_infos.size()) + " blocks");
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
          ASDF_ERROR("Reference path element \"" + elem +
                     "\" is not a valid sequence index");
        }
      }();
      node = unique_ptr<YAML::Node>(new YAML::Node((*node)[idx]));
    } else if (node->IsMap()) {
      node = unique_ptr<YAML::Node>(new YAML::Node((*node)[elem]));
    } else {
      // Could not resolve reference
      ASDF_ERROR("Reference path element \"" + elem +
                 "\" cannot be applied to a scalar node");
    }
    ASDF_CHECK(node->IsDefined(),
               "Reference path element \"" + elem + "\" not found");
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
      ASDF_CHECK(!rs->filename.empty(),
                 "Cannot resolve the relative reference \"" + filename +
                     "\" because the current file's name is unknown");
      auto slashpos = rs->filename.rfind('/');
      if (slashpos == string::npos)
        ref_filename = filename;
      else
        ref_filename = rs->filename.substr(0, slashpos + 1) + filename;
    }
    if (!rs->other_files.count(ref_filename)) {
      auto pis = make_shared<ifstream>(ref_filename, ios::binary | ios::in);
      ASDF_CHECK(pis->good(), "Cannot open the external file \"" +
                                  ref_filename + "\" referenced from \"" +
                                  rs->filename + "\"");
      file_header ref_header;
      auto doc = asdf::from_yaml((istream &)*pis, ref_header);
      rs->other_files[ref_filename] =
          make_shared<reader_state>(doc, pis, ref_filename, ref_header);
    }
    refrs = rs->other_files.at(ref_filename);
  }

  auto node = refrs->resolve_reference(path);
  return make_pair(refrs, node);
}

// The standard's examples put the root tag on the document start marker
// (`--- !core/asdf-1.1.0`). yaml-cpp's `BeginDoc` always writes `---\n`, so
// the writer emits the marker itself and the first thing the emitter writes
// is the root tag.
writer::writer(ostream &os, const map<string, string> &tags,
               const standard_info_t &standard, const bool allow_nonstandard)
    : os(os), emitter(os), standard_(&standard),
      allow_nonstandard_(allow_nonstandard) {
  // yaml-cpp does not support comments without leading space
  os << "#ASDF " << asdf_format_version << "\n"
     << "#ASDF_STANDARD " << standard.version.str() << "\n"
     << "# This is an ASDF file <https://asdf-standard.readthedocs.io/>\n"
     // yaml-cpp does not support writing a YAML tag
     << "%YAML 1.1\n"
     << "%TAG ! " << asdf_tag_prefix << "\n";
  for (const auto &kv : tags)
    os << "%TAG !" << kv.first << "! " << kv.second << "\n";
  os << "--- ";
}

writer::~writer() { assert(tasks.empty()); }

void writer::flush() {
  // Take the tasks out first so that the destructor's invariant holds even if
  // a task, or the emitter check below, throws
  const auto tasks1 = std::move(tasks);
  tasks.clear();
  emitter << YAML::EndDoc;
  // yaml-cpp reports emitter errors only through this flag; without the check
  // a malformed emission would silently truncate the file
  ASDF_CHECK(emitter.good(), "YAML emitter error: " + emitter.GetLastError());
  if (!tasks1.empty()) {
    YAML::Emitter index;
    index << YAML::BeginDoc << YAML::Flow << YAML::BeginSeq;
    for (const auto &task : tasks1) {
      index << os.tellp();
      task(os);
    }
    index << YAML::EndSeq << YAML::EndDoc;
    // yaml-cpp does not support comments without leading space
    os << "#ASDF BLOCK INDEX\n"
       // yaml-cpp does not support writing a YAML tag
       << "%YAML 1.1\n"
       << index.c_str();
  }
}

} // namespace ASDF
