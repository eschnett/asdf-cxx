#include "asdf.hpp"
#include "asdf-config.hpp"

#include <yaml-cpp/emittermanip.h>
#include <yaml-cpp/emitterstyle.h>
#include <yaml-cpp/node/type.h>
#include <yaml-cpp/yaml.h>

#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

#ifdef HAVE_OPENSSL
#include <openssl/md5.h>
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <istream>
#include <limits>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace ASDF {

const string asdf_format_version = "1.0.0";
const string asdf_standard_version = "1.1.0";

////////////////////////////////////////////////////////////////////////////////

// Byte order

void yaml_decode(const YAML::Node &node, byteorder_t &byteorder) {
  string str = node.Scalar();
  if (str == "big")
    byteorder = byteorder_t::big;
  else if (str == "little")
    byteorder = byteorder_t::little;
  else
    assert(0);
}

YAML::Node yaml_encode(byteorder_t byteorder) {
  YAML::Node node;
  switch (byteorder) {
  case byteorder_t::big:
    node = "big";
    break;
  case byteorder_t::little:
    node = "little";
    break;
  default:
    assert(0);
  }
  return node;
}

byteorder_t host_byteorder() {
  const uint64_t magic{0x0102030405060708};
  const array<unsigned char, 8> magic_big{0x01, 0x02, 0x03, 0x04,
                                          0x05, 0x06, 0x07, 0x08};
  const array<unsigned char, 8> magic_little{0x08, 0x07, 0x06, 0x05,
                                             0x04, 0x03, 0x02, 0x01};
  if (memcmp(&magic, &magic_big, 8) == 0)
    return byteorder_t::big;
  if (memcmp(&magic, &magic_little, 8) == 0)
    return byteorder_t::little;
  assert(0);
}

////////////////////////////////////////////////////////////////////////////////

// I/O

reader_state::reader_state(istream &is) {
  for (;;) {
    const auto &block = read_block(is);
    if (!block)
      break;
    blocks.push_back(move(block));
  }
}

writer_state::writer_state() {}

writer_state::~writer_state() { assert(tasks.empty()); }

void writer_state::flush(ostream &os) {
  if (tasks.empty())
    return;
  YAML::Emitter index;
  index << YAML::Flow;
  index << YAML::BeginSeq;
  for (auto &&task : tasks) {
    index << os.tellp();
    move(task)(os);
  }
  tasks.clear();
  index << YAML::EndSeq;
  os << "#ASDF BLOCK INDEX\n"
     << "%YAML 1.1\n"
     << "---\n"
     << index.c_str() << "\n"
     << "...\n";
}

////////////////////////////////////////////////////////////////////////////////

// Scalar types

constexpr scalar_type_id_t get_scalar_type_id<bool8_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<int8_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<int16_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<int32_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<int64_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<uint8_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<uint16_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<uint32_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<uint64_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<float32_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<float64_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<complex64_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<complex128_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<ascii_t>::value;
constexpr scalar_type_id_t get_scalar_type_id<ucs4_t>::value;

// Check consistency between id enum and tuple element
static_assert(is_same<get_scalar_type_t<id_bool8>, bool8_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_int8>, int8_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_int16>, int16_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_int32>, int32_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_int64>, int64_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_uint8>, uint8_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_uint16>, uint16_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_uint32>, uint32_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_uint64>, uint64_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_float32>, float32_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_float64>, float64_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_complex64>, complex64_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_complex128>, complex128_t>::value,
              "");
static_assert(is_same<get_scalar_type_t<id_ascii>, ascii_t>::value, "");
static_assert(is_same<get_scalar_type_t<id_ucs4>, ucs4_t>::value, "");

