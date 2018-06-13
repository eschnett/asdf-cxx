#include "asdf_group.hpp"

namespace ASDF {

// Group and Entry

entry::entry(const shared_ptr<reader_state> &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:github.com/eschnett/asdf-cxx/core/entry-1.0.0");
  name = node["name"].Scalar();
  if (node["data"].IsDefined())
    arr = make_shared<ndarray>(rs, node["data"]);
  if (node["reference"].IsDefined())
    ref = make_shared<reference>(rs, node["reference"]);
  if (node["sequence"].IsDefined())
    seq = make_shared<sequence>(rs, node["sequence"]);
  if (node["group"].IsDefined())
    grp = make_shared<group>(rs, node["group"]);
  assert(bool(arr) + bool(ref) + bool(seq) + bool(grp) == 1);
  if (node["description"].IsDefined())
    description = node["description"].Scalar();
}

entry::entry(const copy_state &cs, const entry &ent)
    : name(ent.name), description(ent.description) {
  if (ent.arr)
    arr = make_shared<ndarray>(cs, *ent.arr);
  if (ent.ref)
    ref = make_shared<reference>(cs, *ent.ref);
  if (ent.seq)
    seq = make_shared<sequence>(cs, *ent.seq);
  if (ent.grp)
    grp = make_shared<group>(cs, *ent.grp);
}

writer &entry::to_yaml(writer &w) const {
  w << YAML::LocalTag("asdf-cxx", "core/entry-1.0.0");
  w << YAML::BeginMap;
  w << YAML::Key << "name" << YAML::Value << name;
  if (arr)
    w << YAML::Key << "data" << YAML::Value << *arr;
  if (ref)
    w << YAML::Key << "reference" << YAML::Value << *ref;
  if (seq)
    w << YAML::Key << "sequence" << YAML::Value << *seq;
  if (grp)
    w << YAML::Key << "group" << YAML::Value << *grp;
  if (!description.empty())
    w << YAML::Key << "description" << YAML::Value << description;
  w << YAML::EndMap;
  return w;
}

sequence::sequence(const shared_ptr<reader_state> &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:github.com/eschnett/asdf-cxx/core/sequence-1.0.0");
  for (const auto &ent : node)
    entries.push_back(make_shared<entry>(rs, ent));
}

sequence::sequence(const copy_state &cs, const sequence &seq) {
  for (const auto &v : seq.entries)
    entries.push_back(make_shared<entry>(cs, *v));
}

writer &sequence::to_yaml(writer &w) const {
  w << YAML::LocalTag("asdf-cxx", "core/sequence-1.0.0");
  w << YAML::BeginSeq;
  for (const auto &v : entries)
    w << *v;
  w << YAML::EndSeq;
  return w;
}

group::group(const shared_ptr<reader_state> &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:github.com/eschnett/asdf-cxx/core/group-1.0.0");
  for (const auto &ent : node)
    entries[ent.first.Scalar()] = make_shared<entry>(rs, ent.second);
}

group::group(const copy_state &cs, const group &grp) {
  for (const auto &kv : grp.entries)
    entries[kv.first] = make_shared<entry>(cs, *kv.second);
}

writer &group::to_yaml(writer &w) const {
  w << YAML::LocalTag("asdf-cxx", "core/group-1.0.0");
  w << YAML::BeginMap;
  for (const auto &kv : entries)
    w << YAML::Key << kv.first << YAML::Value << *kv.second;
  w << YAML::EndMap;
  return w;
}

} // namespace ASDF
