#ifndef ASDF_DATATYPE_HPP
#define ASDF_DATATYPE_HPP

#include "asdf_byteorder.hpp"
#include "asdf_io.hpp"

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
  id_uint8,
  id_uint16,
  id_uint32,
  id_uint64,
  id_float32,
  id_float64,
  id_complex64,
  id_complex128,
  id_ascii,
  id_ucs4,
};

// Define all types
typedef bool bool8_t;
// int8_t
// int16_t
// int32_t
// int64_t
// uint8_t
// uint16_t
// uint32_t
// uint64_t
typedef float float32_t;
typedef double float64_t;
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
    : integral_constant<
          scalar_type_id_t,
          is_same<T, bool8_t>::value
              ? id_bool8
              : is_integral<T>::value && is_signed<T>::value
                    ? (sizeof(T) == 1
                           ? id_int8
                           : sizeof(T) == 2
                                 ? id_int16
                                 : sizeof(T) == 4
                                       ? id_int32
                                       : sizeof(T) == 8 ? id_int64 : id_error)
                    : is_integral<T>::value && is_unsigned<T>::value
                          ? (sizeof(T) == 1
                                 ? id_uint8
                                 : sizeof(T) == 2
                                       ? id_uint16
                                       : sizeof(T) == 4
                                             ? id_uint32
                                             : sizeof(T) == 8 ? id_uint64
                                                              : id_error)
                          : is_floating_point<T>::value
                                ? (sizeof(T) == 4
                                       ? id_float32
                                       : sizeof(T) == 8 ? id_float64 : id_error)
                                : is_complex<T>::value
                                      ? (sizeof(T) == 8
                                             ? id_complex64
                                             : sizeof(T) == 16 ? id_complex128
                                                               : id_error)
                                      : is_same<T, ascii_t>::value
                                            ? id_ascii
                                            : is_same<T, ucs4_t>::value
                                                  ? id_ucs4
                                                  : id_error> {};

// Convert an enum id to its type
template <size_t> struct get_scalar_type;
template <> struct get_scalar_type<id_bool8> { typedef bool8_t type; };
template <> struct get_scalar_type<id_int8> { typedef int8_t type; };
template <> struct get_scalar_type<id_int16> { typedef int16_t type; };
template <> struct get_scalar_type<id_int32> { typedef int32_t type; };
template <> struct get_scalar_type<id_int64> { typedef int64_t type; };
template <> struct get_scalar_type<id_uint8> { typedef uint8_t type; };
template <> struct get_scalar_type<id_uint16> { typedef uint16_t type; };
template <> struct get_scalar_type<id_uint32> { typedef uint32_t type; };
template <> struct get_scalar_type<id_uint64> { typedef uint64_t type; };
template <> struct get_scalar_type<id_float32> { typedef float32_t type; };
template <> struct get_scalar_type<id_float64> { typedef float64_t type; };
template <> struct get_scalar_type<id_complex64> { typedef complex64_t type; };
template <> struct get_scalar_type<id_complex128> {
  typedef complex128_t type;
};
template <> struct get_scalar_type<id_ascii> { typedef ascii_t type; };
template <> struct get_scalar_type<id_ucs4> { typedef ucs4_t type; };
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
void yaml_decode(const YAML::Node &node, uint8_t &val);
void yaml_decode(const YAML::Node &node, uint16_t &val);
void yaml_decode(const YAML::Node &node, uint32_t &val);
void yaml_decode(const YAML::Node &node, uint64_t &val);
void yaml_decode(const YAML::Node &node, float32_t &val);
void yaml_decode(const YAML::Node &node, float64_t &val);
template <typename T> void yaml_decode(const YAML::Node &node, complex<T> &val);

YAML::Node yaml_encode(bool8_t val);
YAML::Node yaml_encode(int8_t val);
YAML::Node yaml_encode(int16_t val);
YAML::Node yaml_encode(int32_t val);
YAML::Node yaml_encode(int64_t val);
YAML::Node yaml_encode(uint8_t val);
YAML::Node yaml_encode(uint16_t val);
YAML::Node yaml_encode(uint32_t val);
YAML::Node yaml_encode(uint64_t val);
YAML::Node yaml_encode(float32_t val);
YAML::Node yaml_encode(float64_t val);
template <typename T> YAML::Node yaml_encode(const complex<T> &val);

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

#define ASDF_DATATYPE_HPP_DONE
#endif // #ifndef ASDF_DATATYPE_HPP
#ifndef ASDF_DATATYPE_HPP_DONE
#error "Cyclic include depencency"
#endif
