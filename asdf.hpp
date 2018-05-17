#ifndef ASDF_HPP
#define ASDF_HPP

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cassert>
#include <complex>
#include <cstddef>
#include <cstdint>
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

// Byte order

enum class byteorder_t { big, little };

void yaml_decode(const YAML::Node &node, byteorder_t &byteorder);
YAML::Node yaml_encode(byteorder_t byteorder);

// inline byteorder_t host_byteorder() {
//   constexpr uint16_t magic{0x0102};
//   constexpr array<unsigned char, 2> magic_big{0x01, 0x02};
//   constexpr array<unsigned char, 2> magic_little{0x02, 0x01};
//   if (reinterpret_cast<array<unsigned char, 2>>(magic) == magic_big)
//     return byteorder_t::big;
//   if (reinterpret_cast<array<unsigned char, 2>>(magic) == magic_little)
//     return byteorder_t::little;
//   assert(0);
// }

constexpr uint16_t byteorder_magic = 1;
inline byteorder_t host_byteorder() {
  return reinterpret_cast<const array<unsigned char, 2> &>(
             byteorder_magic)[0] == 1
             ? byteorder_t::little
             : byteorder_t::big;
}

// Convert to host byte order
template <typename T>
inline T xtoh(const unsigned char *data, byteorder_t byteorder) {
  if (byteorder == host_byteorder())
    return *reinterpret_cast<const T *>(data);
  array<unsigned char, sizeof(T)> res;
  for (size_t i = 0; i < sizeof(T); ++i)
    res[i] = data[sizeof(T) - 1 - i];
  return *reinterpret_cast<const T *>(&res);
}

// Convert from host byte order
template <typename T>
inline array<unsigned char, sizeof(T)> htox(const T &val,
                                            byteorder_t byteorder) {
  const array<unsigned char, sizeof(T)> data =
      reinterpret_cast<const array<unsigned char, sizeof(T)>>(val);
  if (byteorder == host_byteorder())
    return data;
  array<unsigned char, sizeof(T)> res;
  for (size_t i = 0; i < sizeof(T); ++i)
    res[i] = data[sizeof(T) - 1 - i];
  return res;
}

template <size_t N>
inline void htox(unsigned char *val, byteorder_t byteorder) {
  if (byteorder != host_byteorder()) {
    // TODO: use std::reverse?
    array<unsigned char, N> tmp;
    for (size_t i = 0; i < N; ++i)
      tmp[i] = val[N - 1 - i];
    for (size_t i = 0; i < N; ++i)
      val[i] = tmp[i];
  }
}

////////////////////////////////////////////////////////////////////////////////

// I/O

enum class block_format_t { block, inline_array };
enum class compression_t { none, bzip2, zlib };

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

struct copy_state {
  bool set_block_format;
  block_format_t block_format;
  bool set_compression;
  compression_t compression;
};

class writer_state {

  vector<function<void(ostream &os)>> tasks;

public:
  writer_state(const writer_state &) = delete;
  writer_state(writer_state &&) = delete;
  writer_state &operator=(const writer_state &) = delete;
  writer_state &operator=(writer_state &&) = delete;

  writer_state();
  ~writer_state();

  int64_t add_task(function<void(ostream &)> &&task) {
    tasks.push_back(move(task));
    return tasks.size() - 1;
  }

  void flush(ostream &os);
};

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

  field_t(const string &name, const shared_ptr<datatype_t> &datatype,
          bool have_byteorder, byteorder_t byteorder,
          const vector<int64_t> &shape);

  field_t(const reader_state &rs, const YAML::Node &node);
  field_t(const copy_state &cs, const field_t &field);
  YAML::Node to_yaml(writer_state &ws) const;
};

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
  datatype_t(const vector<shared_ptr<field_t>> &fields);
  datatype_t(vector<shared_ptr<field_t>> &&fields);

  datatype_t(const reader_state &rs, const YAML::Node &node);
  datatype_t(const copy_state &cs, const datatype_t &datatype);
  YAML::Node to_yaml(writer_state &ws) const;

  size_t type_size() const;
};

void parse_scalar(const YAML::Node &node, unsigned char *data,
                  const shared_ptr<datatype_t> &datatype,
                  byteorder_t byteorder = host_byteorder());
