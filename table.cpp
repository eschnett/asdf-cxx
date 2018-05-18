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

YAML::Node column::to_yaml(writer_state &ws) const {
  YAML::Node node;
  node.SetTag("tag:stsci.edu:asdf/core/column-1.0.0");
  node["name"] = name;
  node["data"] = data->to_yaml(ws);
  if (!description.empty())
    node["description"] = description;
  return node;
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

YAML::Node table::to_yaml(writer_state &ws) const {
  YAML::Node cols;
  for (size_t i = 0; i < columns.size(); ++i)
    cols[i] = columns[i]->to_yaml(ws);
  YAML::Node node;
  node.SetTag("tag:stsci.edu:asdf/core/table-1.0.0");
  node["columns"] = move(cols);
  return node;
}

} // namespace ASDF