static_assert(get_scalar_type_id<bool8_t>::value == id_bool8, "");
static_assert(get_scalar_type_id<int8_t>::value == id_int8, "");
static_assert(get_scalar_type_id<int16_t>::value == id_int16, "");
static_assert(get_scalar_type_id<int32_t>::value == id_int32, "");
static_assert(get_scalar_type_id<int64_t>::value == id_int64, "");
static_assert(get_scalar_type_id<uint8_t>::value == id_uint8, "");
static_assert(get_scalar_type_id<uint16_t>::value == id_uint16, "");
static_assert(get_scalar_type_id<uint32_t>::value == id_uint32, "");
static_assert(get_scalar_type_id<uint64_t>::value == id_uint64, "");
static_assert(get_scalar_type_id<float32_t>::value == id_float32, "");
static_assert(get_scalar_type_id<float64_t>::value == id_float64, "");
static_assert(get_scalar_type_id<complex64_t>::value == id_complex64, "");
static_assert(get_scalar_type_id<complex128_t>::value == id_complex128, "");
static_assert(get_scalar_type_id<ascii_t>::value == id_ascii, "");
static_assert(get_scalar_type_id<ucs4_t>::value == id_ucs4, "");

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
  node = val;
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
  node = val;
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
                  scalar_type_id_t scalar_type_id) {
  switch (scalar_type_id) {
  case id_bool8:
    yaml_decode(node, *reinterpret_cast<bool8_t *>(data));
    break;
  case id_int8:
    yaml_decode(node, *reinterpret_cast<int8_t *>(data));
    break;
  case id_int16:
    yaml_decode(node, *reinterpret_cast<int16_t *>(data));
    break;
  case id_int32:
    yaml_decode(node, *reinterpret_cast<int32_t *>(data));
    break;
  case id_int64:
    yaml_decode(node, *reinterpret_cast<int64_t *>(data));
    break;
  case id_uint8:
    yaml_decode(node, *reinterpret_cast<uint8_t *>(data));
    break;
  case id_uint16:
    yaml_decode(node, *reinterpret_cast<uint16_t *>(data));
    break;
  case id_uint32:
    yaml_decode(node, *reinterpret_cast<uint32_t *>(data));
    break;
  case id_uint64:
    yaml_decode(node, *reinterpret_cast<uint64_t *>(data));
    break;
  case id_float32:
    yaml_decode(node, *reinterpret_cast<float32_t *>(data));
    break;
  case id_float64:
    yaml_decode(node, *reinterpret_cast<float64_t *>(data));
    break;
  case id_complex64:
    yaml_decode(node, *reinterpret_cast<complex64_t *>(data));
    break;
  case id_complex128:
    yaml_decode(node, *reinterpret_cast<complex128_t *>(data));
    break;
  // case id_ascii
  // case id_ucs4
  default:
    assert(0);
  }
}

