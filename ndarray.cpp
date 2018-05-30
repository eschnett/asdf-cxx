#include "asdf_ndarray.hpp"

#include "asdf_config.hpp"
#include "asdf_stl.hpp"

#ifdef ASDF_HAVE_BZIP2
#include <bzlib.h>
#endif

#ifdef ASDF_HAVE_OPENSSL
#include <openssl/md5.h>
#endif

#ifdef ASDF_HAVE_ZLIB
#include <zlib.h>
#endif

namespace ASDF {

// Multi-dimensional array

blob_t<bool>::blob_t(const vector<bool> &data) {
  this->data.resize(data.size());
  for (size_t i = 0; i < this->data.size(); ++i)
    this->data[i] = data[i];
}

void parse_inline_array_nd(const YAML::Node &node,
                           const shared_ptr<datatype_t> &datatype,
                           const vector<int64_t> &shape, int rank,
                           vector<unsigned char> &data) {
  assert(rank >= 0);
  assert(shape.size() >= rank);
  if (rank == 0) {
    assert(node.IsScalar());
    size_t oldsize = data.size();
    data.resize(oldsize + datatype->type_size());
    parse_scalar(node, &data[oldsize], datatype);
    return;
  }
  int64_t size = shape.at(rank - 1);
  assert(node.IsSequence());
  assert(node.size() == size);
  for (YAML::const_iterator ni = node.begin(), ne = node.end(); ni != ne; ++ni)
    parse_inline_array_nd(*ni, datatype, shape, rank - 1, data);
}

void parse_inline_array(const YAML::Node &node,
                        shared_ptr<generic_blob_t> &data,
                        const bool have_datatype,
                        shared_ptr<datatype_t> &datatype, const bool have_shape,
                        vector<int64_t> &shape) {
  if (!have_shape) {
    // determine shape
    shape.clear();
    YAML::Node n = node;
    while (n.IsSequence()) {
      shape.insert(shape.begin(), n.size());
      // This method does not work if the array size is zero in one dimension
      if (shape[0] == 0)
        break;
      n = n[0];
    }
    assert(n.IsScalar());
  }
  int64_t npoints = 1;
  for (size_t d = 0; d < shape.size(); ++d)
    npoints *= shape[d];
  vector<unsigned char> data1;
  if (!have_datatype) {
    // determine datatype while parsing
    try {
      datatype = make_shared<datatype_t>(id_int64);
      data1.reserve(npoints * datatype->type_size());
      parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
    } catch (YAML::RepresentationException) {
      try {
        datatype = make_shared<datatype_t>(id_float64);
        data1.reserve(npoints * datatype->type_size());
        parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
      } catch (YAML::RepresentationException) {
        try {
          datatype = make_shared<datatype_t>(id_complex128);
          data1.reserve(npoints * datatype->type_size());
          parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
        } catch (YAML::RepresentationException) {
          // bool8_t
          // ucs4_t
          assert(0);
        }
      }
    }
  } else {
    // parse data, expecting a particular datatype
    data1.reserve(npoints * datatype->type_size());
    parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
  }
  data = make_shared<blob_t<unsigned char>>(move(data1));
}

YAML::Node emit_inline_array(const unsigned char *data,
                             const shared_ptr<datatype_t> &datatype,
                             byteorder_t byteorder,
                             const vector<int64_t> &shape,
                             const vector<int64_t> &strides) {
  size_t rank = shape.size();
  assert(strides.size() == rank);
  if (rank == 0) {
    // 0-dimensional array
    YAML::Node node;
    node.SetStyle(YAML::EmitterStyle::Flow);
    // node = data.at(offset);
    node = emit_scalar(data, datatype, byteorder);
    return node;
  }
  if (rank == 1) {
    // 1-dimensional array
    YAML::Node node;
    // node.SetStyle(YAML::EmitterStyle::Flow);
    for (size_t i = 0; i < shape.at(0); ++i)
      node[i] = emit_scalar(data + i * strides.at(0), datatype, byteorder);
    return node;
  }
  // multi-dimensional array
  YAML::Node node;
  // TODO: Try emitting these as Flow, with a Newline at the end
  vector<int64_t> shape1(rank - 1);
  for (size_t d = 0; d < rank - 1; ++d)
    shape1.at(d) = shape.at(d + 1);
  vector<int64_t> strides1(rank - 1);
  for (size_t d = 0; d < rank - 1; ++d)
    strides1.at(d) = strides.at(d + 1);
  for (size_t i = 0; i < shape.at(0); ++i)
    node[i] = emit_inline_array(data + i * strides.at(0), datatype, byteorder,
                                shape1, strides1);
  return node;
}

// (Incidentally, this spells "SBLK", with the highest bit of the "S" set to
// one)
constexpr array<unsigned char, 4> block_magic_token{0xd3, 0x42, 0x4c, 0x4b};

template <typename T> void input(istream &is, T &data) {
  // Always input in big-endian as required for the header
  for (ptrdiff_t i = sizeof(T) - 1; i >= 0; --i) {
    unsigned char ch;
    is.read(reinterpret_cast<char *>(&ch), 1);
    reinterpret_cast<unsigned char *>(&data)[i] = ch;
  }
}

shared_ptr<generic_blob_t> read_block(istream &is) {
  // block_magic_token
  array<unsigned char, 4> token;
  for (auto &ch : token)
    input(is, ch);
  if (token != block_magic_token) {
    is.seekg(-int64_t(token.size()), ios_base::cur);
    return nullptr;
  }
  // header_size
  uint16_t header_size;
  input(is, header_size);
  auto header_prefix_end = is.tellg();
  // flags
  uint32_t flags;
  input(is, flags);
  assert(flags == 0);
  // compression
  array<unsigned char, 4> comp;
  for (auto &ch : comp)
    input(is, ch);
  // TODO: Remember compression
  compression_t compression;
  if ((comp == array<unsigned char, 4>{0, 0, 0, 0}))
    compression = compression_t::none;
  else if ((comp == array<unsigned char, 4>{'b', 'z', 'p', '2'}))
    compression = compression_t::bzip2;
  else if ((comp == array<unsigned char, 4>{'z', 'l', 'i', 'b'}))
    compression = compression_t::zlib;
  else
    assert(0);
  // allocated_space
  uint64_t allocated_space;
  input(is, allocated_space);
  // used_space
  uint64_t used_space;
  input(is, used_space);
  assert(used_space >= allocated_space);
  // data_space
  uint64_t data_space;
  input(is, data_space);
  // checksum
  array<unsigned char, 16> checksum;
  for (auto &ch : checksum)
    input(is, ch);
  // finish reading header
  auto header_end = is.tellg();
  int64_t header_read = header_end - header_prefix_end;
  assert(header_read <= header_size);
  if (header_read < header_size)
    is.seekg(header_size - header_read, ios_base::cur);
  // read data
  vector<unsigned char> indata(allocated_space);
  is.read(reinterpret_cast<char *>(indata.data()), indata.size());
  // TODO: check checksum
  // decompress data
  vector<unsigned char> data;
  switch (compression) {
  case compression_t::none:
    assert(data_space == allocated_space);
    data = move(indata);
    break;
#ifdef ASDF_HAVE_BZIP2
  case compression_t::bzip2: {
    data.resize(data_space);
    bz_stream strm;
    strm.bzalloc = NULL;
    strm.bzfree = NULL;
    strm.opaque = NULL;
    BZ2_bzDecompressInit(&strm, 0, 0);
    strm.next_in =
        reinterpret_cast<char *>(const_cast<unsigned char *>(indata.data()));
    strm.next_out = reinterpret_cast<char *>(data.data());
    uint64_t avail_in = indata.size();
    uint64_t avail_out = data.size();
    for (;;) {
      uint64_t this_avail_in =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_in);
      uint64_t this_avail_out =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_out);
      strm.avail_in = this_avail_in;
      strm.avail_out = this_avail_out;
      int iret = BZ2_bzDecompress(&strm);
      avail_in -= this_avail_in - strm.avail_in;
      avail_out -= this_avail_out - strm.avail_out;
      if (iret == BZ_STREAM_END)
        break;
      assert(iret == BZ_OK);
    }
    BZ2_bzDecompressEnd(&strm);
    assert(avail_in == 0);
    assert(avail_out == 0);
    break;
  }
#endif
#ifdef ASDF_HAVE_ZLIB
  case compression_t::zlib: {
    data.resize(data_space);
    z_stream strm;
    strm.zalloc = NULL;
    strm.zfree = NULL;
    strm.opaque = NULL;
    inflateInit(&strm);
    strm.next_in = const_cast<unsigned char *>(indata.data());
    strm.next_out = data.data();
    uint64_t avail_in = indata.size();
    uint64_t avail_out = data.size();
    for (;;) {
      uint64_t this_avail_in =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_in);
      uint64_t this_avail_out =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_out);
      strm.avail_in = this_avail_in;
      strm.avail_out = this_avail_out;
      int iret = inflate(&strm, Z_NO_FLUSH);
      avail_in -= this_avail_in - strm.avail_in;
      avail_out -= this_avail_out - strm.avail_out;
      if (iret == Z_STREAM_END)
        break;
      assert(iret == Z_OK);
    }
    inflateEnd(&strm);
    assert(avail_in == 0);
    assert(avail_out == 0);
    break;
  }
