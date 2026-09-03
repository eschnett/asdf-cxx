#ifndef ASDF_NDARRAY_HXX
#define ASDF_NDARRAY_HXX

#include <asdf/datatype.hxx>
#include <asdf/io.hxx>
#include <asdf/memoized.hxx>

#include <yaml-cpp/yaml.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
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
  typed_block_t(vector<T> data1) : data(std::move(data1)) {}

  virtual ~typed_block_t() {}

  virtual const void *ptr() const override { return data.data(); }
  virtual void *ptr() override { return data.data(); }
  virtual size_t nbytes() const override { return data.size() * sizeof(T); }
  virtual void reserve(size_t nbytes) override {
    ASDF_CHECK(nbytes % sizeof(T) == 0,
               "Block size is not a multiple of the element size");
    data.reserve(nbytes / sizeof(T));
  }
  virtual void resize(size_t nbytes) override {
    ASDF_CHECK(nbytes % sizeof(T) == 0,
               "Block size is not a multiple of the element size");
    data.resize(nbytes / sizeof(T));
  }
};

template <> class typed_block_t<bool> : public block_t {
  vector<unsigned char> data;

public:
  typed_block_t() = delete;

  typed_block_t(vector<unsigned char> data1) : data(std::move(data1)) {}
  typed_block_t(const vector<bool> &data);

  virtual ~typed_block_t() {}

  virtual const void *ptr() const override { return data.data(); }
  virtual void *ptr() override { return data.data(); }
  virtual size_t nbytes() const override { return data.size(); }
  virtual void reserve(size_t nbytes) override { data.resize(nbytes); }
  virtual void resize(size_t nbytes) override { data.resize(nbytes); }
};

class ptr_block_t : public block_t {
  void *data;
  size_t size;

public:
  ptr_block_t() = delete;

  ptr_block_t(void *data, size_t size) : data(data), size(size) {
    ASDF_CHECK(data, "A ptr_block_t requires a non-null pointer");
  }
  template <typename T>
  ptr_block_t(vector<T> &data)
      : data(data.data()), size(data.size() * sizeof(T)) {}

  virtual ~ptr_block_t() {}

  virtual const void *ptr() const override { return data; }
  virtual void *ptr() override { return data; }
  virtual size_t nbytes() const override { return size; }
  virtual void reserve(size_t nbytes) override {
    ASDF_ERROR("A ptr_block_t cannot be resized");
  }
  virtual void resize(size_t nbytes) override {
    ASDF_ERROR("A ptr_block_t cannot be resized");
  }
};

// Information about a block
// TODO: Rename block_t -> block_data_t, create new block_t as
// tuple<memoized<block>, block_info>
struct block_info_t {
  array<unsigned char, 4> token;
  uint16_t header_size;
  int64_t header_read;
  uint32_t flags;
  array<unsigned char, 4> comp;
  compression_t compression;
  uint64_t allocated_space;
  uint64_t used_space;
  uint64_t data_space;
  array<unsigned char, 16> checksum;
};

// ndarray

class ndarray {
  memoized<block_t> mdata;
  std::optional<block_info_t> block_info; // TODO: remove duplicate information

  block_format_t block_format;
  compression_t compression; // TODO: move to block_t
  int compression_level;     // TODO: move to block_t
  vector<bool> mask;
  shared_ptr<datatype_t> datatype;
  byteorder_t byteorder; // TODO: move to block_t
  vector<int64_t> shape;
  int64_t offset;
  vector<int64_t> strides;

  void write_block(ostream &os) const;

public:
  static std::tuple<memoized<block_t>, block_info_t>
  read_block(const shared_ptr<istream> &is);

  ndarray() = delete;
  ndarray(const ndarray &) = default;
  ndarray(ndarray &&) = default;
  ndarray &operator=(const ndarray &) = default;
  ndarray &operator=(ndarray &&) = default;