YAML::Node emit_scalar(const unsigned char *data,
                       scalar_type_id_t scalar_type_id) {
  YAML::Node node;
  switch (scalar_type_id) {
  case id_bool8:
    node = yaml_encode(*reinterpret_cast<const bool8_t *>(data));
    break;
  case id_int8:
    node = yaml_encode(*reinterpret_cast<const int8_t *>(data));
    break;
  case id_int16:
    node = yaml_encode(*reinterpret_cast<const int16_t *>(data));
    break;
  case id_int32:
    node = yaml_encode(*reinterpret_cast<const int32_t *>(data));
    break;
  case id_int64:
    node = yaml_encode(*reinterpret_cast<const int64_t *>(data));
    break;
  case id_uint8:
    node = yaml_encode(*reinterpret_cast<const uint8_t *>(data));
    break;
  case id_uint16:
    node = yaml_encode(*reinterpret_cast<const uint16_t *>(data));
    break;
  case id_uint32:
    node = yaml_encode(*reinterpret_cast<const uint32_t *>(data));
    break;
  case id_uint64:
    node = yaml_encode(*reinterpret_cast<const uint64_t *>(data));
    break;
  case id_float32:
    node = yaml_encode(*reinterpret_cast<const float32_t *>(data));
    break;
  case id_float64:
    node = yaml_encode(*reinterpret_cast<const float64_t *>(data));
    break;
  case id_complex64:
    node = yaml_encode(*reinterpret_cast<const complex64_t *>(data));
    break;
  case id_complex128:
    node = yaml_encode(*reinterpret_cast<const complex128_t *>(data));
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

field_t::field_t(const string &name, const shared_ptr<datatype_t> &datatype,
                 // bool have_byteorder, byteorder_t byteorder,
                 const vector<int64_t> &shape)
    : name(name), datatype(datatype),
      // have_byteorder(have_byteorder), byteorder(byteorder),
      shape(shape) {
  assert(datatype);
}

field_t::field_t(const reader_state &rs, const YAML::Node &node) { assert(0); }

field_t::field_t(const copy_state &cs, const field_t &field) { assert(0); }

YAML::Node field_t::to_yaml(writer_state &ws) const {
  YAML::Node node;
  if (!name.empty())
    node["name"] = name;
  node["datatype"] = datatype->to_yaml(ws);
  // byteorder
  if (!shape.empty())
    node["shape"] = shape;
  return node;
}

datatype_t::datatype_t(scalar_type_id_t scalar_type_id)
    : is_scalar(true), scalar_type_id(scalar_type_id) {}

datatype_t::datatype_t(const vector<shared_ptr<field_t>> &fields)
    : is_scalar(false), fields(fields) {}

datatype_t::datatype_t(vector<shared_ptr<field_t>> &&fields)
    : is_scalar(false), fields(move(fields)) {}

size_t datatype_t::type_size() const {
  if (is_scalar)
    return get_scalar_type_size(scalar_type_id);
  size_t size = 0;
  for (const auto &field : fields)
    size += field->datatype->type_size();
  return size;
}

datatype_t::datatype_t(const reader_state &rs, const YAML::Node &node) {
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

YAML::Node datatype_t::to_yaml(writer_state &ws) const {
  if (is_scalar)
    return yaml_encode(scalar_type_id);
  YAML::Node node;
  for (const auto &field : fields)
    node.push_back(field->to_yaml(ws));
  return node;
}

void parse_scalar(const YAML::Node &node, unsigned char *data,
                  const shared_ptr<datatype_t> &datatype) {
  if (datatype->is_scalar)
    return parse_scalar(node, data, datatype->scalar_type_id);
  unsigned char *ptr = data;
  for (const auto &field : datatype->fields) {
    parse_scalar(node, ptr, field->datatype);
    ptr += field->datatype->type_size();
  }
}

YAML::Node emit_scalar(const unsigned char *data,
                       const shared_ptr<datatype_t> &datatype) {
  if (datatype->is_scalar)
    return emit_scalar(data, datatype->scalar_type_id);
  YAML::Node node;
  node.SetStyle(YAML::EmitterStyle::Flow);
  const unsigned char *ptr = data;
  for (const auto &field : datatype->fields) {
    node.push_back(emit_scalar(ptr, field->datatype));
    ptr += field->datatype->type_size();
  }
  return node;
}

////////////////////////////////////////////////////////////////////////////////

// Multi-dimensional array

void parse_inline_array_nd(const YAML::Node &node,
                           const shared_ptr<datatype_t> &datatype,
                           const vector<int64_t> &shape, int rank,
                           vector<unsigned char> &data) {
  assert(rank >= 0);
  assert(shape.size() >= rank);
  if (rank == 0) {
    assert(node.IsScalar());
    size_t oldsize = data.size();
    data.resize(oldsize + datatype->type_size());
    parse_scalar(node, &data[oldsize], datatype);
    return;
  }
  int64_t size = shape.at(rank - 1);
  assert(node.IsSequence());
  assert(node.size() == size);
  for (YAML::const_iterator ni = node.begin(), ne = node.end(); ni != ne; ++ni)
    parse_inline_array_nd(*ni, datatype, shape, rank - 1, data);
}

void parse_inline_array(const YAML::Node &node,
                        shared_ptr<generic_blob_t> &data,
                        const bool have_datatype,
                        shared_ptr<datatype_t> &datatype, const bool have_shape,
                        vector<int64_t> &shape) {
  if (!have_shape) {
    // determine shape
    shape.clear();
    YAML::Node n = node;
    while (n.IsSequence()) {
      shape.insert(shape.begin(), n.size());
      // This method does not work if the array size is zero in one dimension
      if (shape[0] == 0)
        break;
      n = n[0];
    }
    assert(n.IsScalar());
  }
  int64_t npoints = 1;
  for (size_t d = 0; d < shape.size(); ++d)
    npoints *= shape[d];
  vector<unsigned char> data1;
  if (!have_datatype) {
    // determine while parsing datatype
    try {
      datatype = make_shared<datatype_t>(id_int64);
      data1.reserve(npoints * datatype->type_size());
      parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
    } catch (YAML::RepresentationException) {
      try {
        datatype = make_shared<datatype_t>(id_float64);
        data1.reserve(npoints * datatype->type_size());
        parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
      } catch (YAML::RepresentationException) {
        try {
          datatype = make_shared<datatype_t>(id_complex128);
          data1.reserve(npoints * datatype->type_size());
          parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
        } catch (YAML::RepresentationException) {
          // bool8_t
          // ucs4_t
          assert(0);
        }
      }
    }
  } else {
    // parse data
    data1.reserve(npoints * datatype->type_size());
    parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
  }
  data = make_shared<blob_t<unsigned char>>(compression_t::none, move(data1));
}

YAML::Node emit_inline_array(const unsigned char *data,
                             const shared_ptr<datatype_t> &datatype,
                             const vector<int64_t> &shape,
                             const vector<int64_t> &strides) {
  size_t rank = shape.size();
  assert(strides.size() == rank);
  if (rank == 0) {
    // 0-dimensional array
    YAML::Node node;
    node.SetStyle(YAML::EmitterStyle::Flow);
    // node = data.at(offset);
    node = emit_scalar(data, datatype);
    return node;
  }
  if (rank == 1) {
    // 1-dimensional array
    YAML::Node node;
    // node.SetStyle(YAML::EmitterStyle::Flow);
    for (size_t i = 0; i < shape.at(0); ++i)
      node[i] = emit_scalar(data + i * strides.at(0), datatype);
    return node;
  }
  // multi-dimensional array
  YAML::Node node;
  // TODO: Try emitting these as Flow, with a Newline at the end
  vector<int64_t> shape1(rank - 1);
  for (size_t d = 0; d < rank - 1; ++d)
    shape1.at(d) = shape.at(d + 1);
  vector<int64_t> strides1(rank - 1);
  for (size_t d = 0; d < rank - 1; ++d)
    strides1.at(d) = strides.at(d + 1);
  for (size_t i = 0; i < shape.at(0); ++i)
    node[i] =
        emit_inline_array(data + i * strides.at(0), datatype, shape1, strides1);
  return node;
}

// (Incidentally, this spells "SBLK", with the highest bit of the "S" set to
// one)
constexpr array<unsigned char, 4> block_magic_token{0xd3, 0x42, 0x4c, 0x4b};

template <typename T> void input(istream &is, T &data) {
  // Always input in big-endian as required for the header
  for (ptrdiff_t i = sizeof(T) - 1; i >= 0; --i) {
    unsigned char ch;
    is.read(reinterpret_cast<char *>(&ch), 1);
    reinterpret_cast<unsigned char *>(&data)[i] = ch;
  }
}

shared_ptr<generic_blob_t> read_block(istream &is) {
  // block_magic_token
  array<unsigned char, 4> token;
  for (auto &ch : token)
    input(is, ch);
  if (token != block_magic_token) {
    is.seekg(-int64_t(token.size()), ios_base::cur);
    return nullptr;
  }
  // header_size
  uint16_t header_size;
  input(is, header_size);
  auto header_prefix_end = is.tellg();
  // flags
  uint32_t flags;
  input(is, flags);
  assert(flags == 0);
  // compression
  array<unsigned char, 4> comp;
  for (auto &ch : comp)
    input(is, ch);
  compression_t compression;
  if ((comp == array<unsigned char, 4>{0, 0, 0, 0}))
    compression = compression_t::none;
  else if ((comp == array<unsigned char, 4>{'b', 'z', 'p', '2'}))
    compression = compression_t::bzip2;
  else if ((comp == array<unsigned char, 4>{'z', 'l', 'i', 'b'}))
    compression = compression_t::zlib;
  else
    assert(0);
  // allocated_space
  uint64_t allocated_space;
  input(is, allocated_space);
  // used_space
  uint64_t used_space;
  input(is, used_space);
  assert(used_space >= allocated_space);
  // data_space
  uint64_t data_space;
  input(is, data_space);
  // checksum
  array<unsigned char, 16> checksum;
  for (auto &ch : checksum)
    input(is, ch);
  // finish reading header
  auto header_end = is.tellg();
  int64_t header_read = header_end - header_prefix_end;
  assert(header_read <= header_size);
  if (header_read < header_size)
    is.seekg(header_size - header_read, ios_base::cur);
  // read data
  vector<unsigned char> indata(allocated_space);
  is.read(reinterpret_cast<char *>(indata.data()), indata.size());
  // TODO: check checksum
  // decompress data
  vector<unsigned char> data;
  switch (compression) {
  case compression_t::none:
    assert(data_space == allocated_space);
    data = move(indata);
    break;
#ifdef HAVE_BZIP2
  case compression_t::bzip2: {
    data.resize(data_space);
    bz_stream strm;
    strm.bzalloc = NULL;
    strm.bzfree = NULL;
    strm.opaque = NULL;
    BZ2_bzDecompressInit(&strm, 0, 0);
    strm.next_in =
        reinterpret_cast<char *>(const_cast<unsigned char *>(indata.data()));
    strm.next_out = reinterpret_cast<char *>(data.data());
    uint64_t avail_in = indata.size();
    uint64_t avail_out = data.size();
    for (;;) {
      uint64_t this_avail_in =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_in);
      uint64_t this_avail_out =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_out);
      strm.avail_in = this_avail_in;
      strm.avail_out = this_avail_out;
      int iret = BZ2_bzDecompress(&strm);
      avail_in -= this_avail_in - strm.avail_in;
      avail_out -= this_avail_out - strm.avail_out;
      if (iret == BZ_STREAM_END)
        break;
      assert(iret == BZ_OK);
    }
    BZ2_bzDecompressEnd(&strm);
    assert(avail_in == 0);
    assert(avail_out == 0);
    break;
  }
#endif
#ifdef HAVE_ZLIB
  case compression_t::zlib: {
    data.resize(data_space);
    z_stream strm;
    strm.zalloc = NULL;
    strm.zfree = NULL;
    strm.opaque = NULL;
    inflateInit(&strm);
    strm.next_in = const_cast<unsigned char *>(indata.data());
    strm.next_out = data.data();
    uint64_t avail_in = indata.size();
    uint64_t avail_out = data.size();
    for (;;) {
      uint64_t this_avail_in =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_in);
      uint64_t this_avail_out =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_out);
      strm.avail_in = this_avail_in;
      strm.avail_out = this_avail_out;
      int iret = inflate(&strm, Z_NO_FLUSH);
      avail_in -= this_avail_in - strm.avail_in;
      avail_out -= this_avail_out - strm.avail_out;
      if (iret == Z_STREAM_END)
        break;
      assert(iret == Z_OK);
    }
    inflateEnd(&strm);
    assert(avail_in == 0);
    assert(avail_out == 0);
    break;
  }
#endif
  default:
    assert(0);
  }

  // skip padding
  if (used_space > allocated_space)
    is.seekg(used_space - allocated_space, ios_base::cur);
  return make_shared<blob_t<unsigned char>>(compression, move(data));
}

