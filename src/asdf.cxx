#include <asdf/asdf.hxx>

#include <array>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace ASDF {

// ASDF

asdf::asdf(const shared_ptr<reader_state> &rs, const YAML::Node &node,
           const map<string, reader_t> &readers) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/asdf-1.0.0" ||
         node.Tag() == "tag:stsci.edu:asdf/core/asdf-1.1.0");

  assert(readers.empty());
  // if (readers.count(tag))
  //   readers.at(tag)(rs, key, node);

  // TODO: data, table

  grp = std::make_shared<group>(rs, node);
}

asdf::asdf(const copy_state &cs, const asdf &project) {
  // for (const auto &kv : project.data) {
  //   const auto &key = kv.first;
  //   data[key] = make_shared<ndarray>(cs, *kv.second);
  // }
  // if (project.tab)
  //   tab = make_shared<table>(cs, *project.tab);
  if (project.grp)
    grp = make_shared<group>(cs, *project.grp);
}

writer &asdf::to_yaml(writer &w) const {
  w << YAML::LocalTag("core/asdf-1.1.0");
  w << YAML::BeginMap;
  w << YAML::Key << "asdf/library" << YAML::Value
    << software(ASDF_NAME, ASDF_AUTHOR, ASDF_HOMEPAGE, ASDF_VERSION);
  // for (const auto &kv : data)
  //   w << YAML::Key << kv.first << YAML::Value << *kv.second;
  // // if (tab)
  // //   node["table"] = tab->to_yaml(w);
  // if (grp)
  //   w << YAML::Key << "group" << YAML::Value << *grp;
  if (grp)
    for (const auto &[key, value] : *grp->get_group())
      if (key != "asdf/library")
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
    cerr << "This is not an ASDF file\n";
    if (is) {
      cerr << "File header should be \"#ASDF\"; found instead \"";
      for (auto ch : header)
        if (ch == '\\' || ch == '"')
          cerr << '\\' << ch;
        else if (isprint(ch))
          cerr << ch;
        else
          cerr << '\\' << oct << setw(3) << setfill('0') << int(ch);
      cerr << "\"\n";
    }
    exit(2);
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
  cerr << "Stream input error\n";
  exit(2);
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