  ndarray(memoized<block_t> mdata1, std::optional<block_info_t> block_info,
          block_format_t block_format, compression_t compression,
          int compression_level, vector<bool> mask1,
          shared_ptr<datatype_t> datatype1, byteorder_t byteorder,
          vector<int64_t> shape1, int64_t offset = 0,
          vector<int64_t> strides1 = {})
      : mdata(std::move(mdata1)), block_info(block_info),
        block_format(block_format), compression(compression),
        compression_level(compression_level), mask(std::move(mask1)),
        datatype(std::move(datatype1)), byteorder(byteorder),
        shape(std::move(shape1)), offset(offset), strides(std::move(strides1)) {
    // Check shape (this rejects negative extents and an overflowing size)
    int rank = shape.size();
    const int64_t npoints = num_elements();
    packed_nbytes();
    // Check mask
    if (!mask.empty())
      ASDF_CHECK(int64_t(mask.size()) == npoints,
                 "Mask size does not match the array shape");
    // offset
    ASDF_CHECK(offset >= 0, "Array offset must not be negative");
    // Check strides
    if (strides.empty()) {
      strides.resize(rank);
      int64_t str = datatype->type_size();
      for (int d = rank - 1; d >= 0; --d) {
        strides.at(d) = str;
        str *= shape.at(d);
      }
    }
    ASDF_CHECK(int(strides.size()) == rank,
               "Number of strides does not match the array rank");
    for (int d = 0; d < rank; ++d)
      ASDF_CHECK(strides.at(d) != 0, "Array strides must not be zero");
    // TODO: check that strides are multiples of the element size
    // Check that the described elements lie inside the block. When the
    // block comes from a file its header size is authoritative, so that
    // this does not load the data.
    ASDF_CHECK(mdata.valid(), "An ndarray requires a data block");
    check_bounds(this->block_info ? this->block_info->data_space
                                  : mdata->nbytes());
  }

  template <typename T>
  ndarray(vector<T> data1, block_format_t block_format,
          compression_t compression, int compression_level, vector<bool> mask1,
          vector<int64_t> shape1, int64_t offset = 0,
          vector<int64_t> strides1 = {})
      : ndarray(make_constant_memoized(shared_ptr<block_t>(
                    make_shared<typed_block_t<T>>(std::move(data1)))),
                std::optional<block_info_t>(), block_format, compression,
                compression_level, std::move(mask1),
                make_shared<datatype_t>(get_scalar_type_id<T>()),
                host_byteorder(), std::move(shape1), offset,
                std::move(strides1)) {}

  ndarray(const shared_ptr<reader_state> &rs, const YAML::Node &node);
  ndarray(const copy_state &cs, const ndarray &arr);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const ndarray &arr) {
    return arr.to_yaml(w);
  }

private:
  // The elements described by `shape`, `offset` and `strides` must lie inside
  // a block of `nbytes` bytes
  void check_bounds(uint64_t nbytes) const;
  // Complain unless this array's datatype is the scalar type `requested`
  void check_scalar_type(scalar_type_id_t requested) const;

