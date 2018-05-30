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

  entry(string name1, shared_ptr<ndarray> arr1, string description1)
      : name(move(name1)), arr(move(arr1)), description(move(description1)) {
    assert(!name.empty());
    assert(arr);
  }

  entry(string name1, shared_ptr<group> grp1, string description1)
      : name(move(name1)), grp(move(grp1)), description(move(description1)) {
    assert(!name.empty());
    assert(grp);
  }

  entry(const reader_state &rs, const YAML::Node &node);
  entry(const copy_state &cs, const entry &ent);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const entry &ent) {
    return ent.to_yaml(w);
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

  group(map<string, shared_ptr<entry>> entries1) : entries(move(entries1)) {}
  template <typename T>
  group(const map<string, shared_ptr<T>> &data,
        const function<entry(const T &)> &f) {
    for (const auto &kv : data)
      entries[kv.first] = f(*kv.second);
  }

  group(const reader_state &rs, const YAML::Node &node);
  group(const copy_state &cs, const group &grp);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const group &grp) {
    return grp.to_yaml(w);
  }
};

} // namespace ASDF

#define ASDF_GROUP_HPP_DONE
#endif // #ifndef ASDF_GROUP_HPP
#ifndef ASDF_GROUP_HPP_DONE
#error "Cyclic include depencency"
#endif
