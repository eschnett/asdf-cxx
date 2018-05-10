#include "asdf.hpp"
#include "config.hpp"

#include <yaml-cpp/yaml.h>

#include <array>
#include <complex>
#include <map>
#include <sstream>
#include <vector>

namespace ASDF {

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

////////////////////////////////////////////////////////////////////////////////

template <typename T>
void output(vector<unsigned char> &header, const T &data) {
  // Always output in big-endian as required for the header
  for (ptrdiff_t i = sizeof(T) - 1; i >= 0; --i)
    header.push_back(*reinterpret_cast<const unsigned char *>((&data)[i]));
}

void write_block(ostream &os, const shared_ptr<ndarray> &arr) {
  vector<unsigned char> header;
  // block_magic_token
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
  for (const auto &arr : ndarrays)
    write_block(os, arr);
  ndarrays.clear();
}

////////////////////////////////////////////////////////////////////////////////

YAML::Node software(const string &name, const optional<string> &author,
                    const optional<string> &homepage, const string &version) {
  YAML::Node node;
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
               "https://github.com/eschnett/asdf-cpp", ASDF_VERSION);
  YAML::Node node;
  node["asdf_library"] = asdf_library;
  return node;
}

////////////////////////////////////////////////////////////////////////////////

void write_asdf(ostream &os, const asdf &objs) {
  writer_state ws;
  const auto &node = objs.to_yaml(ws);
  os << "#ASDF 1.0.0\n"
     << "#\n"
     << "# This is an ASDF file <https://asdf-standard.readthedocs.io/>.\n"
     << "#\n"
     << "%YAML 1.1\n"
     << "%TAG ! tag:stsci.edu:asdf/\n"
     << "--- !core/asdf-1.0.0\n"
     << node << "\n"
     << "...\n";
  ws.flush(os);
}

} // namespace ASDF
