#include <asdf/config.hxx>
#include <asdf/datatype.hxx>
#include <asdf/stl.hxx>

#include <cstring>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ASDF {

bool have_datatype_int128() {
#ifdef ASDF_HAVE_INT128
  return true;
#else
  return false;
#endif
}
bool have_datatype_float16() {
#ifdef ASDF_HAVE_FLOAT16
  return true;
#else
  return false;
#endif
}

// Scalar types

// Check consistency between id enum and tuple element
static_assert(is_same<get_scalar_type_t<id_bool8>, bool8_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_int8>, int8_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_int16>, int16_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_int32>, int32_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_int64>, int64_t>::value, "");
#ifdef ASDF_HAVE_INT128
static_assert(is_same<get_scalar_type_t<id_int128>, int128_t>::value, "");
#endif
static_assert(is_same<get_scalar_type_t<id_uint8>, uint8_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_uint16>, uint16_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_uint32>, uint32_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_uint64>, uint64_t>::value, "");
#ifdef ASDF_HAVE_INT128
static_assert(is_same<get_scalar_type_t<id_uint128>, uint128_t>::value, "");
#endif
#ifdef ASDF_HAVE_FLOAT16
static_assert(is_same<get_scalar_type_t<id_float16>, float16_t>::value, "");
#endif
static_assert(is_same<get_scalar_type_t<id_float32>, float32_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_float64>, float64_t>::value, "");
#ifdef ASDF_HAVE_FLOAT16
static_assert(is_same<get_scalar_type_t<id_complex32>, complex32_t>::value, "");
#endif
static_assert(is_same<get_scalar_type_t<id_complex64>, complex64_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_complex128>, complex128_t>::value,
              "");
static_assert(is_same<get_scalar_type_t<id_ascii>, ascii_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_ucs4>, ucs4_t>::value, "");

static_assert(get_scalar_type_id<bool8_t>() == id_bool8, "");
static_assert(get_scalar_type_id<int8_t>() == id_int8, "");
static_assert(get_scalar_type_id<int16_t>() == id_int16, "");
static_assert(get_scalar_type_id<int32_t>() == id_int32, "");
static_assert(get_scalar_type_id<int64_t>() == id_int64, "");
#ifdef ASDF_HAVE_INT128
static_assert(get_scalar_type_id<int128_t>() == id_int128, "");
#endif
static_assert(get_scalar_type_id<uint8_t>() == id_uint8, "");
static_assert(get_scalar_type_id<uint16_t>() == id_uint16, "");
static_assert(get_scalar_type_id<uint32_t>() == id_uint32, "");
static_assert(get_scalar_type_id<uint64_t>() == id_uint64, "");
#ifdef ASDF_HAVE_INT128
static_assert(get_scalar_type_id<uint128_t>() == id_uint128, "");
#endif
#ifdef ASDF_HAVE_FLOAT16
static_assert(get_scalar_type_id<float16_t>() == id_float16, "");
#endif
static_assert(get_scalar_type_id<float32_t>() == id_float32, "");
static_assert(get_scalar_type_id<float64_t>() == id_float64, "");
#ifdef ASDF_HAVE_FLOAT16
static_assert(get_scalar_type_id<complex32_t>() == id_complex32, "");
#endif
static_assert(get_scalar_type_id<complex64_t>() == id_complex64, "");
static_assert(get_scalar_type_id<complex128_t>() == id_complex128, "");
static_assert(get_scalar_type_id<ascii_t>() == id_ascii, "");
static_assert(get_scalar_type_id<ucs4_t>() == id_ucs4, "");

// The size a datatype occupies in a file. These are properties of the ASDF
// standard, not of this build: a build without `_Float16` or `__int128` must
// still be able to read, bounds-check and copy blocks of those types.
#ifdef ASDF_HAVE_FLOAT16
static_assert(sizeof(float16_t) == 2, "");
static_assert(sizeof(complex32_t) == 4, "");
#endif
#ifdef ASDF_HAVE_INT128
static_assert(sizeof(int128_t) == 16, "");
static_assert(sizeof(uint128_t) == 16, "");
#endif

