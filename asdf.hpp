#ifndef ASDF_HPP
#define ASDF_HPP

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ASDF {
using namespace std;

////////////////////////////////////////////////////////////////////////////////

// Scalar types

// Define an id for every type
enum scalar_type_id_t {
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
template <typename> struct get_scalar_type_id;
template <> struct get_scalar_type_id<bool8_t> {
  constexpr static scalar_type_id_t value = id_bool8;
};
template <> struct get_scalar_type_id<int8_t> {
  constexpr static scalar_type_id_t value = id_int8;
};
template <> struct get_scalar_type_id<int16_t> {
  constexpr static scalar_type_id_t value = id_int16;
};
template <> struct get_scalar_type_id<int32_t> {
  constexpr static scalar_type_id_t value = id_int32;
};
template <> struct get_scalar_type_id<int64_t> {
  constexpr static scalar_type_id_t value = id_int64;
};
template <> struct get_scalar_type_id<uint8_t> {
  constexpr static scalar_type_id_t value = id_uint8;
};
template <> struct get_scalar_type_id<uint16_t> {
  constexpr static scalar_type_id_t value = id_uint16;
};
template <> struct get_scalar_type_id<uint32_t> {
  constexpr static scalar_type_id_t value = id_uint32;
};
template <> struct get_scalar_type_id<uint64_t> {
  constexpr static scalar_type_id_t value = id_uint64;
};
template <> struct get_scalar_type_id<float32_t> {
  constexpr static scalar_type_id_t value = id_float32;
};
template <> struct get_scalar_type_id<float64_t> {
  constexpr static scalar_type_id_t value = id_float64;
};
template <> struct get_scalar_type_id<complex64_t> {
  constexpr static scalar_type_id_t value = id_complex64;
};
template <> struct get_scalar_type_id<complex128_t> {
  constexpr static scalar_type_id_t value = id_complex128;
};
template <> struct get_scalar_type_id<ascii_t> {
  constexpr static scalar_type_id_t value = id_ascii;
};
template <> struct get_scalar_type_id<ucs4_t> {
  constexpr static scalar_type_id_t value = id_ucs4;
};

// A variant that can hold a valur of each type
typedef variant<bool8_t, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t,
                uint32_t, uint64_t, float32_t, float64_t, complex64_t,
                complex128_t, ascii_t, ucs4_t>
    scalar_type_t;

// Convert an enum id to its type
template <size_t I> struct get_scalar_type {
  typedef variant_alternative_t<I, scalar_type_t> type;
};
template <size_t I> using get_scalar_type_t = typename get_scalar_type<I>::type;

////////////////////////////////////////////////////////////////////////////////

// I/O

class ndarray;

class writer_state {
  vector<shared_ptr<ndarray>> ndarrays;

public:
  writer_state(const writer_state &) = delete;
  writer_state(writer_state &&) = delete;
  writer_state &operator=(const writer_state &) = delete;
  writer_state &operator=(writer_state &&) = delete;

  writer_state() = default;
  ~writer_state() { assert(ndarrays.empty()); }
  int64_t add_block(const shared_ptr<ndarray> &ndarray) {
    ndarrays.push_back(ndarray);
    return ndarrays.size() - 1;
  }
  void flush(ostream &os);
};

class writer {
public:
  virtual YAML::Node to_yaml(writer_state &ws) const = 0;
};

////////////////////////////////////////////////////////////////////////////////

// Multi-dimensional array

class ndarray {
  optional<string> name;
  vector<unsigned char> data;
  optional<vector<bool>> mask;
  scalar_type_id_t scalar_type_id;
  vector<int64_t> shape;
  vector<int64_t> stride;
  int64_t offset;

public:
  ndarray() = delete;
  ndarray(const ndarray &) = default;
  ndarray(ndarray &&) = default;
  ndarray &operator=(const ndarray &) = default;
  ndarray &operator=(ndarray &&) = default;

  template <typename T>
  ndarray(const optional<string> &name, const vector<T> &data,
          const optional<vector<bool>> &mask, const vector<int64_t> &shape,
          const vector<int64_t> &stride = vector<int64_t>(),
          int64_t offset = 0) {
    // name
    this->name = name;
    // type
    this->scalar_type_id = get_scalar_type_id<T>::value;
    // shape
    int rank = shape.size();
    for (int d = 0; d < rank; ++d)
      assert(shape[d] >= 0);
    this->shape = shape;
    // data
    int64_t npoints = 1;
    for (int d = 0; d < rank; ++d)
      npoints *= shape[d];
    assert(data.size() == npoints);
    this->data.resize(npoints * sizeof(T));
    memcpy(this->data.data(), data.data(), npoints * sizeof(T));
    // mask
    if (mask)
      assert(mask->size() == npoints);
    this->mask = mask;
    // stride
    if (stride.empty()) {
      // Default: contiguous and in C order
      this->stride.resize(rank);
      int64_t str = 1;
      for (int d = rank - 1; d >= 0; --d) {
        this->stride[d] = str;
        str *= shape[d];
      }
    }
    assert(stride.size() == rank);
    for (int d = 0; d < rank; ++d)
      assert(stride[d] >= 1 || stride[d] <= -1);
    // offset
    assert(offset >= 0);
    this->offset = offset;
  }

  const unsigned char *data_ptr() const { return data.data(); }
  const size_t data_size() const { return data.size(); }
};

////////////////////////////////////////////////////////////////////////////////

// Column

class column {
  string name;
  shared_ptr<ndarray> data;
  optional<string> description;

public:
  column() = default;
  column(const column &) = default;
  column(column &&) = default;
  column &operator=(const column &) = default;
  column &operator=(column &&) = default;

  column(const string &name, const shared_ptr<ndarray> &data,
         const optional<string> &description)
      : name(name), data(data), description(description) {}
};

// Table
class table {
  vector<shared_ptr<column>> columns;

public:
  table() = default;
  table(const table &) = default;
  table(table &&) = default;
  table &operator=(const table &) = default;
  table &operator=(table &&) = default;

  table(const vector<shared_ptr<column>> &columns) : columns(columns) {}
};

////////////////////////////////////////////////////////////////////////////////

class asdf : public writer {
public:
  asdf() = default;
  asdf(const asdf &) = default;
  asdf(asdf &&) = default;
  asdf &operator=(const asdf &) = default;
  asdf &operator=(asdf &&) = default;

  virtual YAML::Node to_yaml(writer_state &ws) const;
};

////////////////////////////////////////////////////////////////////////////////

void write_asdf(ostream &os, const asdf &objs);

} // namespace ASDF

#define ASDF_HPP_DONE
#endif // #ifndef ASDF_HPP
#ifndef ASDF_HPP_DONE
#error "Cyclic include depencency"
#endif
