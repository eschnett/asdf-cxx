#ifndef ASDF_REFERENCE_HPP
#define ASDF_REFERENCE_HPP

#include "asdf_io.hpp"

#include <yaml-cpp/yaml.h>

#include <string>
#include <utility>
#include <vector>

namespace ASDF {
using namespace std;

class reference {
  shared_ptr<reader_state> rs;
  string target;

public:
  reference() = delete;
  reference(const reference &) = default;
  reference(reference &&) = default;
  reference &operator=(const reference &) = delete;
  reference &operator=(reference &&) = delete;

  reference(string target1);
  reference(const string &base_target, const vector<string> &doc_path);
  string get_target() const { return target; }
  pair<string, vector<string>> get_split_target() const;

  reference(const shared_ptr<reader_state> &rs, const YAML::Node &node);
  reference(const copy_state &cs, const reference &ref);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const reference &ref) {
    return ref.to_yaml(w);
  }

  pair<shared_ptr<reader_state>, YAML::Node> resolve() const;
};

} // namespace ASDF

#define ASDF_REFERENCE_HPP_DONE
#endif // #ifndef ASDF_REFERENCE_HPP
#ifndef ASDF_REFERENCE_HPP_DONE
#error "Cyclic include depencency"
#endif