size_t get_scalar_type_size(scalar_type_id_t scalar_type_id) {
  switch (scalar_type_id) {
  case id_error:
    ASDF_ERROR("Scalar type id_error does not have a size");
  case id_bool8:
    return 1;
  case id_int8:
    return 1;
  case id_int16:
    return 2;
  case id_int32:
    return 4;
  case id_int64:
    return 8;
  case id_int128:
    return 16;
  case id_uint8:
    return 1;
  case id_uint16:
    return 2;
  case id_uint32:
    return 4;
  case id_uint64:
    return 8;
  case id_uint128:
    return 16;
  case id_float16:
    return 2;
  case id_float32:
    return 4;
  case id_float64:
    return 8;
  case id_complex32:
    return 4;
  case id_complex64:
    return 8;
  case id_complex128:
    return 16;
  case id_ascii:
  case id_ucs4:
    // These carry their length in the datatype
    ASDF_ERROR("The ascii and ucs4 datatypes have no fixed size; use "
               "datatype_t::type_size()");
  default:
    ASDF_ERROR("Invalid scalar_type_id_t value " +
               std::to_string(int(scalar_type_id)));
  }
}

void yaml_decode(const YAML::Node &node,
                 ASDF::scalar_type_id_t &scalar_type_id) {
  // Only the plain type names live here. The string types are written as the
  // two-element sequence `[ascii, N]` and a structured type as a list of
  // fields; both are handled by `datatype_t(rs, node)`.
  ASDF_CHECK(node.IsScalar(),
             "A scalar datatype must be a type name; the string form "
             "[ascii, N] / [ucs4, N] and structured field lists are decoded "
             "by datatype_t");
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
  else if (str == "int128")
    scalar_type_id = id_int128;
  else if (str == "uint8")
    scalar_type_id = id_uint8;
  else if (str == "uint16")
    scalar_type_id = id_uint16;
  else if (str == "uint32")
    scalar_type_id = id_uint32;
  else if (str == "uint64")
    scalar_type_id = id_uint64;
  else if (str == "uint128")
    scalar_type_id = id_uint128;
  else if (str == "float16")
    scalar_type_id = id_float16;
  else if (str == "float32")
    scalar_type_id = id_float32;
  else if (str == "float64")
    scalar_type_id = id_float64;
  else if (str == "complex32")
    scalar_type_id = id_complex32;
  else if (str == "complex64")
    scalar_type_id = id_complex64;
  else if (str == "complex128")
    scalar_type_id = id_complex128;
  else if (str == "ascii" || str == "ucs4")
    ASDF_ERROR("The " + str +
               " datatype must be written as the two-element form [" + str +
               ", N], not as a bare type name");
  else
    ASDF_ERROR("Unknown datatype \"" + str + "\"");
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
  case id_int128:
    node = "int128";
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
  case id_uint128:
    node = "uint128";
    break;
  case id_float16:
    node = "float16";
    break;
  case id_float32:
    node = "float32";
    break;
  case id_float64:
    node = "float64";
    break;
  case id_complex32:
    node = "complex32";
    break;
  case id_complex64:
    node = "complex64";
    break;
  case id_complex128:
    node = "complex128";
    break;
  case id_ascii:
  case id_ucs4:
    // These need their length, which only `datatype_t` knows
    ASDF_ERROR("Encoding the ascii or ucs4 datatype requires its length; use "
               "datatype_t::to_yaml()");
  default:
    ASDF_ERROR("Cannot encode invalid scalar type id " +
               std::to_string(int(scalar_type_id)));
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
#ifdef ASDF_HAVE_INT128
void yaml_decode(const YAML::Node &node, int128_t &val) {
  // yaml-cpp does not support `int128_t`
  // TODO: Parse as string, then convert ourselves
  val = int128_t(node.as<int64_t>());
}
#endif
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
#ifdef ASDF_HAVE_INT128
void yaml_decode(const YAML::Node &node, uint128_t &val) {
  // yaml-cpp does not support `uint128_t`
  // TODO: Parse as string, then convert ourselves
  val = node.as<uint64_t>();
}
#endif
#ifdef ASDF_HAVE_FLOAT16
void yaml_decode(const YAML::Node &node, float16_t &val) {
  // yaml-cpp does not support `float16_t`
  val = float16_t(node.as<float32_t>());
}
#endif
void yaml_decode(const YAML::Node &node, float32_t &val) {
  val = node.as<float32_t>();
}
void yaml_decode(const YAML::Node &node, float64_t &val) {
  val = node.as<float64_t>();
}
namespace {
template <typename T>
void yaml_decode_complex(const YAML::Node &node, complex<T> &val) {
  ASDF_CHECK(node.Tag() == "tag:stsci.edu:asdf/core/complex-1.0.0",
             "Expected tag core/complex-1.0.0, found \"" + node.Tag() + "\"");
  static const string ieee = "([-+]?[0-9]*\\.?[0-9]+(e[-+]?[0-9]+)?|inf|nan)";
  static const regex cmplx("\\(?(" + ieee + ")?((" + ieee + ")[ij])?\\)?",
                           regex::icase | regex::optimize);
  assert(cmplx.mark_count() == 7);
  const auto &str = node.Scalar();
  smatch m;
  bool didmatch = regex_match(str, m, cmplx);
  ASDF_CHECK(didmatch, "Cannot parse complex number \"" + str + "\"");
  const T re = m[1].matched ? static_cast<T>(stod(m[1].str())) : 0;
  const T im = m[6].matched ? static_cast<T>(stod(m[6].str())) : 0;
  val = {re, im};
}
} // namespace
#ifdef ASDF_HAVE_FLOAT16
void yaml_decode(const YAML::Node &node, complex32_t &val) {
  yaml_decode_complex(node, val);
}
#endif
void yaml_decode(const YAML::Node &node, complex64_t &val) {
  yaml_decode_complex(node, val);
}
void yaml_decode(const YAML::Node &node, complex128_t &val) {
  yaml_decode_complex(node, val);
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
#ifdef ASDF_HAVE_INT128
YAML::Node yaml_encode(int128_t val) {
  YAML::Node node;
  // TODO: Represent as string
  ASDF_CHECK(val >= std::numeric_limits<int64_t>::min() &&
                 val <= std::numeric_limits<int64_t>::max(),
             "Cannot encode int128 values outside the int64 range");
  node = int64_t(val);
  return node;
}
#endif
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
#ifdef ASDF_HAVE_INT128
YAML::Node yaml_encode(uint128_t val) {
  YAML::Node node;
  // TODO: Represent as string
  ASDF_CHECK(val <= std::numeric_limits<uint64_t>::max(),
             "Cannot encode uint128 values outside the uint64 range");
  node = uint64_t(val);
  return node;
}
#endif
#ifdef ASDF_HAVE_FLOAT16
YAML::Node yaml_encode(float16_t val) {
  YAML::Node node;
  // yaml-cpp does not support `float16_t`
  node = float32_t(val);
  return node;
}
#endif
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
namespace {
template <typename T> YAML::Node yaml_encode_complex(const complex<T> &val) {
  // Work around libstdc++ const-handling bug in gcc 4.8
  auto val1(val);
  YAML::Emitter re;
  re << val1.real();
  YAML::Emitter im;
  im << val1.imag();
  ostringstream buf;
  buf << re.c_str();
  if (im.c_str()[0] != '-')
    buf << "+";
  buf << im.c_str() << "i";

  YAML::Node node;
  // TODO: Use a local tag
  node.SetTag("tag:stsci.edu:asdf/core/complex-1.0.0");
  node = buf.str();
  return node;
}
} // namespace
#ifdef ASDF_HAVE_FLOAT16
YAML::Node yaml_encode(complex32_t val) {
  // yaml-cpp does not support `float16_t`
  return yaml_encode(complex64_t(float32_t(val.real()), float32_t(val.imag())));
}
#endif
YAML::Node yaml_encode(complex64_t val) { return yaml_encode_complex(val); }
YAML::Node yaml_encode(complex128_t val) { return yaml_encode_complex(val); }

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
#ifdef ASDF_HAVE_INT128
  case id_int128:
    yaml_decode(node, *reinterpret_cast<int128_t *>(data));
    htox<sizeof(int128_t)>(data, byteorder);
    break;
#else
  case id_int128:
    ASDF_ERROR("Cannot parse int128 values: this build has no 128-bit "
               "integer type");
#endif
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
#ifdef ASDF_HAVE_INT128
  case id_uint128:
    yaml_decode(node, *reinterpret_cast<uint128_t *>(data));
    htox<sizeof(uint128_t)>(data, byteorder);
    break;
#else
  case id_uint128:
    ASDF_ERROR("Cannot parse uint128 values: this build has no 128-bit "
               "integer type");
#endif
#ifdef ASDF_HAVE_FLOAT16
  case id_float16:
    yaml_decode(node, *reinterpret_cast<float16_t *>(data));
    htox<sizeof(float16_t)>(data, byteorder);
    break;
#else
  case id_float16:
    ASDF_ERROR("Cannot parse float16 values: this build has no 16-bit "
               "floating-point type");
#endif
  case id_float32:
    yaml_decode(node, *reinterpret_cast<float32_t *>(data));
    htox<sizeof(float32_t)>(data, byteorder);
    break;
  case id_float64:
    yaml_decode(node, *reinterpret_cast<float64_t *>(data));
    htox<sizeof(float64_t)>(data, byteorder);
    break;
#ifdef ASDF_HAVE_FLOAT16
  case id_complex32:
    yaml_decode(node, *reinterpret_cast<complex32_t *>(data));
    // A complex number is swapped per component
    htox<sizeof(complex32_t) / 2>(data, byteorder);
    htox<sizeof(complex32_t) / 2>(data + sizeof(complex32_t) / 2, byteorder);
    break;
#else
  case id_complex32:
    ASDF_ERROR("Cannot parse complex32 values: this build has no 16-bit "
               "floating-point type");
#endif
  case id_complex64:
    yaml_decode(node, *reinterpret_cast<complex64_t *>(data));
    // A complex number is swapped per component
    htox<sizeof(complex64_t) / 2>(data, byteorder);
    htox<sizeof(complex64_t) / 2>(data + sizeof(complex64_t) / 2, byteorder);
    break;
  case id_complex128:
    yaml_decode(node, *reinterpret_cast<complex128_t *>(data));
    // A complex number is swapped per component
    htox<sizeof(complex128_t) / 2>(data, byteorder);
    htox<sizeof(complex128_t) / 2>(data + sizeof(complex128_t) / 2, byteorder);
    break;
  case id_ascii:
  case id_ucs4:
    // These need their length, which only `datatype_t` knows
    ASDF_ERROR("Parsing ascii or ucs4 values requires their length; use the "
               "datatype_t overload of parse_scalar");
  default:
    ASDF_ERROR("Cannot parse values of invalid scalar type id " +
               std::to_string(int(scalar_type_id)));
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
#ifdef ASDF_HAVE_INT128
  case id_int128:
    node = yaml_encode(xtoh<int128_t>(data, byteorder));
    break;
#else
  case id_int128:
    ASDF_ERROR("Cannot emit int128 values: this build has no 128-bit "
               "integer type");
#endif
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
#ifdef ASDF_HAVE_INT128
  case id_uint128:
    node = yaml_encode(xtoh<uint128_t>(data, byteorder));
    break;
#else
  case id_uint128:
    ASDF_ERROR("Cannot emit uint128 values: this build has no 128-bit "
               "integer type");
#endif
#ifdef ASDF_HAVE_FLOAT16
  case id_float16:
    node = yaml_encode(xtoh<float16_t>(data, byteorder));
    break;
#else
  case id_float16:
    ASDF_ERROR("Cannot emit float16 values: this build has no 16-bit "
               "floating-point type");
#endif
  case id_float32:
    node = yaml_encode(xtoh<float32_t>(data, byteorder));
    break;
  case id_float64:
    node = yaml_encode(xtoh<float64_t>(data, byteorder));
    break;
#ifdef ASDF_HAVE_FLOAT16
  case id_complex32:
    node = yaml_encode(xtoh<complex32_t>(data, byteorder));
    break;
#else
  case id_complex32:
    ASDF_ERROR("Cannot emit complex32 values: this build has no 16-bit "
               "floating-point type");
#endif
  case id_complex64:
    node = yaml_encode(xtoh<complex64_t>(data, byteorder));
    break;
  case id_complex128:
    node = yaml_encode(xtoh<complex128_t>(data, byteorder));
    break;
  case id_ascii:
  case id_ucs4:
    // These need their length, which only `datatype_t` knows
    ASDF_ERROR("Emitting ascii or ucs4 values requires their length; use the "
               "datatype_t overload of emit_scalar");
  default:
    ASDF_ERROR("Cannot emit values of invalid scalar type id " +
               std::to_string(int(scalar_type_id)));
  }
  return node;
}

