#include "asdf_reference.hpp"

#include <cassert>
#include <exception>
#include <memory>
#include <sstream>

namespace ASDF {

// Reference

reference::reference(string target1) : target(move(target1)) {}

reference::reference(const string &base_target,
                     const vector<string> &doc_path) {
  ostringstream buf;
  buf << base_target << "#";
  for (const auto &elem : doc_path) {
    buf << "/";
    for (char ch : elem) {
      switch (ch) {
      case '~':
        buf << "~0";
        break;
      case '/':
        buf << "~1";
        break;
      default:
        buf << ch;
        break;
      }
    }
  }
  // TODO: percent-encode URI
  target = buf.str();
}

pair<string, vector<string>> reference::get_split_target() const {
  auto hashpos = target.find('#');
  if (hashpos == string::npos)
    return {target, {}};
  auto base_target = target.substr(0, hashpos);
  auto fragment = target.substr(hashpos + 1);
  vector<string> doc_path;
  for (;;) {
    auto slashpos = fragment.find('/');
    if (slashpos == string::npos) {
      doc_path.push_back(fragment);
      break;
    }
    doc_path.push_back(fragment.substr(0, slashpos));
    fragment = fragment.substr(slashpos + 1);
  }
  // The fragment should either be empty, or should begin with a slash. In both
  // cases, the first element of doc_path should have length zero.
  assert(doc_path.size() > 0);
  assert(doc_path.at(0).size() == 0);
  doc_path.erase(doc_path.begin());
  for (auto &elem : doc_path) {
    ostringstream buf;
    const size_t len = elem.size();
    size_t pos = 0;
    while (pos < len) {
      char ch = elem[pos++];
      if (ch == '~') {
        assert(pos < len);
        char ch2 = elem[pos++];
        switch (ch2) {
        case '0':
          buf << '/';
          break;
        case '1':
          buf << '~';
          break;
        default:
          assert(0);
        }
      } else {
        buf << ch;
      }
    }
    elem = buf.str();
  }
  return {move(base_target), move(doc_path)};
}

reference::reference(const reader_state &rs, const YAML::Node &node) {
  // assert(node.Tag() == "tag:stsci.edu:asdf/core/reference-1.0.0");
  assert(node.IsMap());
  assert(node.size() == 1);
  target = node["$ref"].Scalar();
}

reference::reference(const copy_state &cs, const reference &ref) {
  target = ref.target;
}

writer &reference::to_yaml(writer &w) const {
  // w << YAML::LocalTag("core/reference-1.0.0");
  w << YAML::Flow << YAML::BeginMap;
  w << YAML::Key << "$ref" << YAML::Value << target;
  w << YAML::EndMap;
  return w;
}

YAML::Node resolve_reference(const YAML::Node &doc,
                             const vector<string> &doc_path) {
  // We allocate a new YAML node each time we take a step. If we don't
  // do this, yaml-cpp will instead only create a reference (alias) to
  // the new node, thus effectively overwriting the "doc" argument.
  auto node = unique_ptr<YAML::Node>(new YAML::Node(doc));
  assert(node->IsDefined());
  for (const auto &elem : doc_path) {
    if (node->IsSequence()) {
      try {
        int idx = stoi(elem);
        node = unique_ptr<YAML::Node>(new YAML::Node((*node)[idx]));
      } catch (exception &) {
        assert(0);
      }
    } else if (node->IsMap()) {
      node = unique_ptr<YAML::Node>(new YAML::Node((*node)[elem]));
    } else {
      assert(0);
    }
    assert(node->IsDefined());
  }
  return *node;
}

YAML::Node resolve_reference(const reader_state &rs,
                             const vector<string> &doc_path) {
  return resolve_reference(rs.doc, doc_path);
}

} // namespace ASDF
