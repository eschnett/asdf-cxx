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
#include <istream>
#include <map>
#include <memory>
#include <string>
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

// Convert an enum id to its type size
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

YAML::Node emit_scalar(const void *data, scalar_type_id_t scalar_type_id);

////////////////////////////////////////////////////////////////////////////////

// STL

template <typename T>
void yaml_decode(const YAML::Node &node, vector<T> &data) {
  data.reserve(node.size());
  for (YAML::const_iterator ni = node.begin(); ni != node.end(); ++ni) {
    T value;
    yaml_decode(*ni, value);
    data.push_back(move(value));
  }
}

template <typename K, typename T>
void yaml_decode(const YAML::Node &node, map<K, T> &data) {
  for (YAML::const_iterator ni = node.begin(); ni != node.end(); ++ni) {
    K key;
    yaml_decode(ni->first, key);
    T value;
    yaml_decode(ni->second, value);
    data[move(key)] = move(value);
  }
}

template <typename T> YAML::Node yaml_encode(const vector<T> &data) {
  YAML::Node node;
  node.SetStyle(YAML::EmitterStyle::Flow);
  node = data;
  node.SetStyle(YAML::EmitterStyle::Flow);
  return node;
}

template <typename K, typename T>
YAML::Node yaml_encode(const map<K, T> &data) {
  YAML::Node node;
  node = data;
  return node;
}

////////////////////////////////////////////////////////////////////////////////

// I/O

class generic_blob_t;
shared_ptr<generic_blob_t> read_block(istream &is);

class reader_state {
  // TODO: Store only the file position
  vector<shared_ptr<generic_blob_t>> blocks;

public:
  reader_state() = delete;
  reader_state(const reader_state &) = delete;
  reader_state(reader_state &&) = delete;
  reader_state &operator=(const reader_state &) = delete;
  reader_state &operator=(reader_state &&) = delete;

  reader_state(istream &is);

  shared_ptr<generic_blob_t> get_block(int64_t index) const {
    assert(index >= 0);
    return blocks.at(index);
  }
};

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

// TODO: remove this class
class writable_t {
public:
  virtual ~writable_t() {}
  virtual YAML::Node to_yaml(writer_state &ws) const = 0;
};

template <typename T> struct writable_vector : writable_t {
  vector<T> data;
  virtual YAML::Node to_yaml(writer_state &ws) const {
    YAML::Node node;
    for (const auto &v : data)
      node.push_back(make_yaml(ws, v));
    return node;
  }
};

template <typename K, typename T> struct writable_map : writable_t {
  map<K, T> data;
  YAML::Node to_yaml(writer_state &ws) const {
    YAML::Node node;
    for (const auto &kv : data)
      node[kv.first] = make_yaml(ws, kv.second);
    return node;
  }
};

////////////////////////////////////////////////////////////////////////////////

// Multi-dimensional array

enum class block_format_t { block, inline_array };
enum class compression_t { none, bzip2, zlib };

class generic_blob_t {

  compression_t compression;

public:
  generic_blob_t(compression_t compression) : compression(compression) {}
  virtual ~generic_blob_t() {}

  compression_t get_compression() const { return compression; }

  virtual const void *ptr() const = 0;
  virtual void *ptr() = 0;
  virtual size_t bytes() const = 0;
  virtual void resize(size_t bytes) = 0;
};

template <typename T> class blob_t : public generic_blob_t {
  vector<T> data;

public:
  blob_t() = delete;
  blob_t(compression_t compression, const vector<T> &data)
      : generic_blob_t(compression), data(data) {}
  blob_t(compression_t compression, vector<T> &&data)
      : generic_blob_t(compression), data(move(data)) {}

  virtual ~blob_t() {}

  virtual const void *ptr() const { return data.data(); }
  virtual void *ptr() { return data.data(); }
  virtual size_t bytes() const { return data.size() * sizeof(T); }
  virtual void resize(size_t bytes) {
    assert(bytes % sizeof(T) == 0);
    data.resize(bytes / sizeof(T));
  }
};

template <> class blob_t<bool> : public generic_blob_t {
  vector<unsigned char> data;

public:
  blob_t() = delete;

  blob_t(compression_t compression, const vector<unsigned char> &data)
      : generic_blob_t(compression), data(data) {}
  blob_t(compression_t compression, vector<unsigned char> &&data)
      : generic_blob_t(compression), data(move(data)) {}
  blob_t(compression_t compression, const vector<bool> &data)
      : generic_blob_t(compression) {
    this->data.resize(data.size());
    for (size_t i = 0; i < this->data.size(); ++i)
      this->data[i] = data[i];
  }

  virtual ~blob_t() {}

  virtual const void *ptr() const { return data.data(); }
  virtual void *ptr() { return data.data(); }
  virtual size_t bytes() const { return data.size(); }
  virtual void resize(size_t bytes) { data.resize(bytes); }
};

shared_ptr<generic_blob_t> read_block(istream &is);

class ndarray : writable_t {
  shared_ptr<generic_blob_t> data;
  block_format_t block_format;
  // compression_t compression;
  vector<bool> mask;
  // TODO: allow general datatypes
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