YAML::Node emit_scalar(const unsigned char *data,
                       const shared_ptr<datatype_t> &datatype,
                       byteorder_t byteorder = host_byteorder());

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

// Multi-dimensional array

#if 0
class block_t {
  vector<unsigned char> data;
  shared_ptr<datatype_t> datatype;
  byteorder_t byteorder;
  compression_t compression;

public:
  blob_t() = delete;
  blob_t(const blob_t &) = delete;
  blob_t(blob_t &&) = delete;
  blob_t &operator=(const blob_t &) = delete;
  blob_t &operator=(blob_t &&) = delete;

  blob_t(const vector<unsigned char> &data,
         const shared_ptr<datatype_t> &datatype, byteorder_t byteorder,
         compression_t compression::none)
      : data(data), datatype(datatype), byteorder(byteorder),
        compression(compression) {
    assert(datatype);
  }
  blob_t(vector<unsigned char> &&data, const shared_ptr<datatype_t> &datatype,
         byteorder_t byteorder, compression_t compression::none)
      : data(move(data)), datatype(datatype), byteorder(byteorder),
        compression(compression) {
    assert(datatype);
  }

  block_t(istream&is);
  void write(ostream&os)const;
};
#endif

// TODO: Simplify this, avoid the abstract class
class generic_blob_t {

public:
  virtual ~generic_blob_t() {}

  virtual const void *ptr() const = 0;
  virtual void *ptr() = 0;
  virtual size_t bytes() const = 0;
  virtual void reserve(size_t bytes) = 0;
  virtual void resize(size_t bytes) = 0;
};

template <typename T> class blob_t : public generic_blob_t {
  vector<T> data;

public:
  blob_t() = delete;
  blob_t(const vector<T> &data) : data(data) {}
  blob_t(vector<T> &&data) : data(move(data)) {}

  virtual ~blob_t() {}

  virtual const void *ptr() const { return data.data(); }
  virtual void *ptr() { return data.data(); }
  virtual size_t bytes() const { return data.size() * sizeof(T); }
  virtual void reserve(size_t bytes) {
    assert(bytes % sizeof(T) == 0);
    data.reserve(bytes / sizeof(T));
  }
  virtual void resize(size_t bytes) {
    assert(bytes % sizeof(T) == 0);
    data.resize(bytes / sizeof(T));
  }
};

template <> class blob_t<bool> : public generic_blob_t {
  vector<unsigned char> data;

public:
  blob_t() = delete;

  blob_t(const vector<unsigned char> &data) : data(data) {}
  blob_t(vector<unsigned char> &&data) : data(move(data)) {}
  blob_t(const vector<bool> &data);

  virtual ~blob_t() {}

  virtual const void *ptr() const { return data.data(); }
  virtual void *ptr() { return data.data(); }
  virtual size_t bytes() const { return data.size(); }
  virtual void reserve(size_t bytes) { data.resize(bytes); }
  virtual void resize(size_t bytes) { data.resize(bytes); }
};

shared_ptr<generic_blob_t> read_block(istream &is);

class ndarray {
  shared_ptr<generic_blob_t> data;
  block_format_t block_format;
  compression_t compression;
  vector<bool> mask;
  shared_ptr<datatype_t> datatype;
  byteorder_t byteorder;
  vector<int64_t> shape;
  int64_t offset;
  vector<int64_t> strides;

  void write_block(ostream &os) const;

public:
  ndarray() = delete;
  ndarray(const ndarray &) = default;
  ndarray(ndarray &&) = default;
  ndarray &operator=(const ndarray &) = default;
  ndarray &operator=(ndarray &&) = default;