////////////////////////////////////////////////////////////////////////////////

// Datatypes

namespace {

string describe_extents(const vector<int64_t> &values) {
  ostringstream buf;
  buf << "[";
  for (size_t d = 0; d < values.size(); ++d)
    buf << (d == 0 ? "" : ", ") << values.at(d);
  buf << "]";
  return buf.str();
}

} // namespace

field_t::field_t(string name, shared_ptr<datatype_t> datatype,
                 bool have_byteorder, byteorder_t byteorder,
                 vector<int64_t> shape)
    : name(std::move(name)), datatype(std::move(datatype)),
      have_byteorder(have_byteorder), byteorder(byteorder),
      shape(std::move(shape)) {
  // (The constructor argument has been moved from)
  ASDF_CHECK(this->datatype, "A field requires a datatype");
}

field_t::field_t(const shared_ptr<reader_state> &rs, const YAML::Node &node)
    : have_byteorder(false), byteorder(byteorder_t::undefined) {
  if (node.IsScalar()) {
    // An anonymous field, described by its datatype alone
    datatype = make_shared<datatype_t>(rs, node);
    return;
  }
  ASDF_CHECK(node.IsMap(),
             "A datatype field must be a scalar type name or a mapping");
  if (node["name"].IsDefined())
    name = node["name"].Scalar();
  // The datatype is the only required entry. It can itself be structured.
  ASDF_CHECK(node["datatype"].IsDefined(),
             "A datatype field must have a \"datatype\" entry");
  datatype = make_shared<datatype_t>(rs, node["datatype"]);
  if (node["byteorder"].IsDefined()) {
    have_byteorder = true;
    yaml_decode(node["byteorder"], byteorder);
  }
  if (node["shape"].IsDefined())
    yaml_decode(node["shape"], shape);
}

