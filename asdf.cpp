#include "asdf_asdf.hpp"

namespace ASDF {

// ASDF

const string asdf_format_version = "1.0.0";
const string asdf_standard_version = "1.1.0";

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

YAML::Node asdf::to_yaml(writer_state &ws) const {
  const auto &asdf_library =
      software("asdf-cxx", "Erik Schnetter",
               "https://github.com/eschnett/asdf-cxx", ASDF_VERSION);
  YAML::Node node;
  node.SetTag("tag:stsci.edu:asdf/core/asdf-1.1.0");
  node["asdf_library"] = asdf_library;
  for (const auto &kv : data) {
    const auto &key = kv.first;
    node[key] = kv.second->to_yaml(ws);
  }
  // if (tab)
  //   node["table"] = tab->to_yaml(ws);
  if (grp)
    node["group"] = grp->to_yaml(ws);
  // node.SetStyle(YAML::EmitterStyle::BeginDoc);
  // node.SetStyle(YAML::EmitterStyle::EndDoc);
  return node;
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
  writer_state ws;
  // TODO: Use YAML::Emitter(os) instead
  const auto &node = to_yaml(ws);
  os << "#ASDF " << asdf_format_version << "\n"
     << "#ASDF_STANDARD " << asdf_standard_version << "\n"
     << "# This is an ASDF file <https://asdf-standard.readthedocs.io/>.\n"
     << "%YAML 1.1\n"
     // << "%TAG ! tag:stsci.edu:asdf/\n"
     // << "%TAG !! tag:github.com/eschnett/asdf-cxx/\n"
     << "---\n"
     << node << "\n"
     << "...\n";
  ws.flush(os);
}

} // namespace ASDF
