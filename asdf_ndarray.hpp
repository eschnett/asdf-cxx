#ifndef ASDF_NDARRAY_HPP
#define ASDF_NDARRAY_HPP

#include "asdf_datatype.hpp"
#include "asdf_io.hpp"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <cstddef>
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
  writer_state &to_yaml(writer_state &ws) const;
  friend writer_state &operator<<(writer_state &ws, const ndarray &arr) {
    return arr.to_yaml(ws);
  }
};

// inline writer_state &make_yaml(writer_state &ws, const ndarray &arr) {
//   return arr.to_yaml(ws);
// }

} // namespace ASDF

#define ASDF_NDARRAY_HPP_DONE
#endif // #ifndef ASDF_NDARRAY_HPP
#ifndef ASDF_NDARRAY_HPP_DONE
#error "Cyclic include depencency"
#endif