#endif
  default:
    assert(0);
  }

  // skip padding
  if (used_space > allocated_space)
    is.seekg(used_space - allocated_space, ios_base::cur);
  return make_shared<blob_t<unsigned char>>(move(data));
}

template <typename T>
void output(vector<unsigned char> &header, const T &data) {
  // Always output in big-endian as required for the header
  for (ptrdiff_t i = sizeof(T) - 1; i >= 0; --i)
    header.push_back(reinterpret_cast<const unsigned char *>(&data)[i]);
}

// TODO: stream the block (e.g. when compressing), then write the correct header
// later
void ndarray::write_block(ostream &os) const {
  vector<unsigned char> header;
  // block_magic_token
  for (auto ch : block_magic_token)
    output(header, ch);
  // header_size (not yet known)
  auto header_size_pos = header.size();
  uint16_t unknown_header_size = 0;
  output(header, unknown_header_size);
  auto header_prefix_length = header.size();
  // flags
  uint32_t flags = 0;
  output(header, flags);
  // compression
  array<unsigned char, 4> comp;
  shared_ptr<generic_blob_t> outdata;
  switch (compression) {
  case compression_t::none:
    comp = {0, 0, 0, 0};
    outdata = data;
    break;
  case compression_t::bzip2: {
#ifdef ASDF_HAVE_BZIP2
    comp = {'b', 'z', 'p', '2'};
    // Allocate 600 bytes plus 1% more
    outdata = make_shared<blob_t<unsigned char>>(vector<unsigned char>(
        600 + data->nbytes() + (data->nbytes() + 99) / 100));
    const int level = 9;
    bz_stream strm;
    strm.bzalloc = NULL;
    strm.bzfree = NULL;
    strm.opaque = NULL;
    BZ2_bzCompressInit(&strm, level, 0, 0);
    strm.next_in = reinterpret_cast<char *>(const_cast<void *>(data->ptr()));
    strm.next_out = reinterpret_cast<char *>(outdata->ptr());
    uint64_t avail_in = data->nbytes();
    uint64_t avail_out = outdata->nbytes();
    for (;;) {
      uint64_t this_avail_in =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_in);
      uint64_t this_avail_out =
          min(uint64_t(numeric_limits<unsigned int>::max()), avail_out);
      strm.avail_in = this_avail_in;
      strm.avail_out = this_avail_out;
      auto state = this_avail_in < avail_in ? BZ_RUN : BZ_FINISH;
      int iret = BZ2_bzCompress(&strm, state);
      avail_in -= this_avail_in - strm.avail_in;
      avail_out -= this_avail_out - strm.avail_out;
      if (iret == BZ_STREAM_END)
        break;
      assert(iret == BZ_RUN_OK);
    }
    assert(avail_in == 0);
    outdata->resize(outdata->nbytes() - avail_out);
    if (outdata->nbytes() >= data->nbytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = data;
    }
#else
    // Fall back to no compression if bzip2 is not available
    comp = {0, 0, 0, 0};
    outdata = data;
#endif
    break;
  }
  case compression_t::zlib: {
#ifdef ASDF_HAVE_ZLIB
    comp = {'z', 'l', 'i', 'b'};
    // Allocate 6 bytes plus 5 bytes per 16 kByte more
    outdata = make_shared<blob_t<unsigned char>>(vector<unsigned char>(
        (6 + data->nbytes() + (data->nbytes() + 16383) / 16384 * 5)));
    const int level = 9;
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int iret = deflateInit(&strm, level);
    assert(iret == Z_OK);
    strm.next_in =
        reinterpret_cast<unsigned char *>(const_cast<void *>(data->ptr()));
    strm.next_out = reinterpret_cast<unsigned char *>(outdata->ptr());
    uint64_t avail_in = data->nbytes();
    uint64_t avail_out = outdata->nbytes();
    for (;;) {
      uint64_t this_avail_in =
          min(uint64_t(numeric_limits<uInt>::max()), avail_in);
      uint64_t this_avail_out =
          min(uint64_t(numeric_limits<uInt>::max()), avail_out);
      strm.avail_in = this_avail_in;
      strm.avail_out = this_avail_out;
      auto state = this_avail_in < avail_in ? Z_NO_FLUSH : Z_FINISH;
      int iret = deflate(&strm, state);
      avail_in -= this_avail_in - strm.avail_in;
      avail_out -= this_avail_out - strm.avail_out;
      if (iret == Z_STREAM_END)
        break;
      assert(iret == Z_OK);
    }
    assert(avail_in == 0);
    outdata->resize(outdata->nbytes() - avail_out);
    if (outdata->nbytes() >= data->nbytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = data;
    }
#else
    // Fall back to no compression if zlib is not available
    comp = {0, 0, 0, 0};
    outdata = data;
#endif
    break;
  }
  default:
    assert(0);
  }
  for (auto ch : comp)
    output(header, ch);
  // allocated_space
  uint64_t allocated_space = outdata->nbytes();
  output(header, allocated_space);
  // used_space
  uint64_t used_space = allocated_space; // no padding
  output(header, used_space);
  // data_space
  uint64_t data_space = data->nbytes();
  output(header, data_space);
  // checksum
  array<unsigned char, 16> checksum;
