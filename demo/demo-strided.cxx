// asdf-demo-strided: write and read back arrays that are not simply a
// densely packed block of host-order data.
//
// Every array here is built with the general `ndarray` constructor, so the
// demo controls `byteorder`, `offset` and `strides` directly: a strided view
// into a larger block, a view with negative strides, a Fortran-order array,
// a big-endian array whose bytes were swapped with `htox`, a `bool8` block,
// and a big-endian structured record array. The file is written, read back,
// and every array's `get_data_vector<T>()` / `get_data_bytes()` is compared
// against the values the demo put in.

#include <asdf/asdf.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace std;
using namespace ASDF;

namespace {

// Wrap raw bytes in a block
memoized<block_t> make_block(vector<unsigned char> bytes) {
  return make_constant_memoized(shared_ptr<block_t>(
      make_shared<typed_block_t<unsigned char>>(std::move(bytes))));
}

// The bytes of `values` in `byteorder`
template <typename T>
vector<unsigned char> encode(const vector<T> &values, byteorder_t byteorder) {
  vector<unsigned char> bytes;
  bytes.reserve(values.size() * sizeof(T));
  for (const auto &value : values) {
    const auto encoded = htox(value, byteorder);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
  }
  return bytes;
}

byteorder_t other_byteorder() {
  return host_byteorder() == byteorder_t::little ? byteorder_t::big
                                                 : byteorder_t::little;
}

// NaN is not equal to itself, but an element written as NaN has to read back
// as NaN
template <typename T> bool same_value(T read, T expected) {
  return read == expected;
}
bool same_value(float64_t read, float64_t expected) {
  return read == expected || (std::isnan(read) && std::isnan(expected));
}
bool same_value(complex128_t read, complex128_t expected) {
  return same_value(read.real(), expected.real()) &&
         same_value(read.imag(), expected.imag());
}

template <typename T>
void check_values(const string &name, const ndarray &arr,
                  const vector<T> &expected) {
  const vector<T> values = arr.get_data_vector<T>();
  ASDF_CHECK(values.size() == expected.size(),
             name + ": read back " + std::to_string(values.size()) +
                 " elements, expected " + std::to_string(expected.size()));
  for (size_t i = 0; i < values.size(); ++i) {
    // `vector<bool>` hands out a proxy, so take copies of both elements
    const T value = values.at(i), want = expected.at(i);
    ASDF_CHECK(same_value(value, want),
               name + ": element " + std::to_string(i) + " differs");
  }
  cout << "  " << name << ": " << values.size() << " elements ok\n";
}

void check_bytes(const string &name, const ndarray &arr,
                 const vector<unsigned char> &expected) {
  const vector<unsigned char> bytes = arr.get_data_bytes();
  ASDF_CHECK(bytes.size() == expected.size(),
             name + ": read back " + std::to_string(bytes.size()) +
                 " bytes, expected " + std::to_string(expected.size()));
  ASDF_CHECK(bytes == expected, name + ": bytes differ");
  cout << "  " << name << ": " << bytes.size() << " bytes ok\n";
}

// `emit_scalar` and `parse_scalar` are each other's inverse, in every byte
// order
template <typename T>
void check_scalar_roundtrip(const string &name, const T &value,
                            byteorder_t byteorder) {
  const auto bytes = htox(value, byteorder);
  const YAML::Node node =
      emit_scalar(bytes.data(), get_scalar_type_id<T>(), byteorder);
  vector<unsigned char> parsed(sizeof(T));
  parse_scalar(node, parsed.data(), get_scalar_type_id<T>(), byteorder);
  ASDF_CHECK(std::memcmp(parsed.data(), bytes.data(), sizeof(T)) == 0,
             name + ": emit_scalar/parse_scalar do not round-trip in a "
                    "foreign byte order");
  cout << "  " << name << ": scalar round trip ok\n";
}

// The tag every `core/complex-1.0.0` node carries, as the full URI a
// `YAML::Node` stores. Taken from the version table, never spelled here.
string complex_tag() {
  return string(asdf_tag_prefix) +
         standard_info(latest_standard_version()).complex_tag;
}

// The `core/complex-1.0.0` grammar spells the non-finite components `inf`,
// `-inf` and `nan`, without the leading dot that YAML -- and hence every file
// asdf-cxx wrote before this -- uses. `emit_scalar` has to produce the first
// spelling, and `parse_scalar` has to accept both.
void check_complex_spelling() {
  const auto emit = [](complex128_t value) {
    const auto bytes = htox(value, host_byteorder());
    return emit_scalar(bytes.data(), id_complex128, host_byteorder()).Scalar();
  };
  const auto parse = [](const string &text) {
    YAML::Node node;
    node.SetTag(complex_tag());
    node = text;
    complex128_t value;
    parse_scalar(node, reinterpret_cast<unsigned char *>(&value), id_complex128,
                 host_byteorder());
    return value;
  };

  const float64_t nan_value = std::numeric_limits<float64_t>::quiet_NaN();
  const float64_t inf_value = std::numeric_limits<float64_t>::infinity();
  const vector<pair<complex128_t, string>> spellings{
      {{1.0, 0.0}, "1+0i"},
      {{-2.5, 0.5}, "-2.5+0.5i"},
      {{1.5, -inf_value}, "1.5-infi"},
      {{nan_value, inf_value}, "nan+infi"},
      {{-inf_value, nan_value}, "-inf+nani"},
      {{0.0, -0.0}, "0-0i"}};
  for (const auto &[value, text] : spellings) {
    const string emitted = emit(value);
    ASDF_CHECK(emitted == text, "complex spelling: emitted \"" + emitted +
                                    "\", expected \"" + text + "\"");
    ASDF_CHECK(same_value(parse(text), value),
               "complex spelling: \"" + text + "\" did not read back");
  }

  // yaml-cpp's spelling, which older asdf-cxx versions wrote
  ASDF_CHECK(
      same_value(parse(".nan+.nani"), complex128_t(nan_value, nan_value)),
      "complex spelling: \".nan+.nani\" did not read back");
  ASDF_CHECK(same_value(parse("-.inf+1.5i"), complex128_t(-inf_value, 1.5)),
             "complex spelling: \"-.inf+1.5i\" did not read back");
  ASDF_CHECK(
      same_value(parse(".inf-.infi"), complex128_t(inf_value, -inf_value)),
      "complex spelling: \".inf-.infi\" did not read back");
  cout << "  complex spelling ok\n";
}

// A plain float keeps a decimal point in its mantissa, so that a reader does
// not turn it into an integer or a string
void check_float_spelling() {
  const auto emit = [](float64_t value) {
    const auto bytes = htox(value, host_byteorder());
    return emit_scalar(bytes.data(), id_float64, host_byteorder()).Scalar();
  };

  // A mantissa of digits only is not a float to a YAML 1.1 reader, with or
  // without an exponent: `1` is an integer and `1e+17` is a string. These
  // values have the same shortest decimal spelling at every precision a
  // yaml-cpp build might print with.
  const vector<pair<float64_t, string>> spellings{
      {1.0, "1.0"},        {-2.0, "-2.0"},     {0.0, "0.0"},
      {-0.0, "-0.0"},      {1.5, "1.5"},       {1e17, "1.0e+17"},
      {-1e17, "-1.0e+17"}, {3e-10, "3.0e-10"}, {1e308, "1.0e+308"},
      {1e5, "100000.0"},   {1e-4, "0.0001"}};
  for (const auto &[value, text] : spellings) {
    const string emitted = emit(value);
    ASDF_CHECK(emitted == text, "float spelling: emitted \"" + emitted +
                                    "\", expected \"" + text + "\"");
  }

  // How many digits yaml-cpp prints is its own business and differs between
  // its versions, so these values are checked for the property this library
  // owes instead of for an exact text
  for (const float64_t value :
       {1e-5, 1.5e-30, 1e-308, 0.1, std::numeric_limits<float64_t>::max(),
        std::numeric_limits<float64_t>::min()}) {
    const string text = emit(value);
    const size_t exponent = text.find_first_of("eE");
    ASDF_CHECK(text.substr(0, exponent).find('.') != string::npos,
               "float spelling: \"" + text +
                   "\" has no decimal point in its mantissa, so YAML 1.1 "
                   "does not resolve it as a float");
  }
  cout << "  float spelling ok\n";
}

shared_ptr<ndarray> get_array(const shared_ptr<group> &grp,
                              const string &name) {
  const auto ent = grp->at(name);
  ASDF_CHECK(ent, "Entry \"" + name + "\" is missing");
  const auto arr = ent->get_maybe_ndarray();
  ASDF_CHECK(arr, "Entry \"" + name + "\" is not an array");
  return arr;
}

// The structured datatype used below: `{int32 i; float64 x;}`, big-endian
shared_ptr<datatype_t> make_record_datatype() {
  return make_shared<datatype_t>(vector<shared_ptr<field_t>>{
      make_shared<field_t>("i", make_shared<datatype_t>(id_int32), false,
                           byteorder_t::undefined, vector<int64_t>()),
      make_shared<field_t>("x", make_shared<datatype_t>(id_float64), false,
                           byteorder_t::undefined, vector<int64_t>())});
}

int run(int argc, char **argv) {
  cout << "asdf-demo-strided: Write and read back strided arrays\n";
  ASDF_CHECK_VERSION();

  const byteorder_t foreign = other_byteorder();

  // A 4x6 int32 array in host byte order, the basis for two views
  vector<int32_t> base_values(24);
  for (size_t i = 0; i < base_values.size(); ++i)
    base_values.at(i) = int32_t(i);
  const vector<unsigned char> base_bytes =
      encode(base_values, host_byteorder());

  // The even columns of the last two rows: offset past the first two rows,
  // row stride 24 bytes, column stride 8 bytes
  const vector<int32_t> offset_view_expected{12, 14, 16, 18, 20, 22};
  auto offset_view = make_shared<ndarray>(
      make_block(base_bytes), optional<block_info_t>(), block_format_t::block,
      compression_t::none, 0, vector<bool>(), make_shared<datatype_t>(id_int32),
      host_byteorder(), vector<int64_t>{2, 3}, /*offset*/ 48,
      vector<int64_t>{24, 8});

  // The whole array with its rows reversed: the offset points at the last
  // row and the row stride is negative
  vector<int32_t> reversed_expected;
  for (int row = 3; row >= 0; --row)
    for (int col = 0; col < 6; ++col)
      reversed_expected.push_back(int32_t(6 * row + col));
  auto reversed_view = make_shared<ndarray>(
      make_block(base_bytes), optional<block_info_t>(), block_format_t::block,
      compression_t::none, 0, vector<bool>(), make_shared<datatype_t>(id_int32),
      host_byteorder(), vector<int64_t>{4, 6}, /*offset*/ 72,
      vector<int64_t>{-24, 4});

  // A 4x4 float64 array stored in Fortran order: the column stride is the
  // element size and the row stride is one column
  vector<float64_t> fortran_values(16);
  for (size_t i = 0; i < fortran_values.size(); ++i)
    fortran_values.at(i) = 0.5 * double(i); // column-major in the block
  vector<float64_t> fortran_expected(16);
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      fortran_expected.at(4 * row + col) = 0.5 * double(4 * col + row);
  auto fortran = make_shared<ndarray>(
      make_block(encode(fortran_values, host_byteorder())),
      optional<block_info_t>(), block_format_t::block, compression_t::none, 0,
      vector<bool>(), make_shared<datatype_t>(id_float64), host_byteorder(),
      vector<int64_t>{4, 4}, /*offset*/ 0, vector<int64_t>{8, 32});

  // A big-endian int32 array on a little-endian host, and vice versa
  const vector<int32_t> foreign_expected{-1, 0, 1, 1000000, -1000000};
  auto foreign_ints = make_shared<ndarray>(
      make_block(encode(foreign_expected, foreign)), optional<block_info_t>(),
      block_format_t::block, compression_t::none, 0, vector<bool>(),
      make_shared<datatype_t>(id_int32), foreign,
      vector<int64_t>{int64_t(foreign_expected.size())});

  // A bool8 block: one byte per element, and any nonzero byte is true
  const vector<unsigned char> flag_bytes{0, 1, 0, 2, 0};
  const vector<bool> flags_expected{false, true, false, true, false};
  auto flags = make_shared<ndarray>(
      make_block(flag_bytes), optional<block_info_t>(), block_format_t::block,
      compression_t::none, 0, vector<bool>(), make_shared<datatype_t>(id_bool8),
      host_byteorder(), vector<int64_t>{int64_t(flag_bytes.size())});

  // A big-endian structured array. `get_data_bytes()` must return the same
  // records in host byte order.
  const auto record_datatype = make_record_datatype();
  vector<unsigned char> record_bytes, record_expected;
  for (int n = 0; n < 3; ++n) {
    const auto i = htox(int32_t(n + 1), foreign);
    const auto x = htox(float64_t(1.25 * (n + 1)), foreign);
    record_bytes.insert(record_bytes.end(), i.begin(), i.end());
    record_bytes.insert(record_bytes.end(), x.begin(), x.end());
    const auto hi = htox(int32_t(n + 1), host_byteorder());
    const auto hx = htox(float64_t(1.25 * (n + 1)), host_byteorder());
    record_expected.insert(record_expected.end(), hi.begin(), hi.end());
    record_expected.insert(record_expected.end(), hx.begin(), hx.end());
  }
  auto records = make_shared<ndarray>(
      make_block(record_bytes), optional<block_info_t>(), block_format_t::block,
      compression_t::none, 0, vector<bool>(), record_datatype, foreign,
      vector<int64_t>{3});

  // A big-endian complex128 array. Real and imaginary part are swapped
  // independently; reversing all sixteen bytes of an element would exchange
  // them, so the values below are deliberately asymmetric. The last three
  // elements are non-finite: the `core/complex-1.0.0` grammar spells those
  // `nan`, `inf` and `-inf`, and the inline form below has to write and read
  // back exactly that.
  const float64_t nan_value = std::numeric_limits<float64_t>::quiet_NaN();
  const float64_t inf_value = std::numeric_limits<float64_t>::infinity();
  const vector<complex128_t> complex_expected{
      {1.5, -2.25},           {-3.75, 4.5},      {0.0, 6.125},
      {nan_value, inf_value}, {-inf_value, 1.0}, {nan_value, nan_value}};
  auto complex_block = make_shared<ndarray>(
      make_block(encode(complex_expected, foreign)), optional<block_info_t>(),
      block_format_t::block, compression_t::none, 0, vector<bool>(),
      make_shared<datatype_t>(id_complex128), foreign,
      vector<int64_t>{int64_t(complex_expected.size())});

  // The same array, but written as inline YAML. Emitting it goes through
  // `emit_scalar`, which has to byte-swap each component on its own.
  auto complex_inline = make_shared<ndarray>(
      make_block(encode(complex_expected, foreign)), optional<block_info_t>(),
      block_format_t::inline_array, compression_t::none, 0, vector<bool>(),
      make_shared<datatype_t>(id_complex128), foreign,
      vector<int64_t>{int64_t(complex_expected.size())});

  auto grp = make_shared<group>();
  grp->emplace("offset_view", offset_view);
  grp->emplace("reversed_view", reversed_view);
  grp->emplace("fortran", fortran);
  grp->emplace("foreign_ints", foreign_ints);
  grp->emplace("flags", flags);
  grp->emplace("records", records);
  grp->emplace("complex_block", complex_block);
  grp->emplace("complex_inline", complex_inline);

  cout << "Checking the arrays before writing them:\n";
  check_values("offset_view", *offset_view, offset_view_expected);
  check_values("reversed_view", *reversed_view, reversed_expected);
  check_values("fortran", *fortran, fortran_expected);
  check_values("foreign_ints", *foreign_ints, foreign_expected);
  check_values("flags", *flags, flags_expected);
  check_bytes("records", *records, record_expected);
  check_values("complex_block", *complex_block, complex_expected);
  check_values("complex_inline", *complex_inline, complex_expected);

  const auto project = make_shared<asdf>(map<string, string>(), grp);
  project->write("strided.asdf");

  cout << "Reading strided.asdf back:\n";
  const asdf copy("strided.asdf");
  const auto copied = copy.get_group();
  check_values("offset_view", *get_array(copied, "offset_view"),
               offset_view_expected);
  check_values("reversed_view", *get_array(copied, "reversed_view"),
               reversed_expected);
  check_values("fortran", *get_array(copied, "fortran"), fortran_expected);
  check_values("foreign_ints", *get_array(copied, "foreign_ints"),
               foreign_expected);
  check_values("flags", *get_array(copied, "flags"), flags_expected);
  check_bytes("records", *get_array(copied, "records"), record_expected);
  check_values("complex_block", *get_array(copied, "complex_block"),
               complex_expected);
  check_values("complex_inline", *get_array(copied, "complex_inline"),
               complex_expected);

  // `emit_scalar` and `parse_scalar` must agree with each other in a foreign
  // byte order, for complex numbers too
  check_scalar_roundtrip<complex64_t>("complex64", {1.5f, -2.25f}, foreign);
  check_scalar_roundtrip<complex128_t>("complex128", {1.5, -2.25}, foreign);
  check_scalar_roundtrip<float64_t>("float64", 1.5, foreign);
  check_scalar_roundtrip<int32_t>("int32", -123456, foreign);

  check_complex_spelling();
  check_float_spelling();

  // Only the densely packed arrays are C-contiguous
  const auto check_contiguous = [&](const string &name, bool expected) {
    const auto arr = get_array(copied, name);
    ASDF_CHECK(arr->is_c_contiguous() == expected,
               name + ": is_c_contiguous() should be " +
                   (expected ? "true" : "false"));
  };
  check_contiguous("foreign_ints", true);
  check_contiguous("flags", true);
  check_contiguous("records", true);
  check_contiguous("offset_view", false);   // column stride is two elements
  check_contiguous("reversed_view", false); // negative row stride
  check_contiguous("fortran", false);       // column-major
  cout << "  is_c_contiguous ok\n";

  // The bounds check must reject an array that reaches past its block
  bool rejected = false;
  try {
    ndarray(make_block(base_bytes), optional<block_info_t>(),
            block_format_t::block, compression_t::none, 0, vector<bool>(),
            make_shared<datatype_t>(id_int32), host_byteorder(),
            vector<int64_t>{4, 7});
  } catch (const ASDF::error &) {
    rejected = true;
  }
  ASDF_CHECK(rejected, "An array reaching past its block was not rejected");
  cout << "  out-of-bounds array rejected\n";

  cout << "Done.\n";
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