field_t::field_t(const copy_state &cs, const field_t &field)
    : name(field.name), datatype(make_shared<datatype_t>(cs, *field.datatype)),
      have_byteorder(field.have_byteorder), byteorder(field.byteorder),
      shape(field.shape) {}

int64_t field_t::num_elements() const {
  int64_t count = 1;
  for (const auto extent : shape) {
    ASDF_CHECK(extent >= 0, "Field shape must not have negative extents");
    ASDF_CHECK(extent == 0 || count <= numeric_limits<int64_t>::max() / extent,
               "Field shape " + describe_extents(shape) +
                   " is too large: the number of elements does not fit into a "
                   "64-bit integer");
    count *= extent;
  }
  return count;
}

size_t field_t::type_size() const {
  const int64_t count = num_elements();
  const size_t elemsize_bytes = datatype->type_size();
  ASDF_CHECK(elemsize_bytes <= uint64_t(numeric_limits<int64_t>::max()),
             "Field element size " + std::to_string(elemsize_bytes) +
                 " does not fit into a 64-bit integer");
  const int64_t elemsize = int64_t(elemsize_bytes);
  ASDF_CHECK(count == 0 || elemsize <= numeric_limits<int64_t>::max() / count,
             "Field shape " + describe_extents(shape) +
                 " is too large: the field's size in bytes does not fit into "
                 "a 64-bit integer");
  const int64_t size = count * elemsize;
  ASDF_CHECK(uint64_t(size) <= numeric_limits<size_t>::max(),
             "Field shape " + describe_extents(shape) +
                 " is too large: the field's size in bytes does not fit into "
                 "a size_t");
  return size_t(size);
}

