#ifndef ASDF_TABLE_HXX
#define ASDF_TABLE_HXX

#include <asdf/ndarray.hxx>

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
      : name(std::move(name1)), data(std::move(data1)),
        description(std::move(description1)) {
    assert(!name.empty());
    assert(data);
  }

  column(const shared_ptr<reader_state> &rs, const YAML::Node &node);
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

  table(vector<shared_ptr<column>> columns1) : columns(std::move(columns1)) {}

  table(const shared_ptr<reader_state> &rs, const YAML::Node &node);
  table(const copy_state &cs, const table &tab);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const table &tab) {
    return tab.to_yaml(w);
  }
};

} // namespace ASDF

#define ASDF_TABLE_HXX_DONE
#endif // #ifndef ASDF_TABLE_HXX
#ifndef ASDF_TABLE_HXX_DONE
#error "Cyclic include depencency"
#endif
