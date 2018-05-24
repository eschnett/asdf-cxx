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

writer_state &entry::to_yaml(writer_state &ws) const {
  ws << YAML::VerbatimTag("tag:github.com/eschnett/asdf-cxx/core/entry-1.0.0");
  ws << YAML::BeginMap;
  ws << YAML::Key << "name" << YAML::Value << name;
  if (grp)
    ws << YAML::Key << "group" << YAML::Value << *grp;
  if (arr)
    ws << YAML::Key << "data" << YAML::Value << *arr;
  if (!description.empty())
    ws << YAML::Key << "description" << YAML::Value << description;
  ws << YAML::EndMap;
  return ws;
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

writer_state &group::to_yaml(writer_state &ws) const {
  ws << YAML::VerbatimTag("tag:github.com/eschnett/asdf-cxx/core/group-1.0.0");
  ws << YAML::BeginMap;
  for (const auto &kv : entries)
    ws << YAML::Key << kv.first << YAML::Value << *kv.second;
  ws << YAML::EndMap;
  return ws;
}

} // namespace ASDF
