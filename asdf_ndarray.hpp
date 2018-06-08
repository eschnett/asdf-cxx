#ifndef ASDF_NDARRAY_HPP
#define ASDF_NDARRAY_HPP

#include "asdf_datatype.hpp"
#include "asdf_io.hpp"
#include "asdf_memoized.hpp"

#include <yaml-cpp/yaml.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>

namespace ASDF {
using namespace std;

// Multi-dimensional array

// TODO: Simplify this, avoid the abstract class
class block_t {

public:
  virtual ~block_t() {}

  virtual const void *ptr() const = 0;
  virtual void *ptr() = 0;
  virtual size_t nbytes() const = 0;
  virtual void reserve(size_t nbytes) = 0;
  virtual void resize(size_t nbytes) = 0;
};

template <typename T> class typed_block_t : public block_t {
  vector<T> data;

public:
  typed_block_t() = delete;
  typed_block_t(vector<T> data1) : data(move(data1)) {}

  virtual ~typed_block_t() {}

  virtual const void *ptr() const { return data.data(); }
  virtual void *ptr() { return data.data(); }
  virtual size_t nbytes() const { return data.size() * sizeof(T); }
  virtual void reserve(size_t nbytes) {
    assert(nbytes % sizeof(T) == 0);
    data.reserve(nbytes / sizeof(T));
  }
  virtual void resize(size_t nbytes) {
    assert(nbytes % sizeof(T) == 0);
    data.resize(nbytes / sizeof(T));
  }
};

template <> class typed_block_t<bool> : public block_t {
  vector<unsigned char> data;

public:
  typed_block_t() = delete;

  typed_block_t(vector<unsigned char> data1) : data(move(data1)) {}
  typed_block_t(const vector<bool> &data);

  virtual ~typed_block_t() {}

  virtual const void *ptr() const { return data.data(); }
  virtual void *ptr() { return data.data(); }
  virtual size_t nbytes() const { return data.size(); }
  virtual void reserve(size_t nbytes) { data.resize(nbytes); }
  virtual void resize(size_t nbytes) { data.resize(nbytes); }
};

class ptr_typed_block_t : public block_t {
  void *data;
  size_t size;

public:
  ptr_typed_block_t() = delete;

  ptr_typed_block_t(void *data, size_t size) : data(data), size(size) {
    assert(data);
  }
  template <typename T>
  ptr_typed_block_t(vector<T> &data)
      : data(data.data()), size(data.size() * sizeof(T)) {}

  virtual ~ptr_typed_block_t() {}

  virtual const void *ptr() const { return data; }
  virtual void *ptr() { return data; }
  virtual size_t nbytes() const { return size; }
  virtual void reserve(size_t nbytes) { assert(0); }
  virtual void resize(size_t nbytes) { assert(0); }
};

class ndarray {
  memoized<block_t> mdata;
  block_format_t block_format;
  compression_t compression; // TODO: move to block_t
  vector<bool> mask;
  shared_ptr<datatype_t> datatype;
  byteorder_t byteorder; // TODO: move to block_t
  vector<int64_t> shape;
  int64_t offset;
  vector<int64_t> strides;

  void write_block(ostream &os) const;

public:
  static memoized<block_t> read_block(const shared_ptr<istream> &is);

  ndarray() = delete;
  ndarray(const ndarray &) = default;
  ndarray(ndarray &&) = default;
  ndarray &operator=(const ndarray &) = default;
  ndarray &operator=(ndarray &&) = default;

  ndarray(memoized<block_t> mdata1, block_format_t block_format,
          compression_t compression, vector<bool> mask1,
          shared_ptr<datatype_t> datatype1, byteorder_t byteorder,
          vector<int64_t> shape1, int64_t offset = 0,
          vector<int64_t> strides1 = {})
      : mdata(move(mdata1)), block_format(block_format),
        compression(compression), mask(move(mask1)), datatype(move(datatype1)),
        byteorder(byteorder), shape(move(shape1)), offset(offset),
        strides(move(strides1)) {
    // Check shape
    int rank = shape.size();
    for (int d = 0; d < rank; ++d)
      assert(shape[d] >= 0);
    // Check data size
    int64_t npoints = 1;
    for (int d = 0; d < rank; ++d)
      npoints *= shape[d];
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
  ndarray(vector<T> data1, block_format_t block_format,
          compression_t compression, vector<bool> mask1, vector<int64_t> shape1,
          int64_t offset = 0, vector<int64_t> strides1 = {})
      : ndarray(make_constant_memoized(shared_ptr<block_t>(
                    make_shared<typed_block_t<T>>(move(data1)))),
                block_format, compression, move(mask1),
                make_shared<datatype_t>(get_scalar_type_id<T>::value),
                host_byteorder(), move(shape1), offset, move(strides1)) {}

  ndarray(const reader_state &rs, const YAML::Node &node);
  ndarray(const copy_state &cs, const ndarray &arr);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const ndarray &arr) {
    return arr.to_yaml(w);
  }

private:
  void check_shape() const;

public:
  const memoized<block_t> &get_data() const {
    // check_shape();
    return mdata;
  }
  memoized<block_t> get_data() {
    // check_shape();
    return mdata;
  }

  shared_ptr<datatype_t> get_datatype() const { return datatype; }
  vector<int64_t> get_shape() const { return shape; }
  int64_t get_offset() const { return offset; }
  vector<int64_t> get_strides() const { return strides; }

  int64_t linear_index(const vector<int64_t> &idx) const {
    int dim = shape.size();
    assert(idx.size() == dim);
    int64_t lin = offset;
    for (int d = 0; d < dim; ++d) {
      assert(idx[d] >= 0 && idx[d] < shape[d]);
      lin += strides[d] * idx[d];
    }
    return lin;
  }
  template <size_t D> int64_t linear_index(const array<int64_t, D> &idx) const {
    int dim = shape.size();
    assert(D == dim);
    int64_t lin = offset;
    for (int d = 0; d < dim; ++d) {
      assert(idx[d] >= 0 && idx[d] < shape[d]);
      lin += strides[d] * idx[d];
    }
    return lin;
  }
};

} // namespace ASDF

#define ASDF_NDARRAY_HPP_DONE
#endif // #ifndef ASDF_NDARRAY_HPP
#ifndef ASDF_NDARRAY_HPP_DONE
#error "Cyclic include depencency"
#endif
