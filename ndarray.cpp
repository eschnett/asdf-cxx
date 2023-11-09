#include "asdf_ndarray.hpp"

#include "asdf_config.hpp"
#include "asdf_stl.hpp"

#ifdef ASDF_HAVE_BLOSC
#include <blosc.h>
#endif

#ifdef ASDF_HAVE_BLOSC2
#include <blosc2.h>
#endif

#ifdef ASDF_HAVE_BZIP2
#include <bzlib.h>
#endif

#ifdef ASDF_HAVE_OPENSSL
#include <openssl/evp.h>
#endif

#ifdef ASDF_HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef ASDF_HAVE_LIBZSTD
#include <zstd.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace ASDF {

// Multi-dimensional array

typed_block_t<bool>::typed_block_t(const vector<bool> &data) {
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
  int64_t size = shape.at(shape.size() - rank);
  assert(node.IsSequence());
  assert(node.size() == size);
  for (YAML::const_iterator ni = node.begin(), ne = node.end(); ni != ne; ++ni)
    parse_inline_array_nd(*ni, datatype, shape, rank - 1, data);
}

void parse_inline_array(const YAML::Node &node, shared_ptr<block_t> &data,
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
  data = make_shared<typed_block_t<unsigned char>>(std::move(data1));
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
  static_assert(std::is_integral<T>::value, "");
  using U = typename std::make_unsigned<T>::type;
  data = 0;
  for (ptrdiff_t i = sizeof(T) - 1; i >= 0; --i) {
    unsigned char ch;
    is.read(reinterpret_cast<char *>(&ch), 1);
    data = (U(data) << 8) | ch;
  }
}