YAML::Node field_t::to_yaml() const {
  YAML::Node node;
  if (!name.empty())
    node["name"] = name;
  node["datatype"] = datatype->to_yaml();
  if (have_byteorder)
    node["byteorder"] = yaml_encode(byteorder);
  if (!shape.empty())
    node["shape"] = yaml_encode(shape);
  return node;
}

datatype_t::datatype_t(scalar_type_id_t scalar_type_id)
    : is_scalar(true), scalar_type_id(scalar_type_id), string_length(0) {
  ASDF_CHECK(scalar_type_id != id_ascii && scalar_type_id != id_ucs4,
             "The ascii and ucs4 datatypes require a string length");
}

datatype_t::datatype_t(scalar_type_id_t scalar_type_id, size_t string_length)
    : is_scalar(true), scalar_type_id(scalar_type_id),
      string_length(string_length) {
  ASDF_CHECK(scalar_type_id == id_ascii || scalar_type_id == id_ucs4,
             "Only the ascii and ucs4 datatypes have a string length");
}

datatype_t::datatype_t(vector<shared_ptr<field_t>> fields)
    : is_scalar(false), scalar_type_id(id_error), string_length(0),
      fields(std::move(fields)) {}

size_t datatype_t::type_size() const {
  if (is_scalar) {
    // The string types carry their length in the datatype: an `ascii`
    // element is one byte per character, a `ucs4` element a 4-byte code unit
    // per character
    if (scalar_type_id == id_ascii)
      return string_length;
    if (scalar_type_id == id_ucs4) {
      ASDF_CHECK(string_length <= numeric_limits<size_t>::max() / 4,
                 "String length " + std::to_string(string_length) +
                     " is too large for a ucs4 datatype");
      return 4 * string_length;
    }
    return get_scalar_type_size(scalar_type_id);
  }
  // Structured types are packed, without any alignment padding
  size_t size = 0;
  for (const auto &field : fields) {
    const size_t field_size = field->type_size();
    ASDF_CHECK(field_size <= numeric_limits<size_t>::max() - size,
               "Structured datatype is too large: its size in bytes does not "
               "fit into a size_t");
    size += field_size;
  }
  return size;
}

