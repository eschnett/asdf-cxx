#ifndef ASDF_NDARRAY_HPP
#define ASDF_NDARRAY_HPP

#include "asdf_datatype.hpp"
#include "asdf_io.hpp"

#include <yaml-cpp/yaml.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <future>
#include <memory>
#include <vector>

namespace ASDF {
using namespace std;

// Multi-dimensional array

// TODO: Simplify this, avoid the abstract class
class generic_blob_t {

public:
  virtual ~generic_blob_t() {}

  virtual const void *ptr() const = 0;
  virtual void *ptr() = 0;
  virtual size_t nbytes() const = 0;
  virtual void reserve(size_t nbytes) = 0;
  virtual void resize(size_t nbytes) = 0;
};

template <typename T> class blob_t : public generic_blob_t {
  vector<T> data;

public:
  blob_t() = delete;
  blob_t(vector<T> data1) : data(move(data1)) {}

  virtual ~blob_t() {}

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

template <> class blob_t<bool> : public generic_blob_t {
  vector<unsigned char> data;

public:
  blob_t() = delete;

  blob_t(vector<unsigned char> data1) : data(move(data1)) {}
  blob_t(const vector<bool> &data);

  virtual ~blob_t() {}

  virtual const void *ptr() const { return data.data(); }
  virtual void *ptr() { return data.data(); }
  virtual size_t nbytes() const { return data.size(); }
  virtual void reserve(size_t nbytes) { data.resize(nbytes); }
  virtual void resize(size_t nbytes) { data.resize(nbytes); }
};

class ptr_blob_t : public generic_blob_t {
  void *data;
  size_t size;

public:
  ptr_blob_t() = delete;

  ptr_blob_t(void *data, size_t size) : data(data), size(size) { assert(data); }
  template <typename T>
  ptr_blob_t(vector<T> &data)
      : data(data.data()), size(data.size() * sizeof(T)) {}

  virtual ~ptr_blob_t() {}

  virtual const void *ptr() const { return data; }
  virtual void *ptr() { return data; }
  virtual size_t nbytes() const { return size; }
  virtual void reserve(size_t nbytes) { assert(0); }
  virtual void resize(size_t nbytes) { assert(0); }
};

shared_future<shared_ptr<generic_blob_t>>
read_block(const shared_ptr<istream> &is);

class ndarray {
  shared_future<shared_ptr<generic_blob_t>> fdata;
  block_format_t block_format;
  compression_t compression;
  vector<bool> mask;
  shared_ptr<datatype_t> datatype;
  byteorder_t byteorder;
  vector<int64_t> shape;
  int64_t offset;
  vector<int64_t> strides;

  void write_block(ostream &os) const;

  // Modelled after std::experimental::make_ready_future
  template <typename T>
  future<typename decay<T>::type> make_ready_future(T &&value) {
    typedef typename decay<T>::type R;
    promise<R> p;
    p.set_value(forward<T>(value));
    return p.get_future();
  }

  // Modelled after std::make_shared
  template <typename T, class... Args> future<T> make_future(Args &&... args) {
    promise<T> p;
    p.set_value(T(forward<Args...>(args)...));
    return p.get_future();
  }

public:
  ndarray() = delete;
  ndarray(const ndarray &) = default;
  ndarray(ndarray &&) = default;
  ndarray &operator=(const ndarray &) = default;
  ndarray &operator=(ndarray &&) = default;

  ndarray(shared_future<shared_ptr<generic_blob_t>> fdata1,
          block_format_t block_format, compression_t compression,
          vector<bool> mask1, shared_ptr<datatype_t> datatype1,
          byteorder_t byteorder, vector<int64_t> shape1, int64_t offset = 0,
          vector<int64_t> strides1 = {})
      : fdata(move(fdata1)), block_format(block_format),
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
      : ndarray(make_future<shared_ptr<generic_blob_t>>(
                    make_shared<blob_t<T>>(move(data1))),
                block_format, compression, move(mask1),
                make_shared<datatype_t>(get_scalar_type_id<T>::value),
                host_byteorder(), move(shape1), offset, move(strides1)) {}

  ndarray(const reader_state &rs, const YAML::Node &node);
  ndarray(const copy_state &cs, const ndarray &arr);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const ndarray &arr) {
    return arr.to_yaml(w);
  }

  shared_ptr<generic_blob_t> get_data() const {
    int rank = shape.size();
    int64_t npoints = 1;
    for (int d = 0; d < rank; ++d)
      npoints *= shape[d];
    assert(fdata.get()->nbytes() == npoints * datatype->type_size());
    return fdata.get();
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
