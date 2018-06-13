#include "asdf_io.hpp"
#include "asdf_reference.hpp"

#include <cassert>
#include <exception>
#include <iomanip>
#include <memory>
#include <sstream>

namespace ASDF {

// Reference

namespace {
string tilde_decode(const string &cooked) {
  ostringstream buf;
  const size_t len = cooked.size();
  size_t pos = 0;
  while (pos < len) {
    unsigned char ch = cooked[pos++];
    switch (ch) {
    case '~': {
      assert(pos < len);
      unsigned char ch2 = cooked[pos++];
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
      break;
    }
    case '/':
      assert(0);
    default:
      buf << ch;
      break;
    }
  }
  return buf.str();
}

string tilde_encode(const string &raw) {
  ostringstream buf;
  for (unsigned char ch : raw) {
    switch (ch) {
    case '/':
      buf << "~1";
      break;
    case '~':
      buf << "~0";
      break;
    default:
      buf << ch;
      break;
    }
  }
  auto cooked = buf.str();
  assert(tilde_decode(cooked) == raw);
  return cooked;
}

string fragment_percent_decode(const string &cooked) {
  ostringstream buf;
  const size_t len = cooked.size();
  size_t pos = 0;
  while (pos < len) {
    unsigned char ch = cooked[pos++];
    switch (ch) {
    case '%': {
      assert(pos + 2 <= len);
      istringstream digs(cooked.substr(pos, 2));
      pos += 2;
      unsigned int ch2;
      digs >> hex >> ch2;
      buf << (unsigned char)ch2;
      break;
    }
    default:
      buf << ch;
      break;
    }
  }
  return buf.str();
}

string fragment_percent_encode(const string &raw) {
  ostringstream buf;
  for (unsigned char ch : raw) {
    bool isallowed = isalpha(ch) || isdigit(ch);
    switch (ch) {
    // unreserved
    case '-':
    case '.':
    case '_':
    case '~':
    // sub-delims
    case '!':
    case '$':
    case '&':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
    case ';':
    case '=':
    // pchar
    case ':':
    case '@':
    // fragment
    case '/':
    case '?':
      isallowed = true;
    }
    if (isallowed)
      buf << ch;
    else
      buf << '%' << uppercase << hex << setw(2) << setfill('0') << int(ch);
  }
  auto cooked = buf.str();
  assert(fragment_percent_decode(cooked) == raw);
  return cooked;
}

} // namespace

reference::reference(string target1) : target(move(target1)) {}

reference::reference(const string &base_target,
                     const vector<string> &doc_path) {
  ostringstream fragment;
  for (const auto &elem : doc_path)
    fragment << '/' << tilde_encode(elem);
  ostringstream buf;
  buf << base_target << '#' << fragment_percent_encode(fragment.str());
  target = buf.str();
}

pair<string, vector<string>> reference::get_split_target() const {
  auto hashpos = target.find('#');
  if (hashpos == string::npos)
    return {target, {}};
  auto base_target = target.substr(0, hashpos);
  auto fragment = fragment_percent_decode(target.substr(hashpos + 1));
  vector<string> doc_path;
  for (;;) {
    auto slashpos = fragment.find('/');
    if (slashpos == string::npos) {
      doc_path.push_back(tilde_decode(fragment));
      break;
    }
    doc_path.push_back(tilde_decode(fragment.substr(0, slashpos)));
    fragment = fragment.substr(slashpos + 1);
  }
  // The fragment should either be empty, or should begin with a slash. In both
  // cases, the first element of doc_path should have length zero.
  assert(doc_path.size() > 0);
  assert(doc_path.at(0).size() == 0);
  doc_path.erase(doc_path.begin());
  return {move(base_target), move(doc_path)};
}

reference::reference(const shared_ptr<reader_state> &rs, const YAML::Node &node)
    : rs(rs) {
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
  // We need to double-quote the string to make Python's YAML accept it;
  // Python's YAML parser does not accept plain strings that contain colons ":"
  w << YAML::Key << "$ref" << YAML::Value << YAML::DoubleQuoted << target;
  w << YAML::EndMap;
  return w;
}

pair<shared_ptr<reader_state>, YAML::Node> reference::resolve() const {
  const auto &tgt = get_split_target();
  const auto &docname = tgt.first;
  const auto &path = tgt.second;
  return reader_state::resolve_reference(rs, docname, path);
}

} // namespace ASDF