template <typename T>
void output(vector<unsigned char> &header, const T &data) {
  // Always output in big-endian as required for the header
  for (ptrdiff_t i = sizeof(T) - 1; i >= 0; --i)
    header.push_back(reinterpret_cast<const unsigned char *>(&data)[i]);
}

// TODO: stream the block (e.g. when compressing), then write the correct header
// later
void ndarray::write_block(ostream &os) const {
  vector<unsigned char> header;
  // block_magic_token
  for (auto ch : block_magic_token)
    output(header, ch);
  // header_size (not yet known)
  auto header_size_pos = header.size();
  uint16_t unknown_header_size = 0;
  output(header, unknown_header_size);
  auto header_prefix_length = header.size();
  // flags
  uint32_t flags = 0;
  output(header, flags);
  // compression
  array<unsigned char, 4> comp;
  shared_ptr<generic_blob_t> outdata;
  switch (data->get_compression()) {
  case compression_t::none:
    comp = {0, 0, 0, 0};
    outdata = data;
    break;
  case compression_t::bzip2: {
#ifdef HAVE_BZIP2
    comp = {'b', 'z', 'p', '2'};
    // Allocate 600 bytes plus 1% more
    outdata = make_shared<blob_t<unsigned char>>(
        data->get_compression(),
        vector<unsigned char>(600 + data->bytes() +
                              (data->bytes() + 99) / 100));
    const int level = 9;
    bz_stream strm;
    strm.bzalloc = NULL;
    strm.bzfree = NULL;
    strm.opaque = NULL;
    BZ2_bzCompressInit(&strm, level, 0, 0);
    strm.next_in = reinterpret_cast<char *>(const_cast<void *>(data->ptr()));
    strm.next_out = reinterpret_cast<char *>(outdata->ptr());
    uint64_t avail_in = data->bytes();
    uint64_t avail_out = outdata->bytes();
    for (;;) {
      uint64_t this_avail_in =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_in);
      uint64_t this_avail_out =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_out);
      strm.avail_in = this_avail_in;
      strm.avail_out = this_avail_out;
      auto state = this_avail_in < avail_in ? BZ_RUN : BZ_FINISH;
      int iret = BZ2_bzCompress(&strm, state);
      avail_in -= this_avail_in - strm.avail_in;
      avail_out -= this_avail_out - strm.avail_out;
      if (iret == BZ_STREAM_END)
        break;
      assert(iret == BZ_RUN_OK);
    }
    assert(avail_in == 0);
    outdata->resize(outdata->bytes() - avail_out);
    if (outdata->bytes() >= data->bytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = data;
    }
#else
    // Fall back to no compression if bzip2 is not available
    comp = {0, 0, 0, 0};
    outdata = data;
#endif
    break;
  }
  case compression_t::zlib: {
#ifdef HAVE_ZLIB
    comp = {'z', 'l', 'i', 'b'};
    // Allocate 6 bytes plus 5 bytes per 16 kByte more
    outdata = make_shared<blob_t<unsigned char>>(
        data->get_compression(),
        vector<unsigned char>(
            (6 + data->bytes() + (data->bytes() + 16383) / 16384 * 5)));
    const int level = 9;
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int iret = deflateInit(&strm, level);
    assert(iret == Z_OK);
    strm.next_in =
        reinterpret_cast<unsigned char *>(const_cast<void *>(data->ptr()));
    strm.next_out = reinterpret_cast<unsigned char *>(outdata->ptr());
    uint64_t avail_in = data->bytes();
    uint64_t avail_out = outdata->bytes();
    for (;;) {
      uint64_t this_avail_in =
          min(uint64_t(numeric_limits<uInt>::max()), avail_in);
      uint64_t this_avail_out =
          min(uint64_t(numeric_limits<uInt>::max()), avail_out);
      strm.avail_in = this_avail_in;
      strm.avail_out = this_avail_out;
      auto state = this_avail_in < avail_in ? Z_NO_FLUSH : Z_FINISH;
      int iret = deflate(&strm, state);
      avail_in -= this_avail_in - strm.avail_in;
      avail_out -= this_avail_out - strm.avail_out;
      if (iret == Z_STREAM_END)
        break;
      assert(iret == Z_OK);
    }
    assert(avail_in == 0);
    outdata->resize(outdata->bytes() - avail_out);
    if (outdata->bytes() >= data->bytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = data;
    }
#else
    // Fall back to no compression if zlib is not available
    comp = {0, 0, 0, 0};
    outdata = data;
#endif
    break;
  }
  default:
    assert(0);
  }
  for (auto ch : comp)
    output(header, ch);
  // allocated_space
  uint64_t allocated_space = outdata->bytes();
  output(header, allocated_space);
  // used_space
  uint64_t used_space = allocated_space; // no padding
  output(header, used_space);
  // data_space
  uint64_t data_space = data->bytes();
  output(header, data_space);
  // checksum
  array<unsigned char, 16> checksum;
