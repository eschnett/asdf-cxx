#ifndef ASDF_TABLE_HPP
#define ASDF_TABLE_HPP

#include "asdf_ndarray.hpp"

#include <yaml-cpp/yaml.h>

#include <memory>
#include <string>

namespace ASDF {
using namespace std;

// Table and Column

class column {
  string name;
  shared_ptr<ndarray> data;
  string description;

public:
  column() = delete;
  column(const column &) = default;
  column(column &&) = default;
  column &operator=(const column &) = default;
  column &operator=(column &&) = default;

  column(const string &name, const shared_ptr<ndarray> &data,
         const string &description)
      : name(name), data(data), description(description) {
    assert(!name.empty());
    assert(data);
  }

  column(const reader_state &rs, const YAML::Node &node);
  column(const copy_state &cs, const column &col);
  writer_state &to_yaml(writer_state &ws) const;
  friend writer_state &operator<<(writer_state &ws, const column &col) {
    return col.to_yaml(ws);
  }
};

class table {
  vector<shared_ptr<column>> columns;

public:
  table() = delete;
  table(const table &) = default;
  table(table &&) = default;
  table &operator=(const table &) = default;
  table &operator=(table &&) = default;

  table(const vector<shared_ptr<column>> &columns) : columns(columns) {}

  table(const reader_state &rs, const YAML::Node &node);
  table(const copy_state &cs, const table &tab);
  writer_state &to_yaml(writer_state &ws) const;
  friend writer_state &operator<<(writer_state &ws, const table &tab) {
    return tab.to_yaml(ws);
  }
};

} // namespace ASDF

#define ASDF_TABLE_HPP_DONE
#endif // #ifndef ASDF_TABLE_HPP
#ifndef ASDF_TABLE_HPP_DONE
#error "Cyclic include depencency"
#endif