  ndarray(const shared_ptr<generic_blob_t> &data, block_format_t block_format,
          compression_t compression, const vector<bool> &mask,
          const shared_ptr<datatype_t> &datatype, byteorder_t byteorder,
          const vector<int64_t> &shape, int64_t offset = 0,
          const vector<int64_t> &strides1 = {})
      : data(data), block_format(block_format), compression(compression),
        mask(mask), datatype(datatype), byteorder(byteorder), shape(shape),
        offset(offset), strides(strides1) {
    // Check shape
    int rank = shape.size();
    for (int d = 0; d < rank; ++d)
      assert(shape[d] >= 0);
    // Check data size
    int64_t npoints = 1;
    for (int d = 0; d < rank; ++d)
      npoints *= shape[d];
    assert(data->bytes() == npoints * datatype->type_size());
    // Check mask
    if (!mask.empty())
      assert(mask.size() == npoints);
    // offset
    assert(offset >= 0);
    // Check strides
    if (strides.empty()) {
      strides.resize(rank);
      int64_t str = datatype->type_size();
      for (int d = rank - 1; d >= 0; --d) {
        strides.at(d) = str;
        str *= shape.at(d);
      }
    }
    assert(strides.size() == rank);
    for (int d = 0; d < rank; ++d)
      assert(strides.at(d) >= 1 || strides.at(d) <= -1);
    // TODO: check that strides are multiples of the element size
  }

  template <typename T>
  ndarray(const vector<T> &data, block_format_t block_format,
          compression_t compression, const vector<bool> &mask,
          const vector<int64_t> &shape, const vector<int64_t> &strides = {},
          int64_t offset = 0)
      : ndarray(make_shared<blob_t<T>>(data), block_format, compression, mask,
                make_shared<datatype_t>(get_scalar_type_id<T>::value),
                host_byteorder(), shape, offset, strides) {}
  template <typename T>
  ndarray(vector<T> &&data, block_format_t block_format,
          compression_t compression, const vector<bool> &mask,
          const vector<int64_t> &shape, const vector<int64_t> &strides = {},
          int64_t offset = 0)
      : ndarray(make_shared<blob_t<T>>(move(data)), block_format, compression,
                mask, make_shared<datatype_t>(get_scalar_type_id<T>::value),
                host_byteorder(), shape, offset, strides) {}

  ndarray(const reader_state &rs, const YAML::Node &node);
  ndarray(const copy_state &cs, const ndarray &arr);
  YAML::Node to_yaml(writer_state &ws) const;
};

inline YAML::Node make_yaml(writer_state &ws, const ndarray &arr) {
  return arr.to_yaml(ws);
}

////////////////////////////////////////////////////////////////////////////////

// Table and Column

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

  column(const reader_state &rs, const YAML::Node &node);
  column(const copy_state &cs, const column &col);
  YAML::Node to_yaml(writer_state &ws) const;
};

class table {
  vector<shared_ptr<column>> columns;

public:
  table() = delete;
  table(const table &) = default;
  table(table &&) = default;
  table &operator=(const table &) = default;
  table &operator=(table &&) = default;

  table(const vector<shared_ptr<column>> &columns) : columns(columns) {}

  table(const reader_state &rs, const YAML::Node &node);
  table(const copy_state &cs, const table &tab);
  YAML::Node to_yaml(writer_state &ws) const;
};

////////////////////////////////////////////////////////////////////////////////

// Group and Entry

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
  entry(const copy_state &cs, const entry &ent);
  YAML::Node to_yaml(writer_state &ws) const;
};

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
  group(const copy_state &cs, const group &grp);
  YAML::Node to_yaml(writer_state &ws) const;
};

////////////////////////////////////////////////////////////////////////////////

// ASDF

class asdf {
  map<string, shared_ptr<ndarray>> data;
  // fits
  // wcs
  // shared_ptr<table> tab;
  shared_ptr<group> grp; // SimulationIO

public:
  asdf() = default;
  asdf(const asdf &) = default;
  asdf(asdf &&) = default;
  asdf &operator=(const asdf &) = default;
  asdf &operator=(asdf &&) = default;

  asdf(const map<string, shared_ptr<ndarray>> &data) : data(data) {}
  // asdf(const shared_ptr<table> &tab) : tab(tab) { assert(tab); }
  asdf(const shared_ptr<group> &grp) : grp(grp) { assert(grp); }

  asdf(const reader_state &rs, const YAML::Node &node);
  asdf(const copy_state &cs, const asdf &project);
  YAML::Node to_yaml(writer_state &ws) const;

  asdf(istream &is);
  asdf copy(const copy_state &cs) const;
  void write(ostream &os) const;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ASDF

#define ASDF_HPP_DONE
#endif // #ifndef ASDF_HPP
#ifndef ASDF_HPP_DONE
#error "Cyclic include depencency"
#endif