public:
  memoized<block_t> get_data() const { return mdata; }
  memoized<block_t> get_data() { return mdata; }

  // Only available after reading a file, not available while writing
  std::optional<block_info_t> get_block_info() const { return block_info; }

  // The array's elements in C order, converted to the host byte order and
  // packed contiguously: `offset` and `strides` are applied, so the result
  // has `num_elements()` elements of `get_datatype()->type_size()` bytes
  // each. Structured datatypes are converted field by field, and a field's
  // own byte order takes precedence over the array's.
  vector<unsigned char> get_data_bytes() const;

  // The array's elements as a `vector<T>`, in C order and host byte order.
  // `T` must match the array's (scalar) datatype.
  template <typename T> vector<T> get_data_vector() const {
    check_scalar_type(get_scalar_type_id<T>());
    const vector<unsigned char> bytes = get_data_bytes();
    const size_t npoints = size_t(num_elements());
    ASDF_CHECK(bytes.size() == npoints * sizeof(T),
               "Block size does not match the array shape");
    vector<T> data(npoints);
    if (npoints > 0)
      std::memcpy(data.data(), bytes.data(), bytes.size());
    return data;
  }

  shared_ptr<datatype_t> get_datatype() const { return datatype; }
  byteorder_t get_byteorder() const { return byteorder; }
  const vector<bool> &get_mask() const { return mask; }
  block_format_t get_block_format() const { return block_format; }
  compression_t get_compression() const { return compression; }
  int get_compression_level() const { return compression_level; }
  vector<int64_t> get_shape() const { return shape; }
  int64_t get_offset() const { return offset; }
  vector<int64_t> get_strides() const { return strides; }

  // The number of elements `shape` describes. A file can claim any shape, so
  // this also rejects extents whose product does not fit into an `int64_t`.
  int64_t num_elements() const {
    int64_t npoints = 1;
    for (const auto extent : shape) {
      ASDF_CHECK(extent >= 0, "Array shape must not have negative extents");
      ASDF_CHECK(extent == 0 ||
                     npoints <= numeric_limits<int64_t>::max() / extent,
                 "Array shape is too large: the number of elements does not "
                 "fit into a 64-bit integer");
      npoints *= extent;
    }
    return npoints;
  }

  // The size of a densely packed C-order array of this shape and datatype.
  // Filling in default strides is only safe once this has succeeded.
  int64_t packed_nbytes() const {
    const int64_t npoints = num_elements();
    const int64_t elemsize = datatype->type_size();
    ASDF_CHECK(npoints == 0 ||
                   elemsize <= numeric_limits<int64_t>::max() / npoints,
               "Array shape is too large: its size in bytes does not fit "
               "into a 64-bit integer");
    return npoints * elemsize;
  }

  // Do `strides` describe a densely packed C-order array?
  bool is_c_contiguous() const {
    const int rank = shape.size();
    int64_t str = datatype->type_size();
    for (int d = rank - 1; d >= 0; --d) {
      if (strides.at(d) != str)
        return false;
      str *= shape.at(d);
    }
    return true;
  }

  // The **byte** offset of element `idx` within the block, i.e. `offset`
  // plus the strided contribution of every index
  int64_t linear_index(const vector<int64_t> &idx) const {
    int rank = shape.size();
    ASDF_CHECK(int(idx.size()) == rank,
               "Index rank does not match the array rank");
    int64_t lin = offset;
    for (int d = 0; d < rank; ++d) {
      ASDF_CHECK(idx[d] >= 0 && idx[d] < shape[d], "Array index out of bounds");
      lin += strides[d] * idx[d];
    }
    return lin;
  }
  // The **byte** offset of element `idx` within the block
  template <size_t D> int64_t linear_index(const array<int64_t, D> &idx) const {
    int rank = shape.size();
    ASDF_CHECK(int(D) == rank, "Index rank does not match the array rank");
    int64_t lin = offset;
    for (int d = 0; d < rank; ++d) {
      ASDF_CHECK(idx[d] >= 0 && idx[d] < shape[d], "Array index out of bounds");
      lin += strides[d] * idx[d];
    }
    return lin;
  }
};

// `vector<bool>` is a bit vector without a `data()` pointer, and the ASDF
// `bool8` datatype stores one byte per element, so any nonzero byte is true
template <> inline vector<bool> ndarray::get_data_vector<bool>() const {
  check_scalar_type(id_bool8);
  const vector<unsigned char> bytes = get_data_bytes();
  ASDF_CHECK(bytes.size() == size_t(num_elements()),
             "Block size does not match the array shape");
  vector<bool> data(bytes.size());
  for (size_t i = 0; i < bytes.size(); ++i)
    data[i] = bytes[i] != 0;
  return data;
}

} // namespace ASDF

#define ASDF_NDARRAY_HXX_DONE
#endif // #ifndef ASDF_NDARRAY_HXX
#ifndef ASDF_NDARRAY_HXX_DONE
#error "Cyclic include depencency"
#endif
