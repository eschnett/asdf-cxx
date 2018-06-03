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
  string target;

public:
  reference() = delete;
  reference(const reference &) = default;
  reference(reference &&) = default;
  reference &operator=(const reference &) = default;
  reference &operator=(reference &&) = default;

  reference(string target1);
  reference(const string &base_target, const vector<string> &doc_path);
  string get_target() const { return target; }
  pair<string, vector<string>> get_split_target() const;

  reference(const reader_state &rs, const YAML::Node &node);
  reference(const copy_state &cs, const reference &ref);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const reference &ref) {
    return ref.to_yaml(w);
  }
};

YAML::Node resolve_reference(const YAML::Node &doc,
                             const vector<string> &doc_path);
YAML::Node resolve_reference(const reader_state &rs,
                             const vector<string> &doc_path);

} // namespace ASDF

#define ASDF_REFERENCE_HPP_DONE
#endif // #ifndef ASDF_REFERENCE_HPP
#ifndef ASDF_REFERENCE_HPP_DONE
#error "Cyclic include depencency"
#endif