datatype_t::datatype_t(const shared_ptr<reader_state> &rs,
                       const YAML::Node &node) {
  if (node.IsScalar()) {
    is_scalar = true;
    string_length = 0;
    yaml_decode(node, scalar_type_id);
    return;
  }
  ASDF_CHECK(node.IsSequence(),
             "A datatype must be a scalar type name or a list of fields");
  // The string types are written as the two-element sequence `[ascii, N]` or
  // `[ucs4, N]`. A field list is told apart by its items: they are either
  // mappings or scalar type names, and neither `ascii` nor `ucs4` is a valid
  // type name on its own.
  if (node.size() == 2 && node[0].IsScalar() && node[1].IsScalar() &&
      (node[0].Scalar() == "ascii" || node[0].Scalar() == "ucs4")) {
    is_scalar = true;
    const string &name = node[0].Scalar();
    scalar_type_id = name == "ascii" ? id_ascii : id_ucs4;
    // yaml-cpp would report its own "bad conversion" for a length that is
    // not a number, which says nothing about the datatype
    int64_t length;
    try {
      length = node[1].as<int64_t>();
    } catch (const YAML::Exception &) {
      length = -1;
    }
    ASDF_CHECK(length >= 0, "The length of the " + name +
                                " datatype must be a non-negative integer, "
                                "not \"" +
                                node[1].Scalar() + "\"");
    string_length = size_t(length);
    // Reject a length whose element size does not fit
    type_size();
    return;
  }
  is_scalar = false;
  scalar_type_id = id_error;
  string_length = 0;
  fields.reserve(node.size());
  for (YAML::const_iterator ni = node.begin(); ni != node.end(); ++ni)
    fields.push_back(make_shared<field_t>(rs, *ni));
}

datatype_t::datatype_t(const copy_state &cs, const datatype_t &datatype)
    : is_scalar(datatype.is_scalar), scalar_type_id(datatype.scalar_type_id),
      string_length(datatype.string_length) {
  if (is_scalar)
    return;
  fields.reserve(datatype.fields.size());
  for (const auto &field : datatype.fields)
    fields.push_back(make_shared<field_t>(cs, *field));
}

YAML::Node datatype_t::to_yaml() const {
  if (is_scalar) {
    if (scalar_type_id == id_ascii || scalar_type_id == id_ucs4) {
      YAML::Node node(YAML::NodeType::Sequence);
      node.SetStyle(YAML::EmitterStyle::Flow);
      node.push_back(scalar_type_id == id_ascii ? "ascii" : "ucs4");
      node.push_back(uint64_t(string_length));
      return node;
    }
    return yaml_encode(scalar_type_id);
  }
  YAML::Node node;
  for (const auto &field : fields)
    node.push_back(field->to_yaml());
  return node;
}

namespace {

// Strings ---------------------------------------------------------------

// Is `code` a Unicode scalar value, i.e. neither out of range nor half of a
// surrogate pair?
bool is_valid_code_point(char32_t code) {
  return uint32_t(code) <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff);
}

// Decode a UTF-8 string into code points. Inline `ucs4` data arrives as
// UTF-8, which is what YAML stores.
vector<char32_t> utf8_decode(const string &str) {
  static const char32_t lowest[4] = {0, 0x80, 0x800, 0x10000};
  vector<char32_t> codes;
  size_t i = 0;
  while (i < str.size()) {
    const unsigned char lead = str[i];
    int extra;
    char32_t code;
    if (lead < 0x80) {
      extra = 0, code = lead;
    } else if ((lead & 0xe0) == 0xc0) {
      extra = 1, code = lead & 0x1f;
    } else if ((lead & 0xf0) == 0xe0) {
      extra = 2, code = lead & 0x0f;
    } else if ((lead & 0xf8) == 0xf0) {
      extra = 3, code = lead & 0x07;
    } else {
      ASDF_ERROR("Malformed UTF-8 string: invalid leading byte");
    }
    ASDF_CHECK(i + size_t(extra) < str.size(),
               "Malformed UTF-8 string: truncated multi-byte sequence");
    for (int k = 1; k <= extra; ++k) {
      const unsigned char cont = str[i + size_t(k)];
      ASDF_CHECK((cont & 0xc0) == 0x80,
                 "Malformed UTF-8 string: invalid continuation byte");
      code = (code << 6) | (cont & 0x3f);
    }
    ASDF_CHECK(code >= lowest[extra],
               "Malformed UTF-8 string: overlong encoding");
    ASDF_CHECK(is_valid_code_point(code),
               "Malformed UTF-8 string: invalid code point");
    codes.push_back(code);
    i += size_t(extra) + 1;
  }
  return codes;
}

