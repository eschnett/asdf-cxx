#ifndef ASDF_GROUP_HPP
#define ASDF_GROUP_HPP

#include "asdf_ndarray.hpp"

#include <yaml-cpp/yaml.h>

#include <memory>
#include <string>

namespace ASDF {
using namespace std;

// Group and Entry

class group;

class entry {
  string name;
  shared_ptr<group> grp;
  shared_ptr<ndarray> arr;
  string description;

public:
  entry() = delete;
  entry(const entry &) = default;
  entry(entry &&) = default;
  entry &operator=(const entry &) = default;
  entry &operator=(entry &&) = default;

  entry(const string &name, const shared_ptr<ndarray> &arr,
        const string &description)
      : name(name), arr(arr), description(description) {
    assert(!name.empty());
    assert(arr);
  }

  entry(const string &name, const shared_ptr<group> &grp,
        const string &description)
      : name(name), grp(grp), description(description) {
    assert(!name.empty());
    assert(grp);
  }

  entry(const reader_state &rs, const YAML::Node &node);
  entry(const copy_state &cs, const entry &ent);
  writer &to_yaml(writer &ws) const;
  friend writer &operator<<(writer &ws, const entry &ent) {
    return ent.to_yaml(ws);
  }
};

class group {
  map<string, shared_ptr<entry>> entries;

public:
  group() = default;
  group(const group &) = default;
  group(group &&) = default;
  group &operator=(const group &) = default;
  group &operator=(group &&) = default;

  group(const map<string, shared_ptr<entry>> &entries) : entries(entries) {}
  template <typename T>
  group(const map<string, shared_ptr<T>> &data,
        const function<entry(const T &)> &f) {
    for (const auto &kv : data)
      entries[kv.first] = f(*kv.second);
  }

  group(const reader_state &rs, const YAML::Node &node);
  group(const copy_state &cs, const group &grp);
  writer &to_yaml(writer &ws) const;
  friend writer &operator<<(writer &ws, const group &grp) {
    return grp.to_yaml(ws);
  }
};

} // namespace ASDF

#define ASDF_GROUP_HPP_DONE
#endif // #ifndef ASDF_GROUP_HPP
#ifndef ASDF_GROUP_HPP_DONE
#error "Cyclic include depencency"
#endif
