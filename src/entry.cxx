#include <asdf/entry.hxx>

#include <asdf/datatype.hxx>

#include <cctype>
#include <optional>
#include <sstream>

namespace ASDF {

namespace {
template <typename T> std::optional<T> try_parse_yaml(const YAML::Node &node) {
  try {
    return node.as<T>();
  } catch (const YAML::BadConversion &e) {
    return std::optional<T>();
  }
}
} // namespace

////////////////////////////////////////////////////////////////////////////////

std::ostream &operator<<(std::ostream &os, entry_type_t entry_type) {
  switch (entry_type) {
  case entry_type_t::null:
    return os << "null";
  case entry_type_t::bool8:
    return os << "bool8";
  case entry_type_t::int64:
    return os << "int64";
  case entry_type_t::float64:
    return os << "float64";
  case entry_type_t::complex128:
    return os << "complex128";
  case entry_type_t::string:
    return os << "string";
  case entry_type_t::software:
    return os << "software";
  case entry_type_t::history_entry:
    return os << "history_entry";
  case entry_type_t::ndarray:
    return os << "ndarray";
  case entry_type_t::reference:
    return os << "reference";
  case entry_type_t::sequence:
    return os << "sequence";
  case entry_type_t::group:
    return os << "group";
  default:
    return os << "unknown";
  }
}

writer &null_entry::to_yaml(writer &w) const { return w << YAML::Null; }

writer &bool_entry::to_yaml(writer &w) const { return w << yaml_encode(value); }

writer &int_entry::to_yaml(writer &w) const { return w << yaml_encode(value); }

writer &float_entry::to_yaml(writer &w) const {
  return w << yaml_encode(value);
}

writer &complex_entry::to_yaml(writer &w) const {
  return w << yaml_encode(value);
}

writer &string_entry::to_yaml(writer &w) const {
  return w << YAML::DoubleQuoted << value;
}

software::software(const std::shared_ptr<reader_state> &rs,
                   const YAML::Node node) {
  assert(node.IsMap());
  assert(node.Tag() == "tag:stsci.edu:asdf/core/software-1.0.0");
  name = node["name"].Scalar();
  author = node["author"] ? node["author"].Scalar() : "";
  homepage = node["homepage"] ? node["homepage"].Scalar() : "";
  version = node["version"].Scalar();
}

software::software(const copy_state &cs, const software &soft)
    : name(soft.name), author(soft.author), homepage(soft.homepage),
      version(soft.version) {}

writer &software::to_yaml(writer &w) const {
  w << YAML::LocalTag("core/software-1.0.0") << YAML::BeginMap;
  w << YAML::Key << "name" << YAML::Value << name;
  if (!author.empty())
    w << YAML::Key << "author" << YAML::Value << author;
  if (!homepage.empty())
    w << YAML::Key << "homepage" << YAML::Value << homepage;
  w << YAML::Key << "version" << YAML::Value << version;
  w << YAML::EndMap;
  return w;
}

ndarray_entry::ndarray_entry(const std::shared_ptr<reader_state> &rs,
                             const YAML::Node node)
    : value(std::make_shared<ndarray>(rs, node)) {}
ndarray_entry::ndarray_entry(const copy_state &cs, const ndarray_entry &arr)
    : value(std::make_shared<ndarray>(cs, *arr.value)) {}

writer &ndarray_entry::to_yaml(writer &w) const { return w << *value; }

reference_entry::reference_entry(const std::shared_ptr<reader_state> &rs,
                                 const YAML::Node node)
    : value(std::make_shared<reference>(rs, node)) {}
reference_entry::reference_entry(const copy_state &cs,
                                 const reference_entry &ref)
    : value(std::make_shared<reference>(cs, *ref.value)) {}

writer &reference_entry::to_yaml(writer &w) const { return w << *value; }

sequence::sequence(const shared_ptr<reader_state> &rs, const YAML::Node &node)
    : sequence() {
  assert(node.IsSequence());
  for (const auto &value : node)
    push_back(make_entry(rs, value));
}

sequence::sequence(const copy_state &cs, const sequence &from) : sequence() {
  for (const auto &value : *from.entries)
    push_back(value->copy(cs));
}

writer &sequence::to_yaml(writer &w) const {
  w << YAML::BeginSeq;
  for (const auto &value : *entries)
    w << *value;
  w << YAML::EndSeq;
  return w;
}

group::group(const shared_ptr<reader_state> &rs, const YAML::Node &node)
    : group() {
  assert(node.IsMap());
  for (const auto &key_value : node)
    insert({key_value.first.Scalar(), make_entry(rs, key_value.second)});
}

group::group(const copy_state &cs, const group &from) : group() {
  for (const auto &[key, value] : *from.entries)
    insert({key, value->copy(cs)});
}

writer &group::to_yaml(writer &w) const {
  w << YAML::BeginMap;
  for (const auto &[key, value] : *entries)
    w << YAML::Key << key << YAML::Value << *value;
  w << YAML::EndMap;
  return w;
}

std::shared_ptr<entry> make_entry(const std::shared_ptr<reader_state> &rs,
                                  const YAML::Node &node) {
  assert(node.IsDefined());

  // First look at the tag. If there is a tag we know what to do.
  const auto tag = node.Tag();

  if (tag == "tag:stsci.edu:asdf/core/complex-1.0.0") {
    std::complex<float64_t> value;
    yaml_decode(node, value);
    return std::make_shared<complex_entry>(value);
  }

  if (tag == "tag:stsci.edu:asdf/core/software-1.0.0")
    return std::make_shared<software>(rs, node);

  // if (tag == "tag:stsci.edu:asdf/core/history_entry-1.0.0")
  //   return std::make_shared<history_entry>(rs, node);

  if (tag == "tag:stsci.edu:asdf/core/ndarray-1.0.0")
    return std::make_shared<ndarray_entry>(std::make_shared<ndarray>(rs, node));

  assert(tag.empty() || tag == "?" || tag == "!");

  // Next look at the node type.
  if (node.IsNull())
    return std::make_shared<null_entry>(std::tuple<>());

  // Scalar nodes can be either boo, int, float, or string. Try in this order.
  if (node.IsScalar()) {
    const auto bool8 = try_parse_yaml<bool>(node);
    if (bool8)
      return std::make_shared<bool_entry>(*bool8);
  }
  if (node.IsScalar()) {
    const auto int64 = try_parse_yaml<int64_t>(node);
    if (int64)
      return std::make_shared<int_entry>(*int64);
  }
  if (node.IsScalar()) {
    const auto float64 = try_parse_yaml<float64_t>(node);
    if (float64)
      return std::make_shared<float_entry>(*float64);
  }
  if (node.IsScalar())
    return std::make_shared<string_entry>(node.Scalar());

  // Sequences are straightforward.
  if (node.IsSequence())
    return std::make_shared<sequence>(rs, node);

  // References are maps with a single key named `$ref`
  if (node.IsMap()) {
    if (node["$ref"])
      return std::make_shared<reference_entry>(rs, node);
    else
      return std::make_shared<group>(rs, node);
  }

  assert(0);
  return NULL; // suppress compiler warning
}

} // namespace ASDF
