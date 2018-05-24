#include "asdf_asdf.hpp"

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

asdf::asdf(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/asdf-1.0.0" ||
         node.Tag() == "tag:stsci.edu:asdf/core/asdf-1.1.0");
  // TODO: read software
  for (const auto &kv : node) {
    const auto &key = kv.first.Scalar();
    if (key == "asdf_library") {
      // TODO
    } else if (key == "group") {
      grp = make_shared<group>(rs, node["group"]);
    } else if (key == "history") {
      // TODO
      // } else if (key == "table") {
      //   tab = make_shared<table>(rs, node["table"]);
    } else {
      data[key] = make_shared<ndarray>(rs, kv.second);
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

writer_state &asdf::to_yaml(writer_state &ws) const {
  ws << YAML::VerbatimTag("tag:stsci.edu:asdf/core/asdf-1.1.0");
  ws << YAML::BeginMap;
  ws << YAML::Key << "asdf_library" << YAML::Value
     << software("asdf-cxx", "Erik Schnetter",
                 "https://github.com/eschnett/asdf-cxx", ASDF_VERSION);
  for (const auto &kv : data)
    ws << YAML::Key << kv.first << YAML::Value << *kv.second;
  // if (tab)
  //   node["table"] = tab->to_yaml(ws);
  if (grp)
    ws << YAML::Key << "group" << YAML::Value << *grp;
  for (const auto &kv : nodes)
    ws << YAML::Key << kv.first << YAML::Value << kv.second;
  // if (writer)
  //   for (const auto &kv : writer(ws))
  //     node[kv.first] = kv.second;
  ws << YAML::EndMap;
  return ws;
}

asdf::asdf(istream &is) {
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
  auto project = asdf(rs, node);
  data = move(project.data);
  // tab = move(project.tab);
  grp = move(project.grp);
}

asdf asdf::copy(const copy_state &cs) const { return asdf(cs, *this); }

void asdf::write(ostream &os) const {
  writer_state ws(os);
  ws << *this;
  ws.flush();
}

} // namespace ASDF
