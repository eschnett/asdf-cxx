#include "asdf_datatype.hpp"

#include <regex>

namespace ASDF {

// Scalar types

// Check consistency between id enum and tuple element
static_assert(is_same_v<get_scalar_type_t<id_bool8>, bool8_t>);
static_assert(is_same_v<get_scalar_type_t<id_int8>, int8_t>);
static_assert(is_same_v<get_scalar_type_t<id_int16>, int16_t>);
static_assert(is_same_v<get_scalar_type_t<id_int32>, int32_t>);
static_assert(is_same_v<get_scalar_type_t<id_int64>, int64_t>);
static_assert(is_same_v<get_scalar_type_t<id_uint8>, uint8_t>);
static_assert(is_same_v<get_scalar_type_t<id_uint16>, uint16_t>);
static_assert(is_same_v<get_scalar_type_t<id_uint32>, uint32_t>);
static_assert(is_same_v<get_scalar_type_t<id_uint64>, uint64_t>);
static_assert(is_same_v<get_scalar_type_t<id_float32>, float32_t>);
static_assert(is_same_v<get_scalar_type_t<id_float64>, float64_t>);
static_assert(is_same_v<get_scalar_type_t<id_complex64>, complex64_t>);
static_assert(is_same_v<get_scalar_type_t<id_complex128>, complex128_t>);
static_assert(is_same_v<get_scalar_type_t<id_ascii>, ascii_t>);
static_assert(is_same_v<get_scalar_type_t<id_ucs4>, ucs4_t>);

static_assert(get_scalar_type_id<bool8_t>() == id_bool8);
static_assert(get_scalar_type_id<int8_t>() == id_int8);
static_assert(get_scalar_type_id<int16_t>() == id_int16);
static_assert(get_scalar_type_id<int32_t>() == id_int32);
static_assert(get_scalar_type_id<int64_t>() == id_int64);
static_assert(get_scalar_type_id<uint8_t>() == id_uint8);
static_assert(get_scalar_type_id<uint16_t>() == id_uint16);
static_assert(get_scalar_type_id<uint32_t>() == id_uint32);
static_assert(get_scalar_type_id<uint64_t>() == id_uint64);
static_assert(get_scalar_type_id<float32_t>() == id_float32);
static_assert(get_scalar_type_id<float64_t>() == id_float64);
static_assert(get_scalar_type_id<complex64_t>() == id_complex64);
static_assert(get_scalar_type_id<complex128_t>() == id_complex128);
static_assert(get_scalar_type_id<ascii_t>() == id_ascii);
static_assert(get_scalar_type_id<ucs4_t>() == id_ucs4);

size_t get_scalar_type_size(scalar_type_id_t scalar_type_id) {
  switch (scalar_type_id) {
  case id_bool8:
    return sizeof(bool8_t);
  case id_int8:
    return sizeof(int8_t);
  case id_int16:
    return sizeof(int16_t);
  case id_int32:
    return sizeof(int32_t);
  case id_int64:
    return sizeof(int64_t);
  case id_uint8:
    return sizeof(uint8_t);
  case id_uint16:
    return sizeof(uint16_t);
  case id_uint32:
    return sizeof(uint32_t);
  case id_uint64:
    return sizeof(uint64_t);
  case id_float32:
    return sizeof(float32_t);
  case id_float64:
    return sizeof(float64_t);
  case id_complex64:
    return sizeof(complex64_t);
  case id_complex128:
    return sizeof(complex128_t);
    // case id_ascii
    // case id_ucs4
  }
  assert(0);
}

void yaml_decode(const YAML::Node &node,
                 ASDF::scalar_type_id_t &scalar_type_id) {
  string str = node.Scalar();
  if (str == "bool8")
    scalar_type_id = id_bool8;
  else if (str == "int8")
    scalar_type_id = id_int8;
  else if (str == "int16")
    scalar_type_id = id_int16;
  else if (str == "int32")
    scalar_type_id = id_int32;
  else if (str == "int64")
    scalar_type_id = id_int64;
  else if (str == "uint8")
    scalar_type_id = id_uint8;
  else if (str == "uint16")
    scalar_type_id = id_uint16;
  else if (str == "uint32")
    scalar_type_id = id_uint32;
  else if (str == "uint64")
    scalar_type_id = id_uint64;
  else if (str == "float32")
    scalar_type_id = id_float32;
  else if (str == "float64")
    scalar_type_id = id_float64;
  else if (str == "complex64")
    scalar_type_id = id_complex64;
  else if (str == "complex128")
    scalar_type_id = id_complex128;
  else
    // case id_ascii
    // case id_ucs4
    assert(0);
}

YAML::Node yaml_encode(scalar_type_id_t scalar_type_id) {
  YAML::Node node;
  switch (scalar_type_id) {
  case id_bool8:
    node = "bool8";
    break;
  case id_int8:
    node = "int8";
    break;
  case id_int16:
    node = "int16";
    break;
  case id_int32:
    node = "int32";
    break;
  case id_int64:
    node = "int64";
    break;
  case id_uint8:
    node = "uint8";
    break;
  case id_uint16:
    node = "uint16";
    break;
  case id_uint32:
    node = "uint32";
    break;
  case id_uint64:
    node = "uint64";
    break;
  case id_float32:
    node = "float32";
    break;
  case id_float64:
    node = "float64";
    break;
  case id_complex64:
    node = "complex64";
    break;
  case id_complex128:
    node = "complex128";
    break;
    // case id_ascii
    // case id_ucs4
  default:
    assert(0);
  }
  return node;
}

void yaml_decode(const YAML::Node &node, bool8_t &val) {
  val = node.as<bool8_t>();
}
void yaml_decode(const YAML::Node &node, int8_t &val) {
  val = node.as<int8_t>();
}
void yaml_decode(const YAML::Node &node, int16_t &val) {
  val = node.as<int16_t>();
}
void yaml_decode(const YAML::Node &node, int32_t &val) {
  val = node.as<int32_t>();
}
void yaml_decode(const YAML::Node &node, int64_t &val) {
  val = node.as<int64_t>();
}
void yaml_decode(const YAML::Node &node, uint8_t &val) {
  val = node.as<uint8_t>();
}
void yaml_decode(const YAML::Node &node, uint16_t &val) {
  val = node.as<uint16_t>();
}
void yaml_decode(const YAML::Node &node, uint32_t &val) {
  val = node.as<uint32_t>();
}
void yaml_decode(const YAML::Node &node, uint64_t &val) {
  val = node.as<uint64_t>();
}
void yaml_decode(const YAML::Node &node, float32_t &val) {
  val = node.as<float32_t>();
}
void yaml_decode(const YAML::Node &node, float64_t &val) {
  val = node.as<float64_t>();
}
template <typename T>
void yaml_decode(const YAML::Node &node, complex<T> &val) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/complex-1.0.0");
  static const string ieee = "[-+]?([0-9]*\\.?[0-9]+(e[-+]?[0-9]+)?|inf|nan)";
  static const regex cmplx("\\(?(" + ieee + ")?((" + ieee + ")[ij])?\\)?",
                           regex::icase | regex::optimize);
  assert(cmplx.mark_count() == 7);
  const auto &str = node.Scalar();
  smatch m;
  bool didmatch = regex_match(str, m, cmplx);
  assert(didmatch);
  T re, im;
  if (m[1].matched)
    re = stod(m[1].str());
  else
    re = 0;
  if (m[6].matched)
    im = stod(m[6].str());
  else
    im = 0;
  val = {re, im};
}

