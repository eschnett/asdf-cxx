#include "asdf_group.hpp"

namespace ASDF {

// Group and Entry

entry::entry(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:github.com/eschnett/asdf-cxx/core/entry-1.0.0");
  name = node["name"].Scalar();
  if (node["group"].IsDefined())
    grp = make_shared<group>(rs, node["group"]);
  if (node["data"].IsDefined())
    arr = make_shared<ndarray>(rs, node["data"]);
  assert(bool(grp) + bool(arr) == 1);
  if (node["description"].IsDefined())
    description = node["description"].Scalar();
}

entry::entry(const copy_state &cs, const entry &ent)
    : name(ent.name), description(ent.description) {
  if (ent.grp)
    grp = make_shared<group>(cs, *ent.grp);
  if (ent.arr)
    arr = make_shared<ndarray>(cs, *ent.arr);
}

YAML::Node entry::to_yaml(writer_state &ws) const {
  YAML::Node node;
  node.SetTag("tag:github.com/eschnett/asdf-cxx/core/entry-1.0.0");
  node["name"] = name;
  if (grp)
    node["group"] = grp->to_yaml(ws);
  if (arr)
    node["data"] = arr->to_yaml(ws);
  if (!description.empty())
    node["description"] = description;
  return node;
}

group::group(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:github.com/eschnett/asdf-cxx/core/group-1.0.0");
  for (const auto &ent : node)
    entries[ent.first.Scalar()] = make_shared<entry>(rs, ent.second);
}

group::group(const copy_state &cs, const group &grp) {
  for (const auto &kv : grp.entries)
    entries[kv.first] = make_shared<entry>(cs, *kv.second);
}

YAML::Node group::to_yaml(writer_state &ws) const {
  YAML::Node node;
  node.SetTag("tag:github.com/eschnett/asdf-cxx/core/group-1.0.0");
  for (const auto &kv : entries)
    node[kv.first] = kv.second->to_yaml(ws);
  return node;
}

} // namespace ASDF