  ndarray(const shared_ptr<generic_blob_t> &data, block_format_t block_format,
          // compression_t compression,
          const vector<bool> &mask, scalar_type_id_t scalar_type_id,
          const vector<int64_t> &shape, const vector<int64_t> &strides1 = {},
          int64_t offset = 0)
      : data(data), block_format(block_format),
        // compression(compression),
        mask(mask), scalar_type_id(scalar_type_id), shape(shape),
        strides(strides1), offset(offset) {
    // Check shape
    int rank = shape.size();
    for (int d = 0; d < rank; ++d)
      assert(shape[d] >= 0);
    // Check data size
    int64_t npoints = 1;
    for (int d = 0; d < rank; ++d)
      npoints *= shape[d];
    assert(data->bytes() == npoints * get_scalar_type_size(scalar_type_id));
    // Check mask
    if (!mask.empty())
      assert(mask.size() == npoints);
    // Check strides
    if (strides.empty()) {
      strides.resize(rank);
      int64_t str = 1;
      for (int d = rank - 1; d >= 0; --d) {
        strides.at(d) = str;
        str *= shape.at(d);
      }
    }
    assert(strides.size() == rank);
    for (int d = 0; d < rank; ++d)
      assert(strides.at(d) >= 1 || strides.at(d) <= -1);
    // TODO: check that strides are multiples of the element size
    // offset
  }

  template <typename T>
  ndarray(const vector<T> &data, block_format_t block_format,
          compression_t compression, const vector<bool> &mask,
          const vector<int64_t> &shape, const vector<int64_t> &strides = {},
          int64_t offset = 0)
      : ndarray(make_shared<blob_t<T>>(compression, data), block_format, mask,
                get_scalar_type_id<T>::value, shape, strides, offset) {}
  template <typename T>
  ndarray(vector<T> &&data, block_format_t block_format,
          compression_t compression, const vector<bool> &mask,
          const vector<int64_t> &shape, const vector<int64_t> &strides = {},
          int64_t offset = 0)
      : ndarray(make_shared<blob_t<T>>(compression, move(data)), block_format,
                mask, get_scalar_type_id<T>::value, shape, strides, offset) {}

  ndarray(const reader_state &rs, const YAML::Node &node);
  virtual YAML::Node to_yaml(writer_state &ws) const;
};

inline YAML::Node make_yaml(writer_state &ws, const ndarray &arr) {
  return arr.to_yaml(ws);
}

////////////////////////////////////////////////////////////////////////////////

// Column

class column : writable_t {
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

  column(const reader_state &rs, const YAML::Node &node);
  virtual YAML::Node to_yaml(writer_state &ws) const;
};

// Table
class table : writable_t {
  vector<shared_ptr<column>> columns;

public:
  table() = delete;
  table(const table &) = default;
  table(table &&) = default;
  table &operator=(const table &) = default;
  table &operator=(table &&) = default;

  table(const vector<shared_ptr<column>> &columns) : columns(columns) {}

  table(const reader_state &rs, const YAML::Node &node);
  virtual YAML::Node to_yaml(writer_state &ws) const;
};

////////////////////////////////////////////////////////////////////////////////

// Entry

class group;

class entry {
  string name;
  shared_ptr<group> grp;
  shared_ptr<ndarray> arr;
  string description;

public:
  entry() = delete;
  entry(const entry &) = default;
  entry(entry &&) = default;
  entry &operator=(const entry &) = default;
  entry &operator=(entry &&) = default;

  entry(const string &name, const shared_ptr<ndarray> &arr,
        const string &description)
      : name(name), arr(arr), description(description) {
    assert(!name.empty());
    assert(arr);
  }
  entry(const string &name, const shared_ptr<group> &grp,
        const string &description)
      : name(name), grp(grp), description(description) {
    assert(!name.empty());
    assert(grp);
  }

  entry(const reader_state &rs, const YAML::Node &node);
  virtual YAML::Node to_yaml(writer_state &ws) const;
};

// Group
class group {
  map<string, shared_ptr<entry>> entries;

public:
  group() = default;
  group(const group &) = default;
  group(group &&) = default;
  group &operator=(const group &) = default;
  group &operator=(group &&) = default;

  group(const map<string, shared_ptr<entry>> &entries) : entries(entries) {}

  group(const reader_state &rs, const YAML::Node &node);
  virtual YAML::Node to_yaml(writer_state &ws) const;
};

////////////////////////////////////////////////////////////////////////////////

class asdf {
  shared_ptr<table> tab;
  shared_ptr<group> grp;

public:
  asdf() = default;
  asdf(const asdf &) = default;
  asdf(asdf &&) = default;
  asdf &operator=(const asdf &) = default;
  asdf &operator=(asdf &&) = default;

  asdf(const shared_ptr<table> &tab) : tab(tab) { assert(tab); }
  asdf(const shared_ptr<group> &grp) : grp(grp) { assert(grp); }

  asdf(const reader_state &rs, const YAML::Node &node);
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