#if HAVE_OPENSSL
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, outdata->ptr(), outdata->bytes());
  MD5_Final(checksum.data(), &ctx);
#else
  checksum = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#endif
  for (auto ch : checksum)
    output(header, ch);
  // fill in header_size
  uint16_t header_size = header.size() - header_prefix_length;
  vector<unsigned char> header_size_buf;
  output(header_size_buf, header_size);
  for (size_t p = 0; p < header_size_buf.size(); ++p)
    header.at(header_size_pos + p) = header_size_buf.at(p);
  // write header
  os.write(reinterpret_cast<const char *>(header.data()), header.size());
  // write data
  os.write(reinterpret_cast<const char *>(outdata->ptr()), outdata->bytes());
  // write padding
  vector<char> padding(allocated_space - used_space);
  os.write(padding.data(), padding.size());
}

ndarray::ndarray(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/ndarray-1.0.0");
  if (node["source"].IsDefined())
    block_format = block_format_t::block;
  else if (node["data"].IsDefined())
    block_format = block_format_t::inline_array;
  else
    assert(0);
  switch (block_format) {
  case block_format_t::block: {
    int64_t source;
    yaml_decode(node["source"], source);
    // compression = compression_t::none;
    datatype = make_shared<datatype_t>(rs, node["datatype"]);
    byteorder_t byteorder;
    yaml_decode(node["byteorder"], byteorder);
    assert(byteorder == host_byteorder());
    yaml_decode(node["shape"], shape);
    if (node["offset"].IsDefined())
      yaml_decode(node["offset"], offset);
    else
      offset = 0;
    if (node["strides"].IsDefined()) {
      yaml_decode(node["strides"], strides);
    } else {
      int rank = shape.size();
      strides.resize(rank);
      int64_t str = datatype->type_size();
      for (int d = rank - 1; d >= 0; --d) {
        strides.at(d) = str;
        str *= shape.at(d);
      }
    }
    data = rs.get_block(source);
    break;
  }
  case block_format_t::inline_array: {
    // not yet implemented
    bool have_datatype = node["datatype"].IsDefined();
    if (have_datatype)
      datatype = make_shared<datatype_t>(rs, node["datatype"]);
    bool have_shape = node["shape"].IsDefined();
    if (have_shape)
      yaml_decode(node["shape"], shape);
    parse_inline_array(node["data"], data, have_datatype, datatype, have_shape,
                       shape);
    offset = 0;
    int rank = shape.size();
    strides.resize(rank);
    int64_t str = datatype->type_size();
    for (int d = rank - 1; d >= 0; --d) {
      strides.at(d) = str;
      str *= shape.at(d);
    }
    break;
  }
  default:
    assert(0);
  }
}