string utf8_encode(const vector<char32_t> &codes) {
  string str;
  for (const auto code : codes) {
    ASDF_CHECK(is_valid_code_point(code), "Cannot encode the ucs4 code point " +
                                              std::to_string(uint32_t(code)) +
                                              " as UTF-8");
    const uint32_t c = code;
    if (c < 0x80) {
      str += char(c);
    } else if (c < 0x800) {
      str += char(0xc0 | (c >> 6));
      str += char(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
      str += char(0xe0 | (c >> 12));
      str += char(0x80 | ((c >> 6) & 0x3f));
      str += char(0x80 | (c & 0x3f));
    } else {
      str += char(0xf0 | (c >> 18));
      str += char(0x80 | ((c >> 12) & 0x3f));
      str += char(0x80 | ((c >> 6) & 0x3f));
      str += char(0x80 | (c & 0x3f));
    }
  }
  return str;
}

// One `ascii` or `ucs4` element. Both are fixed-length and padded with null
// code units, as in numpy's `S` and `U` dtypes.
void parse_string(const YAML::Node &node, unsigned char *data,
                  scalar_type_id_t scalar_type_id, size_t string_length,
                  byteorder_t byteorder) {
  ASDF_CHECK(node.IsScalar(),
             "An ascii or ucs4 array element must be a YAML scalar");
  const string &str = node.Scalar();
  if (scalar_type_id == id_ascii) {
    ASDF_CHECK(str.size() <= string_length,
               "String \"" + str +
                   "\" is longer than the ascii datatype's "
                   "length " +
                   std::to_string(string_length));
    for (const auto ch : str)
      ASDF_CHECK(static_cast<unsigned char>(ch) < 0x80,
                 "String \"" + str +
                     "\" is not ASCII: the ascii datatype holds 7-bit "
                     "characters only");
    std::memcpy(data, str.data(), str.size());
    std::memset(data + str.size(), 0, string_length - str.size());
    return;
  }
  const vector<char32_t> codes = utf8_decode(str);
  ASDF_CHECK(codes.size() <= string_length,
             "String \"" + str +
                 "\" is longer than the ucs4 datatype's "
                 "length " +
                 std::to_string(string_length));
  for (size_t i = 0; i < string_length; ++i) {
    const uint32_t code = i < codes.size() ? uint32_t(codes.at(i)) : 0;
    std::memcpy(data + 4 * i, &code, 4);
    htox<4>(data + 4 * i, byteorder);
  }
}

YAML::Node emit_string(const unsigned char *data,
                       scalar_type_id_t scalar_type_id, size_t string_length,
                       byteorder_t byteorder) {
  YAML::Node node;
  if (scalar_type_id == id_ascii) {
    // Trailing null bytes are padding
    size_t length = string_length;
    while (length > 0 && data[length - 1] == 0)
      --length;
    for (size_t i = 0; i < length; ++i)
      ASDF_CHECK(data[i] < 0x80,
                 "Cannot emit this ascii value: the ascii datatype holds "
                 "7-bit characters only");
    node = string(reinterpret_cast<const char *>(data), length);
    return node;
  }
  vector<char32_t> codes(string_length);
  for (size_t i = 0; i < string_length; ++i)
    codes.at(i) = char32_t(xtoh<uint32_t>(data + 4 * i, byteorder));
  while (!codes.empty() && codes.back() == 0)
    codes.pop_back();
  node = utf8_encode(codes);
  return node;
}

// Structured elements ---------------------------------------------------

// Parse a field that may be a sub-array, i.e. that may have a `shape`. The
// elements are stored contiguously in C order, and `ptr` is advanced past
// them.
void parse_field(const YAML::Node &node, unsigned char *&ptr,
                 const shared_ptr<datatype_t> &datatype,
                 const vector<int64_t> &shape, size_t dim,
                 byteorder_t byteorder) {
  if (dim == shape.size()) {
    parse_scalar(node, ptr, datatype, byteorder);
    ptr += datatype->type_size();
    return;
  }
  ASDF_CHECK(node.IsSequence() &&
                 node.size() == static_cast<size_t>(shape.at(dim)),
             "Sub-array field data does not match the field shape");
  for (YAML::const_iterator ni = node.begin(), ne = node.end(); ni != ne; ++ni)
    parse_field(*ni, ptr, datatype, shape, dim + 1, byteorder);
}

YAML::Node emit_field(const unsigned char *&ptr,
                      const shared_ptr<datatype_t> &datatype,
                      const vector<int64_t> &shape, size_t dim,
                      byteorder_t byteorder) {
  if (dim == shape.size()) {
    YAML::Node node = emit_scalar(ptr, datatype, byteorder);
    ptr += datatype->type_size();
    return node;
  }
  YAML::Node node(YAML::NodeType::Sequence);
  node.SetStyle(YAML::EmitterStyle::Flow);
  for (int64_t i = 0; i < shape.at(dim); ++i)
    node.push_back(emit_field(ptr, datatype, shape, dim + 1, byteorder));
  return node;
}

} // namespace