YAML::Node yaml_encode(bool8_t val) {
  YAML::Node node;
  node = val;
  return node;
}
YAML::Node yaml_encode(int8_t val) {
  YAML::Node node;
  node = int(val);
  return node;
}
YAML::Node yaml_encode(int16_t val) {
  YAML::Node node;
  node = val;
  return node;
}
YAML::Node yaml_encode(int32_t val) {
  YAML::Node node;
  node = val;
  return node;
}
YAML::Node yaml_encode(int64_t val) {
  YAML::Node node;
  node = val;
  return node;
}
YAML::Node yaml_encode(uint8_t val) {
  YAML::Node node;
  node = (unsigned int)(val);
  return node;
}
YAML::Node yaml_encode(uint16_t val) {
  YAML::Node node;
  node = val;
  return node;
}
YAML::Node yaml_encode(uint32_t val) {
  YAML::Node node;
  node = val;
  return node;
}
YAML::Node yaml_encode(uint64_t val) {
  YAML::Node node;
  node = val;
  return node;
}
YAML::Node yaml_encode(float32_t val) {
  YAML::Node node;
  node = val;
  return node;
}
YAML::Node yaml_encode(float64_t val) {
  YAML::Node node;
  node = val;
  return node;
}
template <typename T> YAML::Node yaml_encode(const complex<T> &val) {
  YAML::Emitter re;
  re << val.real();
  YAML::Emitter im;
  im << val.imag();
  ostringstream buf;
  buf << re.c_str();
  if (im.c_str()[0] != '-')
    buf << "+";
  buf << im.c_str() << "i";
  YAML::Node node;
  node.SetTag("tag:stsci.edu:asdf/core/complex-1.0.0");
  node = buf.str();
  return node;
}

