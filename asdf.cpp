#include "asdf.hpp"
#include "config.hpp"

#include <yaml-cpp/emittermanip.h>
#include <yaml-cpp/emitterstyle.h>
#include <yaml-cpp/yaml.h>

#include <array>
#include <cmath>
#include <complex>
#include <map>
#include <sstream>
#include <vector>

namespace ASDF {

const string asdf_format_version = "1.0.0";
const string asdf_standard_version = "1.1.0";

////////////////////////////////////////////////////////////////////////////////

template <typename T>
void output(vector<unsigned char> &header, const T &data) {
  // Always output in big-endian as required for the header
  for (ptrdiff_t i = sizeof(T) - 1; i >= 0; --i)
    header.push_back(reinterpret_cast<const unsigned char *>(&data)[i]);
}

void write_block(ostream &os, const shared_ptr<const ndarray> &arr) {
  vector<unsigned char> header;
  // block_magic_token
  // (This spells "SBLK", with the highest bit of the "S" set to one)
  const array<unsigned char, 4> block_magic_token{0xd3, 0x42, 0x4c, 0x4b};
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
  array<unsigned char, 4> compression{0, 0, 0, 0};
  for (auto ch : compression)
    output(header, ch);
  // allocated_space
  uint64_t allocated_space = arr->data_size();
  output(header, allocated_space);
  // used_space
  uint64_t used_space = allocated_space; // no padding
  output(header, used_space);
  // data_space
  uint64_t data_space = used_space;
  output(header, data_space);
  // checksum
  array<unsigned char, 16> checksum{0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 0, 0, 0, 0, 0};
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
  os.write(reinterpret_cast<const char *>(arr->data_ptr()), arr->data_size());
  // write padding
  vector<char> padding(allocated_space - used_space);
  os.write(padding.data(), padding.size());
}

void writer_state::flush(ostream &os) {
  if (ndarrays.empty())
    return;
  YAML::Emitter index;
  index << YAML::Flow;
  index << YAML::BeginSeq;
  for (const auto &arr : ndarrays) {
    index << os.tellp();
    write_block(os, arr);
  }
  index << YAML::EndSeq;
  ndarrays.clear();
  os << "#ASDF BLOCK INDEX\n"
     << "%YAML 1.1\n"
     << "---\n"
     << index.c_str() << "\n"
     << "...\n";
}

////////////////////////////////////////////////////////////////////////////////

// Check consistency between id enum and variant index
static_assert(is_same<get_scalar_type_t<id_bool8>, bool8_t>::value);
static_assert(is_same<get_scalar_type_t<id_int8>, int8_t>::value);
static_assert(is_same<get_scalar_type_t<id_int16>, int16_t>::value);
static_assert(is_same<get_scalar_type_t<id_int32>, int32_t>::value);
static_assert(is_same<get_scalar_type_t<id_int64>, int64_t>::value);
static_assert(is_same<get_scalar_type_t<id_uint8>, uint8_t>::value);
static_assert(is_same<get_scalar_type_t<id_uint16>, uint16_t>::value);
static_assert(is_same<get_scalar_type_t<id_uint32>, uint32_t>::value);
static_assert(is_same<get_scalar_type_t<id_uint64>, uint64_t>::value);
static_assert(is_same<get_scalar_type_t<id_float32>, float32_t>::value);
static_assert(is_same<get_scalar_type_t<id_float64>, float64_t>::value);
static_assert(is_same<get_scalar_type_t<id_complex64>, complex64_t>::value);
static_assert(is_same<get_scalar_type_t<id_complex128>, complex128_t>::value);
static_assert(is_same<get_scalar_type_t<id_ascii>, ascii_t>::value);
static_assert(is_same<get_scalar_type_t<id_ucs4>, ucs4_t>::value);

static_assert(get_scalar_type_id<bool8_t>::value == id_bool8);
static_assert(get_scalar_type_id<int8_t>::value == id_int8);
static_assert(get_scalar_type_id<int16_t>::value == id_int16);
static_assert(get_scalar_type_id<int32_t>::value == id_int32);
static_assert(get_scalar_type_id<int64_t>::value == id_int64);
static_assert(get_scalar_type_id<uint8_t>::value == id_uint8);
static_assert(get_scalar_type_id<uint16_t>::value == id_uint16);
static_assert(get_scalar_type_id<uint32_t>::value == id_uint32);
static_assert(get_scalar_type_id<uint64_t>::value == id_uint64);
static_assert(get_scalar_type_id<float32_t>::value == id_float32);
static_assert(get_scalar_type_id<float64_t>::value == id_float64);
static_assert(get_scalar_type_id<complex64_t>::value == id_complex64);
static_assert(get_scalar_type_id<complex128_t>::value == id_complex128);
static_assert(get_scalar_type_id<ascii_t>::value == id_ascii);
static_assert(get_scalar_type_id<ucs4_t>::value == id_ucs4);

string yaml_encode(scalar_type_id_t scalar_type_id) {
  switch (scalar_type_id) {
  case id_bool8:
    return "bool8";
  case id_int8:
    return "int8";
  case id_int16:
    return "int16";
  case id_int32:
    return "int32";
  case id_int64:
    return "int64";
  case id_uint8:
    return "uint8";
  case id_uint16:
    return "uint16";
  case id_uint32:
    return "uint32";
  case id_uint64:
    return "uint64";
  case id_float32:
    return "float32";
  case id_float64:
    return "float64";
  case id_complex64:
    return "complex64";
  case id_complex128:
    return "complex128";
    // case id_ascii
    // case id_ucs4
  }
  assert(0);
}

template <typename T> string yaml_encode(const complex<T> &val) {
  YAML::Emitter re;
  re << val.real();
  YAML::Emitter im;
  im << val.imag();
  ostringstream buf;
  buf << re.c_str();
  if (im.c_str()[0] != '-')
    buf << "+";
  buf << im.c_str() << "i";
  return buf.str();
}

YAML::Node emit_scalar(const void *data, scalar_type_id_t scalar_type_id) {
  YAML::Node node;
  switch (scalar_type_id) {
  case id_bool8:
    node = *reinterpret_cast<const bool8_t *>(data);
    break;
  case id_int8:
    node = *reinterpret_cast<const int8_t *>(data);
    break;
  case id_int16:
    node = *reinterpret_cast<const int16_t *>(data);
    break;
  case id_int32:
    node = *reinterpret_cast<const int32_t *>(data);
    break;
  case id_int64:
    node = *reinterpret_cast<const int64_t *>(data);
    break;
  case id_uint8:
    node = *reinterpret_cast<const uint8_t *>(data);
    break;
  case id_uint16:
    node = *reinterpret_cast<const uint16_t *>(data);
    break;
  case id_uint32:
    node = *reinterpret_cast<const uint32_t *>(data);
    break;
  case id_uint64:
    node = *reinterpret_cast<const uint64_t *>(data);
    break;
  case id_float32:
    node = *reinterpret_cast<const float32_t *>(data);
    break;
  case id_float64:
    node = *reinterpret_cast<const float64_t *>(data);
    break;
  case id_complex64: {
    node.SetTag("core/complex-1.0.0");
    node = yaml_encode(*reinterpret_cast<const complex64_t *>(data));
    break;
  }
  case id_complex128: {
    node.SetTag("core/complex-1.0.0");
    node = yaml_encode(*reinterpret_cast<const complex128_t *>(data));
    break;
  }
  // case id_ascii
  // case id_ucs4
  default:
    assert(0);
  }
  return node;
}

////////////////////////////////////////////////////////////////////////////////

enum class byteorder_t { big, little };

string yaml_encode(byteorder_t byteorder) {
  switch (byteorder) {
  case byteorder_t::big:
    return "big";
  case byteorder_t::little:
    return "little";
  }
  assert(0);
};

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

YAML::Node emit_inline_array(const vector<unsigned char> &data,
                             scalar_type_id_t scalar_type_id,
                             const vector<int64_t> &shape,
                             const vector<int64_t> &strides, int64_t offset) {
  size_t rank = shape.size();
  assert(strides.size() == rank);
  if (rank == 0) {
    // 0-dimensional array
    YAML::Node node;
    node.SetStyle(YAML::EmitterStyle::Flow);
    // node[0] = data.at(offset);
    node[0] = emit_scalar(&data[offset], scalar_type_id);
    return node;
  }
  if (rank == 1) {
    // 1-dimensional array
    YAML::Node node;
    node.SetStyle(YAML::EmitterStyle::Flow);
    for (size_t i = 0; i < shape.at(0); ++i)
      // node[i] = data.at(offset + i * strides.at(0));
      node[i] = emit_scalar(&data[offset + i * strides.at(0)], scalar_type_id);
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
    node[i] = emit_inline_array(data, scalar_type_id, shape1, strides1,
                                offset + i * strides.at(0));
  return node;
}

YAML::Node ndarray::to_yaml(writer_state &ws) const {
  YAML::Node node;
  node.SetTag("core/ndarray-1.0.0");
  if (block_format == block_format_t::block) {
    // source
    uint64_t idx = ws.add_block(shared_from_this());
    node["source"] = idx;
  } else {
    // data
    node["data"] =
        emit_inline_array(data, scalar_type_id, shape, strides, offset);
  }
  // mask
  assert(!mask);
  // datatype
  node["datatype"] = yaml_encode(scalar_type_id);
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

YAML::Node column::to_yaml(writer_state &ws) const {
  YAML::Node node;
  node.SetTag("core/column-1.0.0");
  node["name"] = name;
  node["data"] = data->to_yaml(ws);
  if (description)
    node["description"] = *description;
  return node;
}

YAML::Node table::to_yaml(writer_state &ws) const {
  YAML::Node cols;
  for (size_t i = 0; i < columns.size(); ++i)
    cols[i] = columns[i]->to_yaml(ws);
  YAML::Node node;
  node.SetTag("core/table-1.0.0");
  node["columns"] = cols;
  return node;
}

////////////////////////////////////////////////////////////////////////////////

YAML::Node software(const string &name, const optional<string> &author,
                    const optional<string> &homepage, const string &version) {
  YAML::Node node;
  node.SetTag("core/software-1.0.0");
  node[name] = name;
  if (author)
    node["author"] = *author;
  if (homepage)
    node["homepage"] = *homepage;
  node["version"] = version;
  return node;
}

YAML::Node asdf::to_yaml(writer_state &ws) const {
  const auto &asdf_library =
      software("asdf-cxx", "Erik Schnetter",
               "https://github.com/eschnett/asdf-cxx", ASDF_VERSION);
  YAML::Node tabs;
  for (size_t i = 0; i < tables.size(); ++i)
    tabs[i] = tables[i]->to_yaml(ws);
  YAML::Node node;
  // node.SetStyle(YAML::EmitterStyle::BeginDoc);
  // node.SetStyle(YAML::EmitterStyle::EndDoc);
  node.SetTag("core/asdf-1.0.0");
  node["asdf_library"] = asdf_library;
  node["tables"] = tabs;
  return node;
}

void asdf::write(ostream &os) const {
  writer_state ws;
  // TODO: Use YAML::Emitter(os) instead
  const auto &node = to_yaml(ws);
  os << "#ASDF " << asdf_format_version << "\n"
     << "#ASDF_STANDARD " << asdf_standard_version << "\n"
     << "# This is an ASDF file <https://asdf-standard.readthedocs.io/>.\n"
     << "%YAML 1.1\n"
     << "%TAG ! tag:stsci.edu:asdf/\n"
     << "---\n"
     << node << "\n"
     << "...\n";
  ws.flush(os);
}

} // namespace ASDF