void parse_scalar(const YAML::Node &node, unsigned char *data,
                  const shared_ptr<datatype_t> &datatype,
                  byteorder_t byteorder) {
  if (datatype->is_scalar) {
    if (datatype->scalar_type_id == id_ascii ||
        datatype->scalar_type_id == id_ucs4)
      return parse_string(node, data, datatype->scalar_type_id,
                          datatype->string_length, byteorder);
    return parse_scalar(node, data, datatype->scalar_type_id, byteorder);
  }
  // A structured element is a sequence with one entry per field
  ASDF_CHECK(
      node.IsSequence() && node.size() == datatype->fields.size(),
      "A structured element must be a sequence with one entry per field");
  unsigned char *ptr = data;
  for (size_t i = 0; i < datatype->fields.size(); ++i) {
    const auto &field = datatype->fields.at(i);
    parse_field(node[i], ptr, field->datatype, field->shape, 0,
                field->have_byteorder ? field->byteorder : byteorder);
  }
  assert(ptr == data + datatype->type_size());
}
YAML::Node emit_scalar(const unsigned char *data,
                       const shared_ptr<datatype_t> &datatype,
                       byteorder_t byteorder) {
  if (datatype->is_scalar) {
    if (datatype->scalar_type_id == id_ascii ||
        datatype->scalar_type_id == id_ucs4)
      return emit_string(data, datatype->scalar_type_id,
                         datatype->string_length, byteorder);
    return emit_scalar(data, datatype->scalar_type_id, byteorder);
  }
  YAML::Node node(YAML::NodeType::Sequence);
  node.SetStyle(YAML::EmitterStyle::Flow);
  const unsigned char *ptr = data;
  for (const auto &field : datatype->fields)
    node.push_back(
        emit_field(ptr, field->datatype, field->shape, 0,
                   field->have_byteorder ? field->byteorder : byteorder));
  assert(ptr == data + datatype->type_size());
  return node;
}

namespace {

// The number of bytes that make up one byte-swappable component of a scalar
// type, and how many of them there are per element. Complex numbers consist
// of two independently swapped components, `ucs4` strings of one per code
// unit, and `ascii` strings of single bytes that are never swapped.
void scalar_component_layout(scalar_type_id_t scalar_type_id, size_t type_size,
                             size_t &component_size, size_t &num_components) {
  switch (scalar_type_id) {
  case id_complex32:
  case id_complex64:
  case id_complex128:
    component_size = type_size / 2;
    num_components = 2;
    break;
  case id_ucs4:
    component_size = 4;
    num_components = type_size / 4;
    break;
  case id_bool8:
  case id_int8:
  case id_uint8:
  case id_ascii:
    component_size = 1;
    num_components = type_size;
    break;
  default:
    component_size = type_size;
    num_components = 1;
    break;
  }
}

// One field of a structured element, including its sub-array `shape`
void convert_field_to_host(const unsigned char *src, unsigned char *dst,
                           const field_t &field, byteorder_t byteorder) {
  const byteorder_t field_byteorder =
      field.have_byteorder ? field.byteorder : byteorder;
  const size_t elemsize = field.datatype->type_size();
  const int64_t count = field.num_elements();
  for (int64_t i = 0; i < count; ++i)
    convert_element_to_host(src + size_t(i) * elemsize,
                            dst + size_t(i) * elemsize, *field.datatype,
                            field_byteorder);
}

} // namespace

void convert_element_to_host(const unsigned char *src, unsigned char *dst,
                             const datatype_t &datatype,
                             byteorder_t byteorder) {
  if (!datatype.is_scalar) {
    size_t field_offset = 0;
    for (const auto &field : datatype.fields) {
      convert_field_to_host(src + field_offset, dst + field_offset, *field,
                            byteorder);
      field_offset += field->type_size();
    }
    assert(field_offset == datatype.type_size());
    return;
  }

  const size_t type_size = datatype.type_size();
  std::memcpy(dst, src, type_size);
  if (byteorder == host_byteorder())
    return;
  ASDF_CHECK(byteorder != byteorder_t::undefined,
             "Cannot convert array data: the byte order is undefined");
  size_t component_size, num_components;
  scalar_component_layout(datatype.scalar_type_id, type_size, component_size,
                          num_components);
  if (component_size > 1)
    detail::reverse_components(dst, component_size, num_components);
}

} // namespace ASDF