ndarray::ndarray(const copy_state &cs, const ndarray &arr) : ndarray(arr) {
  if (cs.set_block_format)
    block_format = cs.block_format;
  // TODO: handle this
  // if (cs.set_compression)
  //   compression = cs.compression;
}

YAML::Node ndarray::to_yaml(writer_state &ws) const {
  YAML::Node node;
  node.SetTag("tag:stsci.edu:asdf/core/ndarray-1.0.0");
  if (block_format == block_format_t::block) {
    // source
    const auto &self = *this;
    uint64_t idx = ws.add_task([=](ostream &os) { self.write_block(os); });
    node["source"] = idx;
  } else {
    // data
    node["data"] = emit_inline_array(
        static_cast<const unsigned char *>(data->ptr()) + offset, datatype,
        shape, strides);
  }
  // mask
  assert(mask.empty());
  // datatype
  node["datatype"] = datatype->to_yaml(ws);
  if (block_format == block_format_t::block) {
    // byteorder
    node["byteorder"] = yaml_encode(host_byteorder());
  }
  // shape
  node["shape"] = shape;
  node["shape"].SetStyle(YAML::EmitterStyle::Flow);
  if (block_format == block_format_t::block) {
    // offset
    node["offset"] = offset;
    // strides
    node["strides"] = strides;
    node["strides"].SetStyle(YAML::EmitterStyle::Flow);
  }
  return node;
}

