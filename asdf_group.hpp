#ifndef ASDF_GROUP_HPP
#define ASDF_GROUP_HPP

#include "asdf_ndarray.hpp"
#include "asdf_reference.hpp"

#include <yaml-cpp/yaml.h>

#include <memory>
#include <string>

namespace ASDF {
using namespace std;

// Group and Entry

// enum class entry_type { array, reference, sequence, group };

class sequence;
class group;

class entry {
  string name;
  shared_ptr<ndarray> arr;
  shared_ptr<reference> ref;
  shared_ptr<sequence> seq;
  shared_ptr<group> grp;
  string description;

public:
  entry() = delete;
  entry(const entry &) = default;
  entry(entry &&) = default;
  entry &operator=(const entry &) = default;
  entry &operator=(entry &&) = default;

  entry(string name1, shared_ptr<ndarray> arr1, string description1)
      : name(std::move(name1)), arr(std::move(arr1)),
        description(std::move(description1)) {
    assert(!name.empty());
    assert(arr);
  }

  entry(string name1, shared_ptr<reference> ref1, string description1)
      : name(std::move(name1)), ref(std::move(ref1)),
        description(std::move(description1)) {
    assert(!name.empty());
    assert(ref);
  }

  entry(string name1, shared_ptr<sequence> seq1, string description1)
      : name(std::move(name1)), seq(std::move(seq1)),
        description(std::move(description1)) {
    assert(!name.empty());
    assert(seq);
  }

  entry(string name1, shared_ptr<group> grp1, string description1)
      : name(std::move(name1)), grp(std::move(grp1)),
        description(std::move(description1)) {
    assert(!name.empty());
    assert(grp);
  }

  entry(const shared_ptr<reader_state> &rs, const YAML::Node &node);
  entry(const copy_state &cs, const entry &ent);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const entry &ent) {
    return ent.to_yaml(w);
  }

  string get_name() const { return name; }
  shared_ptr<ndarray> get_array() const { return arr; }
  shared_ptr<reference> get_reference() const { return ref; }
  shared_ptr<sequence> get_sequence() const { return seq; }
  shared_ptr<group> get_group() const { return grp; }
  string get_description() const { return description; }
};

class sequence {
  vector<shared_ptr<entry>> entries;

public:
  sequence() = default;
  sequence(const sequence &) = default;
  sequence(sequence &&) = default;
  sequence &operator=(const sequence &) = default;
  sequence &operator=(sequence &&) = default;

  sequence(vector<shared_ptr<entry>> entries1) : entries(std::move(entries1)) {}
  template <typename T>
  sequence(const vector<shared_ptr<T>> &data,
           const function<entry(const T &)> &f) {
    for (const auto &v : data)
      entries.push_back(f(*v));
  }

  sequence(const shared_ptr<reader_state> &rs, const YAML::Node &node);
  sequence(const copy_state &cs, const sequence &seq);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const sequence &seq) {
    return seq.to_yaml(w);
  }

  const vector<shared_ptr<entry>> &get_entries() const { return entries; }
};

class group {
  map<string, shared_ptr<entry>> entries;

public:
  group() = default;
  group(const group &) = default;
  group(group &&) = default;
  group &operator=(const group &) = default;
  group &operator=(group &&) = default;

  group(map<string, shared_ptr<entry>> entries1)
      : entries(std::move(entries1)) {}
  template <typename T>
  group(const map<string, shared_ptr<T>> &data,
        const function<entry(const T &)> &f) {
    for (const auto &kv : data)
      entries[kv.first] = f(*kv.second);
  }

  group(const shared_ptr<reader_state> &rs, const YAML::Node &node);
  group(const copy_state &cs, const group &grp);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const group &grp) {
    return grp.to_yaml(w);
  }

  const map<string, shared_ptr<entry>> &get_entries() const { return entries; }
};

} // namespace ASDF

#define ASDF_GROUP_HPP_DONE
#endif // #ifndef ASDF_GROUP_HPP
#ifndef ASDF_GROUP_HPP_DONE
#error "Cyclic include depencency"
#endif
