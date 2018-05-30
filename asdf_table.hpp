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

  column(string name1, shared_ptr<ndarray> data1, string description1)
      : name(move(name1)), data(move(data1)), description(move(description1)) {
    assert(!name.empty());
    assert(data);
  }

  column(const reader_state &rs, const YAML::Node &node);
  column(const copy_state &cs, const column &col);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const column &col) {
    return col.to_yaml(w);
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

  table(vector<shared_ptr<column>> columns1) : columns(move(columns1)) {}

  table(const reader_state &rs, const YAML::Node &node);
  table(const copy_state &cs, const table &tab);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const table &tab) {
    return tab.to_yaml(w);
  }
};

} // namespace ASDF

#define ASDF_TABLE_HPP_DONE
#endif // #ifndef ASDF_TABLE_HPP
#ifndef ASDF_TABLE_HPP_DONE
#error "Cyclic include depencency"
#endif
