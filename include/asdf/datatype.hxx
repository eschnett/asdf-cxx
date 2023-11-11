#ifndef ASDF_DATATYPE_HXX
#define ASDF_DATATYPE_HXX

#include "asdf/byteorder.hxx"
#include "asdf/io.hxx"

#include <yaml-cpp/yaml.h>

#include <complex>
#include <type_traits>
#include <vector>

namespace ASDF {
using namespace std;

// Scalar types

// Define an id for every type
enum scalar_type_id_t {
  id_error = -1,
  id_bool8,
  id_int8,
  id_int16,
  id_int32,
  id_int64,
  id_int128,
  id_uint8,
  id_uint16,
  id_uint32,
  id_uint64,
  id_uint128,
  id_float16,
  id_float32,
  id_float64,
  id_complex32,
  id_complex64,
  id_complex128,
  id_ascii,
  id_ucs4,
};

bool have_datatype_int128();
bool have_datatype_float16();

// Define all types
typedef bool bool8_t;
// int8_t
// int16_t
// int32_t
// int64_t
#ifdef ASDF_HAVE_INT128
typedef __int128 int128_t;
#endif
// uint8_t
// uint16_t
// uint32_t
// uint64_t
#ifdef ASDF_HAVE_INT128
typedef unsigned __int128 uint128_t;
#endif
#ifdef ASDF_HAVE_FLOAT16
typedef _Float16 float16_t;
#endif
typedef float float32_t;
typedef double float64_t;
#ifdef ASDF_HAVE_FLOAT16
typedef complex<float16_t> complex32_t;
#endif
typedef complex<float32_t> complex64_t;
typedef complex<float64_t> complex128_t;
typedef vector<unsigned char> ascii_t;
typedef vector<char32_t> ucs4_t;

// Convert a type to its id enum
namespace {
template <typename T> struct is_complex : false_type {};
template <typename T> struct is_complex<complex<T>> : is_floating_point<T> {};
// template <typename T> inline constexpr bool is_complex_v =
// is_complex<T>::value;
} // namespace

template <typename T>
struct get_scalar_type_id
    : integral_constant<scalar_type_id_t,
                        is_same<T, bool8_t>::value ? id_bool8
                        : is_integral<T>::value && is_signed<T>::value
                            ? (sizeof(T) == 1   ? id_int8
                               : sizeof(T) == 2 ? id_int16
                               : sizeof(T) == 4 ? id_int32
                               : sizeof(T) == 8 ? id_int64
#ifdef ASDF_HAVE_INT128
                               : sizeof(T) == 16 ? id_int128
#endif
                                                 : id_error)
                        : is_integral<T>::value && is_unsigned<T>::value
                            ? (sizeof(T) == 1   ? id_uint8
                               : sizeof(T) == 2 ? id_uint16
                               : sizeof(T) == 4 ? id_uint32
                               : sizeof(T) == 8 ? id_uint64
#ifdef ASDF_HAVE_INT128

#endif
                               : sizeof(T) == 16 ? id_uint128
                                                 : id_error)
#ifdef ASDF_HAVE_FLOAT16
                            : // float16_t is not officially a floating-point
                              // type
                            is_same<T, float16_t>::value ? id_float16
#endif
                        : is_floating_point<T>::value
                            ? (is_same<T, float32_t>::value   ? id_float32
                               : is_same<T, float64_t>::value ? id_float64
                                                              : id_error)
#ifdef ASDF_HAVE_FLOAT16
                            : // float16_t is not officially a floating-point
                              // type
                            is_same<T, complex32_t>::value ? id_complex32
#endif
                        : is_complex<T>::value
                            ? (is_same<T, complex64_t>::value    ? id_complex64
                               : is_same<T, complex128_t>::value ? id_complex128
                                                                 : id_error)
                        : is_same<T, ascii_t>::value ? id_ascii
                        : is_same<T, ucs4_t>::value  ? id_ucs4
                                                     : id_error> {
};

// Convert an enum id to its type
template <size_t> struct get_scalar_type;
template <> struct get_scalar_type<id_bool8> {
  typedef bool8_t type;
};
template <> struct get_scalar_type<id_int8> {
  typedef int8_t type;
};
template <> struct get_scalar_type<id_int16> {
  typedef int16_t type;
};
template <> struct get_scalar_type<id_int32> {
  typedef int32_t type;
};
template <> struct get_scalar_type<id_int64> {
  typedef int64_t type;
};
#ifdef ASDF_HAVE_INT128
template <> struct get_scalar_type<id_int128> {
  typedef int128_t type;
};
#endif
template <> struct get_scalar_type<id_uint8> {
  typedef uint8_t type;
};
template <> struct get_scalar_type<id_uint16> {
  typedef uint16_t type;
};
template <> struct get_scalar_type<id_uint32> {
  typedef uint32_t type;
};
template <> struct get_scalar_type<id_uint64> {
  typedef uint64_t type;
};
#ifdef ASDF_HAVE_INT128
template <> struct get_scalar_type<id_uint128> {
  typedef uint128_t type;
};
#endif
#ifdef ASDF_HAVE_FLOAT16
template <> struct get_scalar_type<id_float16> {
  typedef float16_t type;
};
#endif
template <> struct get_scalar_type<id_float32> {
  typedef float32_t type;
};
template <> struct get_scalar_type<id_float64> {
  typedef float64_t type;
};
#ifdef ASDF_HAVE_FLOAT16
template <> struct get_scalar_type<id_complex32> {
  typedef complex32_t type;
};
#endif
template <> struct get_scalar_type<id_complex64> {
  typedef complex64_t type;
};
template <> struct get_scalar_type<id_complex128> {
  typedef complex128_t type;
};
template <> struct get_scalar_type<id_ascii> {
  typedef ascii_t type;
};
template <> struct get_scalar_type<id_ucs4> {
  typedef ucs4_t type;
};
template <size_t I> using get_scalar_type_t = typename get_scalar_type<I>::type;

// Convert an enum id to its type size
size_t get_scalar_type_size(scalar_type_id_t scalar_type_id);

void yaml_decode(const YAML::Node &node, scalar_type_id_t &scalar_type_id);
YAML::Node yaml_encode(scalar_type_id_t scalar_type_id);

void yaml_decode(const YAML::Node &node, bool8_t &val);
void yaml_decode(const YAML::Node &node, int8_t &val);
void yaml_decode(const YAML::Node &node, int16_t &val);
void yaml_decode(const YAML::Node &node, int32_t &val);
void yaml_decode(const YAML::Node &node, int64_t &val);
#ifdef ASDF_HAVE_INT128
void yaml_decode(const YAML::Node &node, int128_t &val);
#endif
void yaml_decode(const YAML::Node &node, uint8_t &val);
void yaml_decode(const YAML::Node &node, uint16_t &val);
void yaml_decode(const YAML::Node &node, uint32_t &val);
void yaml_decode(const YAML::Node &node, uint64_t &val);
#ifdef ASDF_HAVE_INT128
void yaml_decode(const YAML::Node &node, uint128_t &val);
#endif
#ifdef ASDF_HAVE_FLOAT16
void yaml_decode(const YAML::Node &node, float16_t &val);
#endif
void yaml_decode(const YAML::Node &node, float32_t &val);
void yaml_decode(const YAML::Node &node, float64_t &val);
#ifdef ASDF_HAVE_FLOAT16
void yaml_decode(const YAML::Node &node, complex32_t &val);
#endif
void yaml_decode(const YAML::Node &node, complex64_t &val);
void yaml_decode(const YAML::Node &node, complex128_t &val);

YAML::Node yaml_encode(bool8_t val);
YAML::Node yaml_encode(int8_t val);
YAML::Node yaml_encode(int16_t val);
YAML::Node yaml_encode(int32_t val);
YAML::Node yaml_encode(int64_t val);
#ifdef ASDF_HAVE_INT128
YAML::Node yaml_encode(int128_t val);
#endif
YAML::Node yaml_encode(uint8_t val);
YAML::Node yaml_encode(uint16_t val);
YAML::Node yaml_encode(uint32_t val);
YAML::Node yaml_encode(uint64_t val);
#ifdef ASDF_HAVE_INT128
YAML::Node yaml_encode(uint128_t val);
#endif
#ifdef ASDF_HAVE_FLOAT16
YAML::Node yaml_encode(float16_t val);
#endif
YAML::Node yaml_encode(float32_t val);
YAML::Node yaml_encode(float64_t val);
#ifdef ASDF_HAVE_FLOAT16
YAML::Node yaml_encode(complex32_t val);
#endif
YAML::Node yaml_encode(complex64_t val);
YAML::Node yaml_encode(complex128_t val);

void parse_scalar(const YAML::Node &node, unsigned char *data,
                  scalar_type_id_t scalar_type_id,
                  byteorder_t byteorder = host_byteorder());
YAML::Node emit_scalar(const unsigned char *data,
                       scalar_type_id_t scalar_type_id,
                       byteorder_t byteorder = host_byteorder());

////////////////////////////////////////////////////////////////////////////////

// Datatypes

class datatype_t;

class field_t {
public:
  string name;
  shared_ptr<datatype_t> datatype;
  bool have_byteorder;
  byteorder_t byteorder;
  vector<int64_t> shape;

public:
  field_t() = delete;
  field_t(const field_t &) = default;
  field_t(field_t &&) = default;
  field_t &operator=(const field_t &) = default;
  field_t &operator=(field_t &&) = default;

