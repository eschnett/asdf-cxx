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

writer_state &column::to_yaml(writer_state &ws) const {
  ws << YAML::VerbatimTag("tag:stsci.edu:asdf/core/column-1.0.0");
  ws << YAML::BeginMap;
  ws << YAML::Key << "name" << YAML::Value << name;
  ws << YAML::Key << "data" << YAML::Value << *data;
  if (!description.empty())
    ws << YAML::Key << "description" << YAML::Value << description;
  ws << YAML::EndMap;
  return ws;
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

writer_state &table::to_yaml(writer_state &ws) const {
  ws << YAML::VerbatimTag("tag:stsci.edu:asdf/core/table-1.0.0");
  ws << YAML::BeginMap;
  ws << YAML::Key << "columns" << YAML::Value;
  ws << YAML::BeginSeq;
  for (size_t i = 0; i < columns.size(); ++i)
    ws << *columns[i];
  ws << YAML::EndSeq;
  ws << YAML::EndMap;
  return ws;
}

} // namespace ASDF