////////////////////////////////////////////////////////////////////////////////

// Table and Column

column::column(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/column-1.0.0");
  name = node["name"].Scalar();
  data = make_shared<ndarray>(rs, node["data"]);
  if (node["description"].IsDefined())
    description = node["description"].Scalar();
}

column::column(const copy_state &cs, const column &col) : column(col) {}

YAML::Node column::to_yaml(writer_state &ws) const {
  YAML::Node node;
  node.SetTag("tag:stsci.edu:asdf/core/column-1.0.0");
  node["name"] = name;
  node["data"] = data->to_yaml(ws);
  if (!description.empty())
    node["description"] = description;
  return node;
}

table::table(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/table-1.0.0");
  for (const auto &col : node["columns"])
    columns.push_back(make_shared<column>(rs, col));
}

table::table(const copy_state &cs, const table &tab) {
  for (const auto &col : tab.columns)
    columns.push_back(make_shared<column>(cs, *col));
}

YAML::Node table::to_yaml(writer_state &ws) const {
  YAML::Node cols;
  for (size_t i = 0; i < columns.size(); ++i)
    cols[i] = columns[i]->to_yaml(ws);
  YAML::Node node;
  node.SetTag("tag:stsci.edu:asdf/core/table-1.0.0");
  node["columns"] = move(cols);
  return node;
}

////////////////////////////////////////////////////////////////////////////////

// Group and Entry

entry::entry(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:github.com/eschnett/asdf-cxx/core/entry-1.0.0");
  name = node["name"].Scalar();
  if (node["group"].IsDefined())
    grp = make_shared<group>(rs, node["group"]);
  if (node["data"].IsDefined())
    arr = make_shared<ndarray>(rs, node["data"]);
  assert(bool(grp) + bool(arr) == 1);
  if (node["description"].IsDefined())
    description = node["description"].Scalar();
}

entry::entry(const copy_state &cs, const entry &ent)
    : name(ent.name), description(ent.description) {
  if (ent.grp)
    grp = make_shared<group>(cs, *ent.grp);
  if (ent.arr)
    arr = make_shared<ndarray>(cs, *ent.arr);
}

YAML::Node entry::to_yaml(writer_state &ws) const {
  YAML::Node node;
  node.SetTag("tag:github.com/eschnett/asdf-cxx/core/entry-1.0.0");
  node["name"] = name;
  if (grp)
    node["group"] = grp->to_yaml(ws);
  if (arr)
    node["data"] = arr->to_yaml(ws);
  if (!description.empty())
    node["description"] = description;
  return node;
}