void parse_scalar(const YAML::Node &node, unsigned char *data,
                  scalar_type_id_t scalar_type_id, byteorder_t byteorder) {
  switch (scalar_type_id) {
  case id_bool8:
    yaml_decode(node, *reinterpret_cast<bool8_t *>(data));
    htox<sizeof(bool8_t)>(data, byteorder);
    break;
  case id_int8:
    yaml_decode(node, *reinterpret_cast<int8_t *>(data));
    htox<sizeof(int8_t)>(data, byteorder);
    break;
  case id_int16:
    yaml_decode(node, *reinterpret_cast<int16_t *>(data));
    htox<sizeof(int16_t)>(data, byteorder);
    break;
  case id_int32:
    yaml_decode(node, *reinterpret_cast<int32_t *>(data));
    htox<sizeof(int32_t)>(data, byteorder);
    break;
  case id_int64:
    yaml_decode(node, *reinterpret_cast<int64_t *>(data));
    htox<sizeof(int64_t)>(data, byteorder);
    break;
  case id_uint8:
    yaml_decode(node, *reinterpret_cast<uint8_t *>(data));
    htox<sizeof(uint8_t)>(data, byteorder);
    break;
  case id_uint16:
    yaml_decode(node, *reinterpret_cast<uint16_t *>(data));
    htox<sizeof(uint16_t)>(data, byteorder);
    break;
  case id_uint32:
    yaml_decode(node, *reinterpret_cast<uint32_t *>(data));
    htox<sizeof(uint32_t)>(data, byteorder);
    break;
  case id_uint64:
    yaml_decode(node, *reinterpret_cast<uint64_t *>(data));
    htox<sizeof(uint64_t)>(data, byteorder);
    break;
  case id_float32:
    yaml_decode(node, *reinterpret_cast<float32_t *>(data));
    htox<sizeof(float32_t)>(data, byteorder);
    break;
  case id_float64:
    yaml_decode(node, *reinterpret_cast<float64_t *>(data));
    htox<sizeof(float64_t)>(data, byteorder);
    break;
  case id_complex64:
    yaml_decode(node, *reinterpret_cast<complex64_t *>(data));
    htox<sizeof(complex64_t)>(data, byteorder);
    break;
  case id_complex128:
    yaml_decode(node, *reinterpret_cast<complex128_t *>(data));
    htox<sizeof(complex128_t)>(data, byteorder);
    break;
  // case id_ascii
  // case id_ucs4
  default:
    assert(0);
  }
}

YAML::Node emit_scalar(const unsigned char *data,
                       scalar_type_id_t scalar_type_id, byteorder_t byteorder) {
  YAML::Node node;
  switch (scalar_type_id) {
  case id_bool8:
    node = yaml_encode(bool(xtoh<unsigned char>(data, byteorder)));
    break;
  case id_int8:
    node = yaml_encode(xtoh<int8_t>(data, byteorder));
    break;
  case id_int16:
    node = yaml_encode(xtoh<int16_t>(data, byteorder));
    break;
  case id_int32:
    node = yaml_encode(xtoh<int32_t>(data, byteorder));
    break;
  case id_int64:
    node = yaml_encode(xtoh<int64_t>(data, byteorder));
    break;
  case id_uint8:
    node = yaml_encode(xtoh<uint8_t>(data, byteorder));
    break;
  case id_uint16:
    node = yaml_encode(xtoh<uint16_t>(data, byteorder));
    break;
  case id_uint32:
    node = yaml_encode(xtoh<uint32_t>(data, byteorder));
    break;
  case id_uint64:
    node = yaml_encode(xtoh<uint64_t>(data, byteorder));
    break;
  case id_float32:
    node = yaml_encode(xtoh<float32_t>(data, byteorder));
    break;
  case id_float64:
    node = yaml_encode(xtoh<float64_t>(data, byteorder));
    break;
  case id_complex64:
    node = yaml_encode(xtoh<complex64_t>(data, byteorder));
    break;
  case id_complex128:
    node = yaml_encode(xtoh<complex128_t>(data, byteorder));
    break;
  // case id_ascii
  // case id_ucs4
  default:
    assert(0);
  }
  return node;
}

