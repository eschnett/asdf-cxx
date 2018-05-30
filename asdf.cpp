#include "asdf_asdf.hpp"

#include <cstdlib>
#include <iostream>

namespace ASDF {

// ASDF

YAML::Node software(const string &name, const string &author,
                    const string &homepage, const string &version) {
  YAML::Node node;
  node.SetTag("tag:stsci.edu:asdf/core/software-1.0.0");
  assert(!name.empty());
  node[name] = name;
  if (!author.empty())
    node["author"] = author;
  if (!homepage.empty())
    node["homepage"] = homepage;
  assert(!version.empty());
  node["version"] = version;
  return node;
}

asdf::asdf(
    const reader_state &rs, const YAML::Node &node,
    const map<string, function<void(const reader_state &rs, const string &name,
                                    const YAML::Node &node)>> &readers) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/asdf-1.0.0" ||
         node.Tag() == "tag:stsci.edu:asdf/core/asdf-1.1.0");
  for (const auto &kv : node) {
    const auto &key = kv.first.Scalar();
    const auto &node = kv.second;
    const auto &tag = node.Tag();
    if (tag == "tag:stsci.edu:asdf/core/software-1.0.0") {
      // TODO
    } else if (tag == "tag:stsci.edu:asdf/core/history_entry-1.0.0") {
      // TODO
    } else if (tag == "tag:github.com/eschnett/asdf-cxx/core/group-1.0.0") {
      grp = make_shared<group>(rs, node);
      // } else if (key == "table") {
      //   tab = make_shared<table>(rs, node["table"]);
    } else if (tag == "tag:stsci.edu:asdf/core/ndarray-1.0.0") {
      data[key] = make_shared<ndarray>(rs, node);
    } else if (readers.count(tag)) {
      readers.at(tag)(rs, key, node);
    } else {
      cerr << "No handler for tag <" << tag << ">\n";
      if (readers.empty()) {
        cerr << "There are no known tags.\n";
      } else {
        cerr << "Known tags are:\n";
        for (const auto &kv : readers)
          cerr << "  <" << kv.first << ">\n";
      }
      exit(2);
    }
  }
}

asdf::asdf(const copy_state &cs, const asdf &project) {
  for (const auto &kv : project.data) {
    const auto &key = kv.first;
    data[key] = make_shared<ndarray>(cs, *kv.second);
  }
  // if (project.tab)
  //   tab = make_shared<table>(cs, *project.tab);
  if (project.grp)
    grp = make_shared<group>(cs, *project.grp);
}

writer &asdf::to_yaml(writer &w) const {
  w << YAML::LocalTag("core/asdf-1.1.0");
  w << YAML::BeginMap;
  w << YAML::Key << "asdf_library" << YAML::Value
    << software("asdf-cxx", "Erik Schnetter",
                "https://github.com/eschnett/asdf-cxx", ASDF_VERSION);
  for (const auto &kv : data)
    w << YAML::Key << kv.first << YAML::Value << *kv.second;
  // if (tab)
  //   node["table"] = tab->to_yaml(w);
  if (grp)
    w << YAML::Key << "group" << YAML::Value << *grp;
  for (const auto &kv : nodes)
    w << YAML::Key << kv.first << YAML::Value << kv.second;
  for (const auto &kv : writers) {
    w << YAML::Key << kv.first << YAML::Value;
    kv.second(w);
  }
  w << YAML::EndMap;
  return w;
}

asdf::asdf(
    istream &is,
    const map<string, function<void(const reader_state &rs, const string &name,
                                    const YAML::Node &node)>> &readers) {
  // TODO: stream the file instead
  ostringstream doc;
  for (;;) {
    string line;
    getline(is, line);
    doc << line << "\n";
    if (line == "...")
      break;
  }
  YAML::Node node = YAML::Load(doc.str());
  reader_state rs(is);
  *this = asdf(rs, node, readers);
}

asdf asdf::copy(const copy_state &cs) const { return asdf(cs, *this); }

void asdf::write(ostream &os) const {
  writer w(os, tags);
  w << *this;
  w.flush();
}

} // namespace ASDF
