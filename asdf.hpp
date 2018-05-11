#ifndef ASDF_HPP
#define ASDF_HPP

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
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

// A tuple that can hold a value of each type
typedef tuple<bool8_t, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t,
              uint32_t, uint64_t, float32_t, float64_t, complex64_t,
              complex128_t, ascii_t, ucs4_t>
    fake_scalar_type_t;

// Convert an enum id to its type
template <size_t I> struct get_scalar_type {
  typedef typename tuple_element<I, fake_scalar_type_t>::type type;
};
template <size_t I> using get_scalar_type_t = typename get_scalar_type<I>::type;

string yaml_encode(scalar_type_id_t scalar_type_id);

YAML::Node emit_scalar(const void *data, scalar_type_id_t scalar_type_id);

////////////////////////////////////////////////////////////////////////////////

// I/O

class writer_state {
  vector<function<void(ostream &os)>> tasks;

public:
  writer_state(const writer_state &) = delete;
  writer_state(writer_state &&) = delete;
  writer_state &operator=(const writer_state &) = delete;
  writer_state &operator=(writer_state &&) = delete;

  writer_state() = default;
  ~writer_state() { assert(tasks.empty()); }
  int64_t add_task(function<void(ostream &)> &&task) {
    tasks.push_back(move(task));
    return tasks.size() - 1;
  }
  void flush(ostream &os);
};

class writer {
public:
  virtual YAML::Node to_yaml(writer_state &ws) const = 0;
};

////////////////////////////////////////////////////////////////////////////////

// Multi-dimensional array

enum class block_format_t { block, inline_array };
enum class compression_t { none, bzip2, zlib };

template <typename T> struct copy_array {
  void operator()(vector<unsigned char> &dst, const vector<T> &src) const {
    dst.resize(src.size() * sizeof(T));
    memcpy(dst.data(), src.data(), dst.size());
  }
};
template <> struct copy_array<bool8_t> {
  void operator()(vector<unsigned char> &dst, const vector<bool> &src) const {
    dst.resize(src.size());
    for (size_t i = 0; i < dst.size(); ++i)
      dst[i] = src[i];
  }
};

class ndarray : public enable_shared_from_this<ndarray> {
  vector<unsigned char> data;
  block_format_t block_format;
  compression_t compression;
  vector<bool> mask;
  scalar_type_id_t scalar_type_id;
  vector<int64_t> shape;
  vector<int64_t> strides;
  int64_t offset;

  void write_block(ostream &os) const;

public:
  ndarray() = delete;
  ndarray(const ndarray &) = default;
  ndarray(ndarray &&) = default;
  ndarray &operator=(const ndarray &) = default;
  ndarray &operator=(ndarray &&) = default;

  template <typename T>
  ndarray(const vector<T> &data, block_format_t block_format,
          compression_t compression, const vector<bool> &mask,
          const vector<int64_t> &shape,
          const vector<int64_t> &strides = vector<int64_t>(),
          int64_t offset = 0) {
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
    copy_array<T>()(this->data, data);
    // block_format
    this->block_format = block_format;
    // compression
    this->compression = compression;
    // mask
    if (!mask.empty())
      assert(mask.size() == npoints);
    this->mask = mask;
    // strides
    if (strides.empty()) {
      // Default: contiguous and in C order
      this->strides.resize(rank);
      int64_t str = sizeof(T);
      for (int d = rank - 1; d >= 0; --d) {
        this->strides[d] = str;
        str *= shape[d];
      }
    }
    assert(this->strides.size() == rank);
    for (int d = 0; d < rank; ++d)
      assert(this->strides[d] >= 1 || this->strides[d] <= -1);
    // TODO: check that strides are multiples of the element size
    // offset
    assert(offset >= 0);
    this->offset = offset;
  }

  virtual YAML::Node to_yaml(writer_state &ws) const;
};

////////////////////////////////////////////////////////////////////////////////

// Column

class column {
  string name;
  shared_ptr<ndarray> data;
  string description;

public:
  column() = delete;
  column(const column &) = default;
  column(column &&) = default;
  column &operator=(const column &) = default;
  column &operator=(column &&) = default;

  column(const string &name, const shared_ptr<ndarray> &data,
         const string &description)
      : name(name), data(data), description(description) {
    assert(!name.empty());
    assert(data);
  }

  virtual YAML::Node to_yaml(writer_state &ws) const;
};

// Table
class table {
  vector<shared_ptr<column>> columns;

public:
  table() = delete;
  table(const table &) = default;
  table(table &&) = default;
  table &operator=(const table &) = default;
  table &operator=(table &&) = default;

  table(const vector<shared_ptr<column>> &columns) : columns(columns) {}

  virtual YAML::Node to_yaml(writer_state &ws) const;
};

////////////////////////////////////////////////////////////////////////////////

class asdf : public writer {
  vector<shared_ptr<table>> tables;

public:
  asdf() = delete;
  asdf(const asdf &) = default;
  asdf(asdf &&) = default;
  asdf &operator=(const asdf &) = default;
  asdf &operator=(asdf &&) = default;

  asdf(const vector<shared_ptr<table>> &tables) : tables(tables) {}

  virtual YAML::Node to_yaml(writer_state &ws) const;

  void write(ostream &os) const;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ASDF

#define ASDF_HPP_DONE
#endif // #ifndef ASDF_HPP
#ifndef ASDF_HPP_DONE
#error "Cyclic include depencency"
#endif