////////////////////////////////////////////////////////////////////////////////

// Datatypes

field_t::field_t(string name, shared_ptr<datatype_t> datatype,
                 bool have_byteorder, byteorder_t byteorder,
                 vector<int64_t> shape)
    : name(move(name)), datatype(move(datatype)),
      have_byteorder(have_byteorder), byteorder(byteorder), shape(move(shape)) {
  assert(datatype);
}

field_t::field_t(const shared_ptr<reader_state> &rs, const YAML::Node &node) {
  assert(0);
}

field_t::field_t(const copy_state &cs, const field_t &field) { assert(0); }

YAML::Node field_t::to_yaml() const {
  YAML::Node node;
  if (!name.empty())
    node["name"] = name;
  node["datatype"] = datatype->to_yaml();
  if (have_byteorder)
    node["byteorder"] = yaml_encode(byteorder);
  if (!shape.empty())
    node["shape"] = shape;
  return node;
}

datatype_t::datatype_t(scalar_type_id_t scalar_type_id)
    : is_scalar(true), scalar_type_id(scalar_type_id) {}

datatype_t::datatype_t(vector<shared_ptr<field_t>> fields)
    : is_scalar(false), fields(move(fields)) {}

size_t datatype_t::type_size() const {
  if (is_scalar)
    return get_scalar_type_size(scalar_type_id);
  size_t size = 0;
  for (const auto &field : fields)
    size += field->datatype->type_size();
  return size;
}

datatype_t::datatype_t(const shared_ptr<reader_state> &rs,
                       const YAML::Node &node) {
  if (node.IsScalar()) {
    is_scalar = true;
    yaml_decode(node, scalar_type_id);
    return;
  }
  assert(node.IsSequence());
  is_scalar = false;
  fields.reserve(node.size());
  for (YAML::const_iterator ni = node.begin(); ni != node.end(); ++ni)
    fields.push_back(make_shared<field_t>(rs, *ni));
}

datatype_t::datatype_t(const copy_state &cs, const datatype_t &datatype) {
  assert(0);
}

YAML::Node datatype_t::to_yaml() const {
  if (is_scalar)
    return yaml_encode(scalar_type_id);
  YAML::Node node;
  for (const auto &field : fields)
    node.push_back(field->to_yaml());
  return node;
}

void parse_scalar(const YAML::Node &node, unsigned char *data,
                  const shared_ptr<datatype_t> &datatype,
                  byteorder_t byteorder) {
  if (datatype->is_scalar)
    return parse_scalar(node, data, datatype->scalar_type_id, byteorder);
  unsigned char *ptr = data;
  for (const auto &field : datatype->fields) {
    parse_scalar(node, ptr, field->datatype,
                 field->have_byteorder ? field->byteorder : byteorder);
    ptr += field->datatype->type_size();
  }
}

YAML::Node emit_scalar(const unsigned char *data,
                       const shared_ptr<datatype_t> &datatype,
                       byteorder_t byteorder) {
  if (datatype->is_scalar)
    return emit_scalar(data, datatype->scalar_type_id, byteorder);
  YAML::Node node;
  node.SetStyle(YAML::EmitterStyle::Flow);
  const unsigned char *ptr = data;
  for (const auto &field : datatype->fields) {
    node.push_back(
        emit_scalar(ptr, field->datatype,
                    field->have_byteorder ? field->byteorder : byteorder));
    ptr += field->datatype->type_size();
  }
  return node;
}

} // namespace ASDF
