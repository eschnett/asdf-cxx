// asdf-read-check: print the values of every array in an ASDF file.
//
// A data-access oracle for the test suite: it walks the tree in a
// deterministic order and prints one line per ndarray,
//
//     <path>: <datatype> [<shape>] <value> <value> ...
//
// so that the values can be diffed against a committed expected output (see
// tests/expected/ and tests/conformance.cmake). Everything that could differ
// between platforms is normalised: floating-point values are printed with
// enough digits to round-trip, NaN always prints as "nan" regardless of its
// sign, and float16 is converted to float in software so that the output does
// not depend on whether the build has `_Float16`.
//
// Scalar arrays go through the library's `get_data_vector<T>()` wherever that
// accessor exists, so that the expected outputs test the library's data access
// rather than a reimplementation of it. Structured arrays, float16 and the
// types that no build is guaranteed to have go through
// `ndarray::get_data_bytes()`, which applies `offset`, `strides` and the byte
// order in the same way; the tool only formats the resulting host-order
// bytes.

#include <asdf/asdf.hxx>

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace ASDF;

namespace {

// Number formatting -----------------------------------------------------

string format_float(double val, int precision) {
  if (std::isnan(val))
    return "nan"; // never "-nan"; the sign of a NaN is not portable
  char buf[64];
  snprintf(buf, sizeof buf, "%.*g", precision, val);
  return buf;
}

string format_float32(float val) { return format_float(val, 9); }
string format_float64(double val) { return format_float(val, 17); }

string format_complex64(const complex<float> &val) {
  return "(" + format_float32(val.real()) + "," + format_float32(val.imag()) +
         ")";
}
string format_complex128(const complex<double> &val) {
  return "(" + format_float64(val.real()) + "," + format_float64(val.imag()) +
         ")";
}

// IEEE 754 binary16 -> float, in software
float half_to_float(uint16_t h) {
  const uint32_t sign = uint32_t(h >> 15) & 1;
  const uint32_t exponent = uint32_t(h >> 10) & 0x1f;
  const uint32_t mantissa = uint32_t(h) & 0x3ff;
  float result;
  if (exponent == 0)
    result = std::ldexp(float(mantissa), -24); // zero or subnormal
  else if (exponent == 0x1f)
    result = mantissa == 0 ? numeric_limits<float>::infinity()
                           : numeric_limits<float>::quiet_NaN();
  else
    result = std::ldexp(float(mantissa + 0x400), int(exponent) - 25);
  return sign ? -result : result;
}

// Datatype names --------------------------------------------------------

string shape_suffix(const vector<int64_t> &shape) {
  if (shape.empty())
    return "";
  ostringstream buf;
  buf << "[";
  for (size_t d = 0; d < shape.size(); ++d)
    buf << (d == 0 ? "" : ",") << shape.at(d);
  buf << "]";
  return buf.str();
}

string datatype_name(const datatype_t &datatype);

string field_name(const field_t &field) {
  return field.name + ":" + datatype_name(*field.datatype) +
         shape_suffix(field.shape);
}

string datatype_name(const datatype_t &datatype) {
  ostringstream buf;
  if (datatype.is_scalar) {
    // The string types carry their length; `[ascii, 5]` prints as `ascii(5)`
    // so that the brackets stay reserved for shapes
    if (datatype.scalar_type_id == id_ascii ||
        datatype.scalar_type_id == id_ucs4) {
      buf << (datatype.scalar_type_id == id_ascii ? "ascii" : "ucs4") << "("
          << datatype.string_length << ")";
      return buf.str();
    }
    buf << yaml_encode(datatype.scalar_type_id);
    return buf.str();
  }
  buf << "(";
  for (size_t f = 0; f < datatype.fields.size(); ++f)
    buf << (f == 0 ? "" : ",") << field_name(*datatype.fields.at(f));
  buf << ")";
  return buf.str();
}

// Byte-level access -----------------------------------------------------

string utf8_encode(const u32string &codes) {
  string result;
  for (const auto ch : codes) {
    const uint32_t code = ch;
    if (code < 0x80) {
      result += char(code);
    } else if (code < 0x800) {
      result += char(0xc0 | (code >> 6));
      result += char(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
      result += char(0xe0 | (code >> 12));
      result += char(0x80 | ((code >> 6) & 0x3f));
      result += char(0x80 | (code & 0x3f));
    } else {
      result += char(0xf0 | (code >> 18));
      result += char(0x80 | ((code >> 12) & 0x3f));
      result += char(0x80 | ((code >> 6) & 0x3f));
      result += char(0x80 | (code & 0x3f));
    }
  }
  return result;
}

string format_ucs4(const unsigned char *data, size_t length,
                   byteorder_t byteorder) {
  // Trailing null code units are padding, as in numpy's U dtype
  while (length > 0 && xtoh<uint32_t>(data + 4 * (length - 1), byteorder) == 0)
    --length;
  u32string codes;
  for (size_t i = 0; i < length; ++i)
    codes += char32_t(xtoh<uint32_t>(data + 4 * i, byteorder));
  return utf8_encode(codes);
}

// One scalar element, decoded from `data`
string format_element(const unsigned char *data,
                      scalar_type_id_t scalar_type_id, size_t elemsize,
                      byteorder_t byteorder) {
  switch (scalar_type_id) {
  case id_bool8:
    return *data ? "true" : "false";
  case id_int8:
    return to_string(int(xtoh<int8_t>(data, byteorder)));
  case id_int16:
    return to_string(xtoh<int16_t>(data, byteorder));
  case id_int32:
    return to_string(xtoh<int32_t>(data, byteorder));
  case id_int64:
    return to_string(xtoh<int64_t>(data, byteorder));
  case id_uint8:
    return to_string(unsigned(xtoh<uint8_t>(data, byteorder)));
  case id_uint16:
    return to_string(xtoh<uint16_t>(data, byteorder));
  case id_uint32:
    return to_string(xtoh<uint32_t>(data, byteorder));
  case id_uint64:
    return to_string(xtoh<uint64_t>(data, byteorder));
  case id_float16:
    return format_float32(half_to_float(xtoh<uint16_t>(data, byteorder)));
  case id_float32:
    return format_float32(xtoh<float>(data, byteorder));
  case id_float64:
    return format_float64(xtoh<double>(data, byteorder));
  case id_complex64:
    return format_complex64(
        {xtoh<float>(data, byteorder), xtoh<float>(data + 4, byteorder)});
  case id_complex128:
    return format_complex128(
        {xtoh<double>(data, byteorder), xtoh<double>(data + 8, byteorder)});
  case id_ascii: {
    size_t length = elemsize;
    while (length > 0 && data[length - 1] == 0)
      --length;
    return "\"" + string(reinterpret_cast<const char *>(data), length) + "\"";
  }
  case id_ucs4:
    return "\"" + format_ucs4(data, elemsize / 4, byteorder) + "\"";
  default:
    // int128, uint128 and complex32 have no portable printed form
    return "(skipped)";
  }
}

string format_field(const unsigned char *data, const field_t &field,
                    byteorder_t byteorder) {
  const auto &datatype = *field.datatype;
  if (!datatype.is_scalar)
    return "(skipped)"; // nested structured fields
  const size_t elemsize = datatype.type_size();
  if (field.shape.empty())
    return format_element(data, datatype.scalar_type_id, elemsize, byteorder);
  const int64_t count = field.num_elements();
  ostringstream buf;
  buf << "[";
  for (int64_t i = 0; i < count; ++i)
    buf << (i == 0 ? "" : " ")
        << format_element(data + size_t(i) * elemsize, datatype.scalar_type_id,
                          elemsize, byteorder);
  buf << "]";
  return buf.str();
}

// Values ----------------------------------------------------------------

template <typename T, typename F>
vector<string> typed_values(const ndarray &arr, const F &format) {
  vector<string> result;
  for (const auto &val : arr.get_data_vector<T>())
    result.push_back(format(val));
  return result;
}

template <typename T> string format_integer(T val) {
  return to_string(int64_t(val));
}

// Values through the library's typed accessor, where one exists
bool typed_accessor_values(const ndarray &arr, vector<string> &values) {
  const auto datatype = arr.get_datatype();
  if (!datatype->is_scalar)
    return false;
  switch (datatype->scalar_type_id) {
  case id_bool8:
    values = typed_values<bool8_t>(
        arr, [](bool8_t val) { return string(val ? "true" : "false"); });
    return true;
  case id_int8:
    values = typed_values<int8_t>(arr, format_integer<int8_t>);
    return true;
  case id_int16:
    values = typed_values<int16_t>(arr, format_integer<int16_t>);
    return true;
  case id_int32:
    values = typed_values<int32_t>(arr, format_integer<int32_t>);
    return true;
  case id_int64:
    values = typed_values<int64_t>(arr, format_integer<int64_t>);
    return true;
  case id_uint8:
    values = typed_values<uint8_t>(
        arr, [](uint8_t val) { return to_string(uint64_t(val)); });
    return true;
  case id_uint16:
    values = typed_values<uint16_t>(
        arr, [](uint16_t val) { return to_string(uint64_t(val)); });
    return true;
  case id_uint32:
    values = typed_values<uint32_t>(
        arr, [](uint32_t val) { return to_string(uint64_t(val)); });
    return true;
  case id_uint64:
    values = typed_values<uint64_t>(
        arr, [](uint64_t val) { return to_string(val); });
    return true;
  case id_float32:
    values = typed_values<float32_t>(arr, format_float32);
    return true;
  case id_float64:
    values = typed_values<float64_t>(arr, format_float64);
    return true;
  case id_complex64:
    values = typed_values<complex64_t>(arr, format_complex64);
    return true;
  case id_complex128:
    values = typed_values<complex128_t>(arr, format_complex128);
    return true;
  case id_ascii:
    values = typed_values<string>(
        arr, [](const string &val) { return "\"" + val + "\""; });
    return true;
  case id_ucs4:
    values = typed_values<u32string>(arr, [](const u32string &val) {
      return "\"" + utf8_encode(val) + "\"";
    });
    return true;
  default:
    // float16, complex32, int128 and uint128 have no accessor that every
    // build provides
    return false;
  }
}

// Values decoded from the array's bytes. `get_data_bytes()` has already
// applied `offset`, `strides` and every byte order, so the bytes it returns
// are packed in C order and in host order.
vector<string> byte_values(const ndarray &arr) {
  const auto datatype = arr.get_datatype();
  const byteorder_t byteorder = host_byteorder();
  const size_t elemsize = datatype->type_size();
  const vector<unsigned char> bytes = arr.get_data_bytes();
  const size_t npoints = elemsize == 0 ? 0 : bytes.size() / elemsize;

  vector<string> values;
  for (size_t n = 0; n < npoints; ++n) {
    const unsigned char *const element = &bytes.at(n * elemsize);
    if (datatype->is_scalar) {
      values.push_back(format_element(element, datatype->scalar_type_id,
                                      elemsize, byteorder));
      continue;
    }
    ostringstream buf;
    buf << "(";
    size_t field_offset = 0;
    for (size_t f = 0; f < datatype->fields.size(); ++f) {
      const auto &field = *datatype->fields.at(f);
      buf << (f == 0 ? "" : ", ")
          << format_field(element + field_offset, field, byteorder);
      field_offset += field.type_size();
    }
    buf << ")";
    values.push_back(buf.str());
  }
  return values;
}

void print_array(ostream &os, const string &path, const ndarray &arr) {
  const auto datatype = arr.get_datatype();
  const auto shape = arr.get_shape();

  os << path << ": " << datatype_name(*datatype) << " [";
  for (size_t d = 0; d < shape.size(); ++d)
    os << (d == 0 ? "" : " ") << shape.at(d);
  os << "]";

  vector<string> values;
  if (!typed_accessor_values(arr, values))
    values = byte_values(arr);
  for (const auto &value : values)
    os << " " << value;
  os << "\n";
}

// Walking the tree ------------------------------------------------------

void walk(ostream &os, const string &path, const shared_ptr<entry> &ent) {
  if (!ent)
    return;
  if (const auto arr = ent->get_maybe_ndarray()) {
    print_array(os, path, *arr);
    return;
  }
  if (const auto grp = ent->get_maybe_group()) {
    for (const auto &key_value : *grp)
      walk(os, path.empty() ? key_value.first : path + "/" + key_value.first,
           key_value.second);
    return;
  }
  if (const auto seq = ent->get_maybe_sequence()) {
    for (size_t i = 0; i < seq->size(); ++i)
      walk(os, path + "/" + to_string(i), seq->at(i));
    return;
  }
}

int run(int argc, char **argv) {
  ASDF_CHECK_VERSION();
  if (argc < 2) {
    cerr << "Synopsis: " << argv[0] << " <file>...\n";
    return 2;
  }
  for (int i = 1; i < argc; ++i) {
    const asdf project(argv[i]);
    walk(cout, "", project.get_group());
  }
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << argv[0] << ": error: " << e.what() << "\n";
    return 1;
  }
}