group::group(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:github.com/eschnett/asdf-cxx/core/group-1.0.0");
  for (const auto &ent : node)
    entries[ent.first.Scalar()] = make_shared<entry>(rs, ent.second);
}

group::group(const copy_state &cs, const group &grp) {
  for (const auto &kv : grp.entries)
    entries[kv.first] = make_shared<entry>(cs, *kv.second);
}

YAML::Node group::to_yaml(writer_state &ws) const {
  YAML::Node node;
  node.SetTag("tag:github.com/eschnett/asdf-cxx/core/group-1.0.0");
  for (const auto &kv : entries)
    node[kv.first] = kv.second->to_yaml(ws);
  return node;
}

////////////////////////////////////////////////////////////////////////////////

// ASDF

YAML::Node software(const string &name, const string &author,
                    const string &homepage, const string &version) {
  YAML::Node node;
  node.SetTag("tag:stsci.edu:asdf/core/software-1.0.0");
  assert(!name.empty());
  node[name] = name;
  if (!author.empty())
    node["author"] = author;
  if (!homepage.empty())
    node["homepage"] = homepage;
  assert(!version.empty());
  node["version"] = version;
  return node;
}

asdf::asdf(const reader_state &rs, const YAML::Node &node) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/asdf-1.0.0" ||
         node.Tag() == "tag:stsci.edu:asdf/core/asdf-1.1.0");
  // TODO: read software
  for (const auto &kv : node) {
    const auto &key = kv.first.Scalar();
    if (key == "asdf_library") {
      // TODO
    } else if (key == "group") {
      grp = make_shared<group>(rs, node["group"]);
    } else if (key == "history") {
      // TODO
      // } else if (key == "table") {
      //   tab = make_shared<table>(rs, node["table"]);
    } else {
      data[key] = make_shared<ndarray>(rs, kv.second);
    }
  }
}

asdf::asdf(const copy_state &cs, const asdf &project) {
  for (const auto &kv : project.data) {
    const auto &key = kv.first;
    data[key] = make_shared<ndarray>(cs, *kv.second);
  }
  // if (project.tab)
  //   tab = make_shared<table>(cs, *project.tab);
  if (project.grp)
    grp = make_shared<group>(cs, *project.grp);
}

YAML::Node asdf::to_yaml(writer_state &ws) const {
  const auto &asdf_library =
      software("asdf-cxx", "Erik Schnetter",
               "https://github.com/eschnett/asdf-cxx", ASDF_VERSION);
  YAML::Node node;
  node.SetTag("tag:stsci.edu:asdf/core/asdf-1.1.0");
  node["asdf_library"] = asdf_library;
  for (const auto &kv : data) {
    const auto &key = kv.first;
    node[key] = kv.second->to_yaml(ws);
  }
  // if (tab)
  //   node["table"] = tab->to_yaml(ws);
  if (grp)
    node["group"] = grp->to_yaml(ws);
  // node.SetStyle(YAML::EmitterStyle::BeginDoc);
  // node.SetStyle(YAML::EmitterStyle::EndDoc);
  return node;
}

asdf::asdf(istream &is) {
  // TODO: stream the file instead
  ostringstream doc;
  for (;;) {
    string line;
    getline(is, line);
    doc << line << "\n";
    if (line == "...")
      break;
  }
  YAML::Node node = YAML::Load(doc.str());
  reader_state rs(is);
  auto project = asdf(rs, node);
  data = move(project.data);
  // tab = move(project.tab);
  grp = move(project.grp);
}

asdf asdf::copy(const copy_state &cs) const { return asdf(cs, *this); }

void asdf::write(ostream &os) const {
  writer_state ws;
  // TODO: Use YAML::Emitter(os) instead
  const auto &node = to_yaml(ws);
  os << "#ASDF " << asdf_format_version << "\n"
     << "#ASDF_STANDARD " << asdf_standard_version << "\n"
     << "# This is an ASDF file <https://asdf-standard.readthedocs.io/>.\n"
     << "%YAML 1.1\n"
     // << "%TAG ! tag:stsci.edu:asdf/\n"
     // << "%TAG !! tag:github.com/eschnett/asdf-cxx/\n"
     << "---\n"
     << node << "\n"
     << "...\n";
  ws.flush(os);
}

} // namespace ASDF
