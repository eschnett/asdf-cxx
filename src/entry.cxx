#include <asdf/entry.hxx>

#include <asdf/datatype.hxx>

#include <cctype>
#include <cstdlib>
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

// yaml-cpp follows the YAML 1.1 type repository and reads a bare `y` or `n`
// as a boolean. The reference implementation's parser (PyYAML) does not, and
// neither does any ASDF schema, so a one-letter string such as the axis name
// `y` would silently turn into `true` on a copy.
bool is_bool_spelling(const std::string &text) {
  return !(text.size() == 1 && (text[0] == 'y' || text[0] == 'Y' ||
                                text[0] == 'n' || text[0] == 'N'));
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

writer &null_entry::to_yaml(writer &w) const {
  emit_tag(w, tag_);
  return w << YAML::Null;
}

writer &bool_entry::to_yaml(writer &w) const {
  emit_tag(w, tag_);
  return w << yaml_encode(value);
}

writer &int_entry::to_yaml(writer &w) const {
  emit_tag(w, tag_);
  return w << yaml_encode(value);
}

writer &float_entry::to_yaml(writer &w) const {
  emit_tag(w, tag_);
  return w << yaml_encode(value);
}

writer &complex_entry::to_yaml(writer &w) const {
  // The writer's `complex` overload emits a local tag from the version table;
  // `yaml_encode` would go through a `YAML::Node` and emit it verbatim
  return w << value;
}

writer &string_entry::to_yaml(writer &w) const {
  emit_tag(w, tag_);
  if (plain)
    // The emitter's default style, which quotes only where YAML requires it
    return w << value;
  return w << YAML::DoubleQuoted << value;
}

software::software(const std::shared_ptr<reader_state> &rs,
                   const YAML::Node node) {
  ASDF_CHECK(node.IsMap(), "A core/software entry must be a mapping");
  ASDF_CHECK(classify_core_tag(node.Tag()) == core_tag_t::software,
             "Expected tag core/software-1.0.0, found \"" + node.Tag() + "\"");
  ASDF_CHECK(node["name"].IsScalar(),
             "A core/software entry must have a \"name\"");
  ASDF_CHECK(node["version"].IsScalar(),
             "A core/software entry must have a \"version\"");
  name = node["name"].Scalar();
  author = node["author"] ? node["author"].Scalar() : "";
  homepage = node["homepage"] ? node["homepage"].Scalar() : "";
  version = node["version"].Scalar();
}

software::software(const copy_state &cs, const software &soft)
    : name(soft.name), author(soft.author), homepage(soft.homepage),
      version(soft.version) {
  tag_ = soft.tag_;
}

writer &software::to_yaml(writer &w) const {
  w << YAML::LocalTag(w.standard().software_tag) << YAML::BeginMap;
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
    : value(std::make_shared<ndarray>(cs, *arr.value)) {
  tag_ = arr.tag_;
}

writer &ndarray_entry::to_yaml(writer &w) const { return w << *value; }

reference_entry::reference_entry(const std::shared_ptr<reader_state> &rs,
                                 const YAML::Node node)
    : value(std::make_shared<reference>(rs, node)) {}
reference_entry::reference_entry(const copy_state &cs,
                                 const reference_entry &ref)
    : value(std::make_shared<reference>(cs, *ref.value)) {
  tag_ = ref.tag_;
}

writer &reference_entry::to_yaml(writer &w) const {
  emit_tag(w, tag_);
  return w << *value;
}

sequence::sequence(const shared_ptr<reader_state> &rs, const YAML::Node &node)
    : sequence() {
  ASDF_CHECK(node.IsSequence(), "Expected a YAML sequence");
  for (const auto &value : node)
    push_back(make_entry(rs, value));
}

sequence::sequence(const copy_state &cs, const sequence &from) : sequence() {
  tag_ = from.tag_;
  for (const auto &value : *from.entries)
    push_back(value->copy(cs));
}

void sequence::collect_requirements(content_requirements &req,
                                    const std::string &path) const {
  for (size_t n = 0; n < entries->size(); ++n)
    entries->at(n)->collect_requirements(req, path + "/" + std::to_string(n));
}

writer &sequence::to_yaml(writer &w) const {
  emit_tag(w, tag_);
  w << YAML::BeginSeq;
  for (const auto &value : *entries)
    w << *value;
  w << YAML::EndSeq;
  return w;
}

group::group(const shared_ptr<reader_state> &rs, const YAML::Node &node)
    : group() {
  ASDF_CHECK(node.IsMap(), "Expected a YAML mapping");
  for (const auto &key_value : node)
    insert({key_value.first.Scalar(), make_entry(rs, key_value.second)});
}

group::group(const copy_state &cs, const group &from) : group() {
  tag_ = from.tag_;
  for (const auto &[key, value] : *from.entries)
    insert({key, value->copy(cs)});
}

void group::collect_requirements(content_requirements &req,
                                 const std::string &path) const {
  for (const auto &[key, value] : *entries)
    value->collect_requirements(req, path + "/" + key);
}

writer &group::to_yaml(writer &w) const {
  // A map under a tag this library does not interpret is emitted as it was
  // read. If it holds an integer `source`, it looks like an array flavour
  // this library does not know: its block is not copied, so the `source`
  // index would end up pointing at an unrelated block (or at none).
  if (!tag_.empty()) {
    const auto source = entries->find("source");
    if (source != entries->end() && source->second->get_maybe_int())
      ASDF_CHECK(w.allow_nonstandard(),
                 "The node tagged \"" + tag_ +
                     "\" has an integer \"source\", so it refers to a binary "
                     "block, but this library does not know the tag and "
                     "therefore does not copy the block. Pass "
                     "--allow-nonstandard (write_options::allow_nonstandard) "
                     "to write the node anyway.");
  }
  emit_tag(w, tag_);
  w << YAML::BeginMap;
  for (const auto &[key, value] : *entries)
    w << YAML::Key << key << YAML::Value << *value;
  w << YAML::EndMap;
  return w;
}

std::shared_ptr<entry> make_entry(const std::shared_ptr<reader_state> &rs,
                                  const YAML::Node &node) {
  ASDF_CHECK(node.IsDefined(), "Undefined YAML node");

  // First look at the tag. If there is a tag we know what to do.
  const auto tag = node.Tag();

  switch (classify_core_tag(tag)) {
  case core_tag_t::complex_: {
    std::complex<float64_t> value;
    yaml_decode(node, value);
    return std::make_shared<complex_entry>(value);
  }
  case core_tag_t::software:
    return std::make_shared<software>(rs, node);
  case core_tag_t::ndarray:
    return std::make_shared<ndarray_entry>(std::make_shared<ndarray>(rs, node));
  case core_tag_t::asdf:
  case core_tag_t::none:
    break;
  }

  // A tag this library does not know is preserved verbatim, and so is the
  // node under it: the standard requires an unknown extension to survive a
  // round trip undeserialised.
  const bool have_tag = !is_trivial_tag(tag);

  // Next look at the node type.
  const auto make_untagged = [&]() -> std::shared_ptr<entry> {
    if (node.IsNull())
      return std::make_shared<null_entry>(std::tuple<>());

    if (node.IsScalar()) {
      // A tagged scalar keeps its text exactly as it was written: the tag's
      // schema is what gives the text its meaning, so re-typing it (`1.0` to
      // an int, a timestamp to a quoted string) would corrupt it.
      if (have_tag)
        return std::make_shared<string_entry>(node.Scalar(), true);

      // An untagged scalar can be a bool, an int, a float, or a string. Try
      // in this order.
      if (is_bool_spelling(node.Scalar())) {
        const auto bool8 = try_parse_yaml<bool>(node);
        if (bool8)
          return std::make_shared<bool_entry>(*bool8);
      }
      const auto int64 = try_parse_yaml<int64_t>(node);
      if (int64)
        return std::make_shared<int_entry>(*int64);
      const auto float64 = try_parse_yaml<float64_t>(node);
      if (float64)
        return std::make_shared<float_entry>(*float64);
      return std::make_shared<string_entry>(node.Scalar());
    }

    // Sequences are straightforward.
    if (node.IsSequence())
      return std::make_shared<sequence>(rs, node);

    // References are maps with a single key named `$ref`
    if (node.IsMap()) {
      if (node["$ref"])
        return std::make_shared<reference_entry>(rs, node);
      return std::make_shared<group>(rs, node);
    }

    ASDF_ERROR("Unhandled YAML node type");
  };

  const auto ent = make_untagged();
  if (have_tag)
    ent->set_tag(tag);
  return ent;
}

} // namespace ASDF