shared_ptr<block_t>
read_block_data(const shared_ptr<istream> &pis, streamoff block_begin,
                uint64_t allocated_space, uint64_t data_space,
                compression_t compression,
                const array<unsigned char, 16> &want_checksum) {
  istream &is = *pis;
  assert(is);
  is.seekg(block_begin);
  assert(is);
  vector<unsigned char> indata(allocated_space);
  is.read(reinterpret_cast<char *>(indata.data()), indata.size());
  assert(is);

  // check checksum
#ifdef ASDF_HAVE_OPENSSL
  if (want_checksum != array<unsigned char, 16>{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                0, 0, 0, 0, 0}) {
    array<unsigned char, 16> checksum;
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    assert(mdctx);
    int ires = EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
    assert(ires == 1);
    ires = EVP_DigestUpdate(mdctx, indata.data(), indata.size());
    assert(ires == 1);
    assert(EVP_MD_size(EVP_md5()) == checksum.size());
    unsigned int digest_size;
    ires = EVP_DigestFinal_ex(mdctx, checksum.data(), &digest_size);
    assert(digest_size == checksum.size());
    assert(ires == 1);
    EVP_MD_CTX_free(mdctx);
    assert(checksum == want_checksum);
  }
#endif

  // decompress data
  vector<unsigned char> data;
  switch (compression) {

  case compression_t::none:
    assert(data_space == allocated_space);
    data = std::move(indata);
    break;

#ifdef ASDF_HAVE_BLOSC
  case compression_t::blosc: {
    const int numinternalthreads = 1;
    data.resize(data_space);
    assert(data.size() <= size_t(INT_MAX));
    int dsize = blosc_decompress_ctx(indata.data(), data.data(), data.size(),
                                     numinternalthreads);
    assert(dsize > 0);
    assert(dsize == data.size());
    break;
  }
#endif

#ifdef ASDF_HAVE_BLOSC2
  case compression_t::blosc2: {
    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    // TODO: Don't copy the data
    blosc2_schunk *const schunk =
        blosc2_schunk_from_buffer(indata.data(), indata.size(), false);
    blosc2_schunk_avoid_cframe_free(schunk, true);
    data.resize(data_space);
    uint8_t *output_ptr = data.data();
    int64_t total_output_size = data.size();
    for (int chunk = 0; chunk < schunk->nchunks; ++chunk) {
      using std::min;
      const int output_size = blosc2_schunk_decompress_chunk(
          schunk, chunk, output_ptr,
          int(min(total_output_size, int64_t(INT_MAX))));
      assert(output_size > 0);
      assert(output_size <= total_output_size);
      output_ptr += output_size;
      total_output_size -= output_size;
    }
    blosc2_schunk_free(schunk);
    break;
  }
#endif

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

  return make_shared<typed_block_t<unsigned char>>(std::move(data));
}

std::tuple<memoized<block_t>, block_info_t>
ndarray::read_block(const shared_ptr<istream> &pis) {
  istream &is = *pis;
  // block_magic_token
  array<unsigned char, 4> token;
  for (auto &ch : token)
    input(is, ch);
  if (token != block_magic_token) {
    is.seekg(-int64_t(token.size()), ios_base::cur);
    return {};
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
  else if ((comp == array<unsigned char, 4>{'b', 'l', 's', 'c'}))
    compression = compression_t::blosc;
  else if ((comp == array<unsigned char, 4>{'b', 'l', 's', '2'}))
    compression = compression_t::blosc2;
  else if ((comp == array<unsigned char, 4>{'b', 'z', 'p', '2'}))
    compression = compression_t::bzip2;
  else if ((comp == array<unsigned char, 4>{'z', 'l', 'i', 'b'}))
    compression = compression_t::zlib;
  else if ((comp == array<unsigned char, 4>{'z', 's', 't', 'd'}))
    compression = compression_t::libzstd;
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
  auto block_begin = is.tellg();
  auto fdata = memoized<block_t>([=]() {
    return read_block_data(pis, block_begin, allocated_space, data_space,
                           compression, checksum);
  });
  // This would ensure synchronous reading, which might be useful for
  // debugging
  // fdata.fill_cache();

  // skip padding
  is.seekg(block_begin + streamoff(used_space));

  block_info_t block_info{
      token,       header_size,     header_read, flags,      comp,
      compression, allocated_space, used_space,  data_space, checksum,
  };

  return {fdata, block_info};
}

template <typename T>
void output(vector<unsigned char> &header, const T &data) {
  // Always output in big-endian as required for the header
  static_assert(std::is_integral<T>::value, "");
  using U = typename std::make_unsigned<T>::type;
  for (ptrdiff_t i = sizeof(T) - 1; i >= 0; --i)
    header.push_back((U(data) >> (8 * i)) & 0xff);
}

// TODO: stream the block (e.g. when compressing), then write the correct
// header later
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
  shared_ptr<block_t> outdata;

  // storage management
  const bool old_ready = get_data().ready();

  switch (compression) {

  case compression_t::none:
    comp = {0, 0, 0, 0};
    outdata = get_data().get();
    break;

  case compression_t::blosc: {
#ifdef ASDF_HAVE_BLOSC
    comp = {'b', 'l', 's', 'c'};
    const int level = compression_level;
    const int doshuffle = BLOSC_BITSHUFFLE;
    const size_t typesize = get_scalar_type_size(datatype->scalar_type_id);
    const char *const compressor = BLOSC_BLOSCLZ_COMPNAME;
    const int blocksize = 0;
    const int numinternalthreads = 1;

    assert(get_data()->nbytes() <= size_t(INT_MAX));

    // Allocate `BLOSC_MAX_OVERHEAD` more
    outdata = make_shared<typed_block_t<unsigned char>>(
        vector<unsigned char>(get_data()->nbytes() + BLOSC_MAX_OVERHEAD));
    int bytes_written =
        blosc_compress_ctx(level, doshuffle, typesize, get_data()->nbytes(),
                           get_data()->ptr(), outdata->ptr(), outdata->nbytes(),
                           compressor, blocksize, numinternalthreads);
    assert(bytes_written > 0);
    outdata->resize(bytes_written);
    if (outdata->nbytes() >= get_data()->nbytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = get_data().get();
    }
#else
    // Fall back to no compression if bzip2 is not available
    comp = {0, 0, 0, 0};
    outdata = get_data().get();
#endif
    break;
  }

  case compression_t::blosc2: {
#ifdef ASDF_HAVE_BLOSC2
    comp = {'b', 'l', 's', '2'};

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.compcode = BLOSC_BLOSCLZ;
    cparams.clevel = compression_level;
    cparams.typesize = get_scalar_type_size(datatype->scalar_type_id);
    cparams.nthreads = 1;
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;

    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;

    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    storage.contiguous = true;
    storage.cparams = &cparams;

    blosc2_schunk *const schunk = blosc2_schunk_new(&storage);

    const int64_t chunk_size = INT_MAX - BLOSC2_MAX_OVERHEAD;
    uint8_t *input_ptr = static_cast<uint8_t *>(get_data()->ptr());
    int64_t total_input_size = get_data()->nbytes();
    while (total_input_size > 0) {
      using std::min;
      const int input_size = min(total_input_size, chunk_size);
      const int nchunks =
          blosc2_schunk_append_buffer(schunk, input_ptr, input_size);
      assert(nchunks > 0);
      input_ptr += input_size;
      total_input_size -= input_size;
    }

    uint8_t *cframe;
    bool needs_free;
    const int64_t size = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);

    // TODO: Reuse `cframe`, at least of `needs_free== true`
    outdata =
        make_shared<typed_block_t<unsigned char>>(vector<unsigned char>(size));
    std::memcpy(outdata->ptr(), cframe, outdata->nbytes());

    blosc2_schunk_free(schunk);
    if (needs_free)
      std::free(cframe);

#else
    // Fall back to no compression if bzip2 is not available
    comp = {0, 0, 0, 0};
    outdata = get_data().get();
#endif
    break;
  }

  case compression_t::bzip2: {
#ifdef ASDF_HAVE_BZIP2
    comp = {'b', 'z', 'p', '2'};
    // Allocate 600 bytes plus 1% more
    outdata = make_shared<typed_block_t<unsigned char>>(vector<unsigned char>(
        600 + get_data()->nbytes() + (get_data()->nbytes() + 99) / 100));
    const int level = compression_level;
    bz_stream strm;
    strm.bzalloc = NULL;
    strm.bzfree = NULL;
    strm.opaque = NULL;
    BZ2_bzCompressInit(&strm, level, 0, 0);
    strm.next_in =
        reinterpret_cast<char *>(const_cast<void *>(get_data()->ptr()));
    strm.next_out = reinterpret_cast<char *>(outdata->ptr());
    uint64_t avail_in = get_data()->nbytes();
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
    if (outdata->nbytes() >= get_data()->nbytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = get_data().get();
    }
#else
    // Fall back to no compression if bzip2 is not available
    comp = {0, 0, 0, 0};
    outdata = get_data().get();
#endif
    break;
  }

  case compression_t::zlib: {
#ifdef ASDF_HAVE_ZLIB
    comp = {'z', 'l', 'i', 'b'};
    // Allocate 6 bytes plus 5 bytes per 16 kByte more
    outdata = make_shared<typed_block_t<unsigned char>>(
        vector<unsigned char>((6 + get_data()->nbytes() +
                               (get_data()->nbytes() + 16383) / 16384 * 5)));
    const int level = compression_level;
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int iret = deflateInit(&strm, level);
    assert(iret == Z_OK);
    strm.next_in = reinterpret_cast<unsigned char *>(
        const_cast<void *>(get_data()->ptr()));
    strm.next_out = reinterpret_cast<unsigned char *>(outdata->ptr());
    uint64_t avail_in = get_data()->nbytes();
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
    if (outdata->nbytes() >= get_data()->nbytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = get_data().get();
    }
#else
    // Fall back to no compression if zlib is not available
    comp = {0, 0, 0, 0};
    outdata = get_data().get();
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
  uint64_t data_space = get_data()->nbytes();
  output(header, data_space);

  // checksum
  array<unsigned char, 16> checksum;
#ifdef ASDF_HAVE_OPENSSL
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  assert(mdctx);
  int ires = EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
  assert(ires == 1);
  ires = EVP_DigestUpdate(mdctx, outdata->ptr(), outdata->nbytes());
  assert(ires == 1);
  assert(EVP_MD_size(EVP_md5()) == checksum.size());
  unsigned int digest_size;
  ires = EVP_DigestFinal_ex(mdctx, checksum.data(), &digest_size);
  assert(digest_size == checksum.size());
  assert(ires == 1);
  EVP_MD_CTX_free(mdctx);
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

  // storage management
  if (!old_ready)
    get_data().forget();

  // write padding
  vector<char> padding(allocated_space - used_space);
  os.write(padding.data(), padding.size());
}

ndarray::ndarray(const shared_ptr<reader_state> &rs, const YAML::Node &node)
    : block_format(block_format_t::undefined),
      compression(compression_t::undefined), compression_level(-1),
      byteorder(byteorder_t::undefined), offset(-1) {
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
    compression_level = 9;
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
    mdata = rs->get_block(source);
    block_info = std::make_optional<block_info_t>(rs->get_block_info(source));
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
    shared_ptr<block_t> data;
    parse_inline_array(node["data"], data, have_datatype, datatype, have_shape,
                       shape);
    mdata = memoized<block_t>([=]() { return data; });
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
  if (cs.set_compression_level)
    compression_level = cs.compression_level;
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
      << emit_inline_array(
             static_cast<const unsigned char *>(get_data()->ptr()) + offset,
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

void ndarray::check_shape() const {
  int rank = shape.size();
  int64_t npoints = 1;
  for (int d = 0; d < rank; ++d)
    npoints *= shape[d];
  assert(mdata->nbytes() == npoints * datatype->type_size());
}

} // namespace ASDF
