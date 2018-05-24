#include "asdf_table.hpp"

namespace ASDF {

// Table and Column

column::column(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/column-1.0.0");
  name = node["name"].Scalar();
  data = make_shared<ndarray>(rs, node["data"]);
  if (node["description"].IsDefined())
    description = node["description"].Scalar();
}

column::column(const copy_state &cs, const column &col) : column(col) {}

writer &column::to_yaml(writer &w) const {
  w << YAML::VerbatimTag("tag:stsci.edu:asdf/core/column-1.0.0");
  w << YAML::BeginMap;
  w << YAML::Key << "name" << YAML::Value << name;
  w << YAML::Key << "data" << YAML::Value << *data;
  if (!description.empty())
    w << YAML::Key << "description" << YAML::Value << description;
  w << YAML::EndMap;
  return w;
}

table::table(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/table-1.0.0");
  for (const auto &col : node["columns"])
    columns.push_back(make_shared<column>(rs, col));
}

table::table(const copy_state &cs, const table &tab) {
  for (const auto &col : tab.columns)
    columns.push_back(make_shared<column>(cs, *col));
}

writer &table::to_yaml(writer &w) const {
  w << YAML::VerbatimTag("tag:stsci.edu:asdf/core/table-1.0.0");
  w << YAML::BeginMap;
  w << YAML::Key << "columns" << YAML::Value;
  w << YAML::BeginSeq;
  for (size_t i = 0; i < columns.size(); ++i)
    w << *columns[i];
  w << YAML::EndSeq;
  w << YAML::EndMap;
  return w;
}

} // namespace ASDF