#ifdef ASDF_HAVE_OPENSSL
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, outdata->ptr(), outdata->nbytes());
  MD5_Final(checksum.data(), &ctx);
#else
  checksum = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#endif
  for (auto ch : checksum)
    output(header, ch);
  // fill in header_size
  uint16_t header_size = header.size() - header_prefix_length;
  vector<unsigned char> header_size_buf;
  output(header_size_buf, header_size);
  for (size_t p = 0; p < header_size_buf.size(); ++p)
    header.at(header_size_pos + p) = header_size_buf.at(p);
  // write header
  os.write(reinterpret_cast<const char *>(header.data()), header.size());
  // write data
  os.write(reinterpret_cast<const char *>(outdata->ptr()), outdata->nbytes());
  // write padding
  vector<char> padding(allocated_space - used_space);
  os.write(padding.data(), padding.size());
}

ndarray::ndarray(const reader_state &rs, const YAML::Node &node)
    : block_format(block_format_t::undefined),
      compression(compression_t::undefined), byteorder(byteorder_t::undefined),
      offset(-1) {
  assert(node.Tag() == "tag:stsci.edu:asdf/core/ndarray-1.0.0");
  if (node["source"].IsDefined())
    block_format = block_format_t::block;
  else if (node["data"].IsDefined())
    block_format = block_format_t::inline_array;
  else
    assert(0);
  switch (block_format) {
  case block_format_t::block: {
    int64_t source;
    yaml_decode(node["source"], source);
    // TODO: This is just a default choice
    compression = compression_t::zlib;
    datatype = make_shared<datatype_t>(rs, node["datatype"]);
    yaml_decode(node["byteorder"], byteorder);
    yaml_decode(node["shape"], shape);
    if (node["offset"].IsDefined())
      yaml_decode(node["offset"], offset);
    else
      offset = 0;
    if (node["strides"].IsDefined()) {
      yaml_decode(node["strides"], strides);
    } else {
      int rank = shape.size();
      strides.resize(rank);
      int64_t str = datatype->type_size();
      for (int d = rank - 1; d >= 0; --d) {
        strides.at(d) = str;
        str *= shape.at(d);
      }
    }
    data = rs.get_block(source);
    break;
  }
  case block_format_t::inline_array: {
    // compression remains uninitialized
    bool have_datatype = node["datatype"].IsDefined();
    if (have_datatype)
      datatype = make_shared<datatype_t>(rs, node["datatype"]);
    byteorder = host_byteorder();
    bool have_shape = node["shape"].IsDefined();
    if (have_shape)
      yaml_decode(node["shape"], shape);
    parse_inline_array(node["data"], data, have_datatype, datatype, have_shape,
                       shape);
    offset = 0;
    int rank = shape.size();
    strides.resize(rank);
    int64_t str = datatype->type_size();
    for (int d = rank - 1; d >= 0; --d) {
      strides.at(d) = str;
      str *= shape.at(d);
    }
    break;
  }
  default:
    assert(0);
  }
}