  field_t(string name, shared_ptr<datatype_t> datatype, bool have_byteorder,
          byteorder_t byteorder, vector<int64_t> shape);

  field_t(const shared_ptr<reader_state> &rs, const YAML::Node &node);
  field_t(const copy_state &cs, const field_t &field);
  YAML::Node to_yaml() const;
  YAML::Node to_yaml(writer &w) const { return to_yaml(); }
};

inline YAML::Node yaml_encode(const field_t &field) { return field.to_yaml(); }

class datatype_t {
public:
  bool is_scalar;
  scalar_type_id_t scalar_type_id;
  vector<shared_ptr<field_t>> fields;

public:
  datatype_t() = delete;
  datatype_t(const datatype_t &) = default;
  datatype_t(datatype_t &&) = default;
  datatype_t &operator=(const datatype_t &) = default;
  datatype_t &operator=(datatype_t &&) = default;

  datatype_t(scalar_type_id_t scalar_type_id);
  datatype_t(vector<shared_ptr<field_t>> fields);

  datatype_t(const shared_ptr<reader_state> &rs, const YAML::Node &node);
  datatype_t(const copy_state &cs, const datatype_t &datatype);
  YAML::Node to_yaml() const;
  YAML::Node to_yaml(writer &w) const { return to_yaml(); }

  size_t type_size() const;
};

inline YAML::Node yaml_encode(const datatype_t &datatype) {
  return datatype.to_yaml();
}
inline ostream &operator<<(ostream &os, const datatype_t &datatype) {
  return os << yaml_encode(datatype);
}

void parse_scalar(const YAML::Node &node, unsigned char *data,
                  const shared_ptr<datatype_t> &datatype,
                  byteorder_t byteorder = host_byteorder());
YAML::Node emit_scalar(const unsigned char *data,
                       const shared_ptr<datatype_t> &datatype,
                       byteorder_t byteorder = host_byteorder());

} // namespace ASDF

#define ASDF_DATATYPE_HXX_DONE
#endif // #ifndef ASDF_DATATYPE_HXX
#ifndef ASDF_DATATYPE_HXX_DONE
#error "Cyclic include depencency"
#endif
