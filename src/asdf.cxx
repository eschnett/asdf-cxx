#include <asdf/asdf.hxx>

#include <array>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ASDF {

// ASDF

asdf::asdf(const shared_ptr<reader_state> &rs, const YAML::Node &node,
           const map<string, reader_t> &readers) {
  const auto &tag = node.Tag();
  ASDF_CHECK(tag == "tag:stsci.edu:asdf/core/asdf-1.0.0" ||
                 tag == "tag:stsci.edu:asdf/core/asdf-1.1.0" ||
                 tag == "tag:stsci.edu:asdf/core/asdf-1.2.0",
             "Unknown root tag \"" + tag +
                 "\"; expected core/asdf-1.0.0, -1.1.0, or -1.2.0");

  ASDF_CHECK(readers.empty(), "Custom readers are not supported");
  // if (readers.count(tag))
  //   readers.at(tag)(rs, key, node);

  // History entries are not supported and are ignored when reading. (Their
  // tagged extension metadata would otherwise be rejected as unknown tags.)
  ASDF_CHECK(node.IsMap(), "The ASDF tree must be a mapping");
  grp = std::make_shared<group>();
  for (const auto &key_value : node) {
    const auto key = key_value.first.Scalar();
    if (key == "history")
      continue;
    grp->insert(key, make_entry(rs, key_value.second));
  }
}

asdf::asdf(const copy_state &cs, const asdf &project) {
  if (project.grp)
    grp = make_shared<group>(cs, *project.grp);
}

writer &asdf::to_yaml(writer &w) const {
  w << YAML::LocalTag("core/asdf-1.1.0");
  w << YAML::BeginMap;
  w << YAML::Key << "asdf_library" << YAML::Value
    << software(ASDF_CXX_NAME, ASDF_CXX_AUTHOR, ASDF_CXX_HOMEPAGE,
                ASDF_CXX_VERSION);
  if (grp)
    for (const auto &[key, value] : *grp->get_group())
      if (key != "asdf_library")
        w << YAML::Key << key << YAML::Value << *value;
  for (const auto &kv : nodes)
    w << YAML::Key << kv.first << YAML::Value << kv.second;
  for (const auto &kv : writers) {
    w << YAML::Key << kv.first << YAML::Value;
    kv.second(w);
  }
  w << YAML::EndMap;
  return w;
}

YAML::Node asdf::from_yaml(istream &is) {
  ostringstream doc;
  const array<unsigned char, 5> magic{'#', 'A', 'S', 'D', 'F'};
  array<unsigned char, 5> header;
  is.read(reinterpret_cast<char *>(header.data()), header.size());
  if (!is || header != magic) {
    ostringstream msg;
    msg << "This is not an ASDF file";
    if (is) {
      msg << ": the file header should be \"#ASDF\"; found instead \"";
      for (auto ch : header)
        if (ch == '\\' || ch == '"')
          msg << '\\' << ch;
        else if (isprint(ch))
          msg << ch;
        else
          msg << '\\' << oct << setw(3) << setfill('0') << int(ch);
      msg << "\"";
    }
    ASDF_ERROR(msg.str());
  }
  for (auto ch : header)
    doc << ch;
  // TODO: Check format version

  // TODO: stream the file instead
  while (is) {
    string line;
    getline(is, line);
    doc << line << "\n";
    if (line == "...")
      return YAML::Load(doc.str());
  }
  ASDF_ERROR("Unexpected end of file while reading the YAML tree (missing "
             "\"...\" terminator)");
}

asdf::asdf(const shared_ptr<istream> &pis, const string &filename,
           const map<string, reader_t> &readers) {
  auto node = from_yaml(*pis);
  auto rs = make_shared<reader_state>(node, pis, filename);
  *this = asdf(rs, node, readers);
}

asdf::asdf(const string &filename, const map<string, reader_t> &readers)
    : asdf(make_shared<ifstream>(filename, ios::binary | ios::in), filename,
           readers) {}

asdf asdf::copy(const copy_state &cs) const { return asdf(cs, *this); }

void asdf::write(ostream &os) const {
  writer w(os, tags);
  w << *this;
  w.flush();
}

void asdf::write(const string &filename) const {
  ofstream os(filename, ios::binary | ios::trunc | ios::out);
  write(os);
}

} // namespace ASDF