ndarray::ndarray(const copy_state &cs, const ndarray &arr) : ndarray(arr) {
  if (cs.set_block_format)
    block_format = cs.block_format;
  if (cs.set_compression)
    compression = cs.compression;
}

writer &ndarray::to_yaml(writer &w) const {
  w << YAML::LocalTag("core/ndarray-1.0.0");
  w << YAML::BeginMap;
  if (block_format == block_format_t::block) {
    // source
    const auto &self = *this;
    uint64_t idx = w.add_task([=](ostream &os) { self.write_block(os); });
    w << YAML::Key << "source" << YAML::Value << idx;
  } else {
    // data
    w << YAML::Key << "data" << YAML::Value
      << emit_inline_array(static_cast<const unsigned char *>(data->ptr()) +
                               offset,
                           datatype, byteorder, shape, strides);
  }
  // mask
  assert(mask.empty());
  // datatype
  w << YAML::Key << "datatype" << YAML::Value << datatype->to_yaml(w);
  if (block_format == block_format_t::block) {
    // byteorder
    w << YAML::Key << "byteorder" << YAML::Value << yaml_encode(byteorder);
  }
  // shape
  w << YAML::Key << "shape" << YAML::Value << YAML::Flow << shape;
  if (block_format == block_format_t::block) {
    // offset
    w << YAML::Key << "offset" << YAML::Value << offset;
    // strides
    w << YAML::Key << "strides" << YAML::Value << YAML::Flow << strides;
  }
  w << YAML::EndMap;
  return w;
}

} // namespace ASDF
