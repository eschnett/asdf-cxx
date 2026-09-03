#include <asdf/ndarray.hxx>

#include <asdf/config.hxx>
#include <asdf/stl.hxx>

#ifdef ASDF_HAVE_BLOSC
#include <blosc.h>
#endif

#ifdef ASDF_HAVE_BLOSC2
#include <blosc2.h>
#endif

#ifdef ASDF_HAVE_BZIP2
#include <bzlib.h>
#endif

#ifdef ASDF_HAVE_LIBLZ4
#include <lz4.h>
#include <lz4frame.h>
#include <lz4hc.h>
#endif

#ifdef ASDF_HAVE_LIBZSTD
#include <zstd.h>
#endif

#ifdef ASDF_HAVE_OPENSSL
#include <openssl/evp.h>
#endif

#ifdef ASDF_HAVE_ZLIB
#include <zlib.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <type_traits>

namespace ASDF {

namespace {
std::string compression_name(compression_t compression) {
  std::ostringstream buf;
  buf << compression;
  return buf.str();
}
} // namespace

#ifdef ASDF_HAVE_OPENSSL
namespace {
array<unsigned char, 16> md5(const void *data, size_t nbytes) {
  array<unsigned char, 16> checksum;
  EVP_MD_CTX *const mdctx = EVP_MD_CTX_new();
  ASDF_CHECK(mdctx, "OpenSSL: EVP_MD_CTX_new failed");
  int ires = EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
  ASDF_CHECK(ires == 1, "OpenSSL: MD5 digest failed");
  ires = EVP_DigestUpdate(mdctx, data, nbytes);
  ASDF_CHECK(ires == 1, "OpenSSL: MD5 digest failed");
  assert(static_cast<size_t>(EVP_MD_size(EVP_md5())) == checksum.size());
  unsigned int digest_size;
  ires = EVP_DigestFinal_ex(mdctx, checksum.data(), &digest_size);
  assert(digest_size == checksum.size());
  ASDF_CHECK(ires == 1, "OpenSSL: MD5 digest failed");
  EVP_MD_CTX_free(mdctx);
  return checksum;
}
} // namespace
#endif

#ifdef ASDF_HAVE_BLOSC2
namespace {
// blosc2 must be initialised once per process before its schunk API is used
void ensure_blosc2_initialized() {
  static const bool initialized = [] {
    blosc2_init();
    return true;
  }();
  (void)initialized;
}
} // namespace
#endif

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
  assert(shape.size() >= static_cast<size_t>(rank));
  if (rank == 0) {
    // A scalar element is a YAML scalar, a structured element is a YAML
    // sequence with one entry per field
    ASDF_CHECK(
        datatype->is_scalar ? node.IsScalar() : node.IsSequence(),
        "Inline array element has the wrong YAML node type for its datatype");
    size_t oldsize = data.size();
    data.resize(oldsize + datatype->type_size());
    parse_scalar(node, &data[oldsize], datatype);
    return;
  }
  int64_t size = shape.at(shape.size() - rank);
  ASDF_CHECK(node.IsSequence(),
             "Inline array data must be nested YAML sequences");
  ASDF_CHECK(node.size() == static_cast<size_t>(size),
             "Inline array data does not match the declared shape");
  for (YAML::const_iterator ni = node.begin(), ne = node.end(); ni != ne; ++ni)
    parse_inline_array_nd(*ni, datatype, shape, rank - 1, data);
}

namespace {
// The nesting depth of a single element in the inline representation: a
// scalar element is not nested, a structured element is a sequence with one
// entry per field, and sub-array fields add further nesting.
size_t element_nesting(const shared_ptr<datatype_t> &datatype) {
  if (datatype->is_scalar)
    return 0;
  ASDF_CHECK(!datatype->fields.empty(), "Structured datatype has no fields");
  const auto &field = datatype->fields.front();
  return 1 + field->shape.size() + element_nesting(field->datatype);
}
} // namespace

void parse_inline_array(const YAML::Node &node, shared_ptr<block_t> &data,
                        const bool have_datatype,
                        shared_ptr<datatype_t> &datatype, const bool have_shape,
                        vector<int64_t> &shape) {
  if (!have_shape) {
    // determine shape
    shape.clear();
    YAML::Node n = node;
    while (n.IsSequence()) {
      shape.push_back(n.size());
      // This method does not work if the array size is zero in one dimension
      if (shape.back() == 0)
        break;
      // Note: `n = n[0]` would modify the tree instead of rebinding `n`
      n.reset(n[0]);
    }
    ASDF_CHECK(n.IsScalar(), "Cannot infer the shape of an inline array: the "
                             "innermost element is not a scalar");
    // The nesting of a structured element is not part of the array shape
    const size_t nesting = have_datatype ? element_nesting(datatype) : 0;
    ASDF_CHECK(shape.size() >= nesting,
               "Inline array data is nested less deeply than its structured "
               "datatype requires");
    shape.erase(shape.end() - nesting, shape.end());
  }
  int64_t npoints = 1;
  for (size_t d = 0; d < shape.size(); ++d)
    npoints *= shape[d];
  vector<unsigned char> data1;
  if (!have_datatype) {
    // determine datatype while parsing
    try {
      datatype = make_shared<datatype_t>(id_int64);
      data1.clear();
      data1.reserve(npoints * datatype->type_size());
      parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
    } catch (const YAML::RepresentationException &) {
      try {
        datatype = make_shared<datatype_t>(id_float64);
        data1.clear();
        data1.reserve(npoints * datatype->type_size());
        parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
      } catch (const YAML::RepresentationException &) {
        try {
          datatype = make_shared<datatype_t>(id_complex128);
          data1.clear();
          data1.reserve(npoints * datatype->type_size());
          parse_inline_array_nd(node, datatype, shape, shape.size(), data1);
        } catch (const YAML::RepresentationException &) {
          // bool8_t
          // ucs4_t
          ASDF_ERROR("Cannot infer the datatype of an inline array; only "
                     "int64, float64, and complex128 are inferred");
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
    for (size_t i = 0; i < static_cast<size_t>(shape.at(0)); ++i)
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
  for (size_t i = 0; i < static_cast<size_t>(shape.at(0)); ++i)
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
                uint64_t used_space, uint64_t data_space,
                compression_t compression,
                const array<unsigned char, 16> &want_checksum) {
  istream &is = *pis;
  ASDF_CHECK(is, "Input stream is in a failed state");
  is.seekg(block_begin);
  ASDF_CHECK(is, "Cannot seek to the block data");
  // The payload occupies the first `used_space` bytes of the block; the
  // remaining `allocated_space - used_space` bytes are padding
  vector<unsigned char> indata(used_space);
  is.read(reinterpret_cast<char *>(indata.data()), indata.size());
  ASDF_CHECK(is, "Unexpected end of file while reading block data");

  // Check the checksum against the stored (compressed) bytes. This is what
  // the Python reference implementation writes and verifies. Files written by
  // much older versions of it -- among them asdf-standard's `compressed.asdf`
  // reference files -- instead checksum the uncompressed data, so a mismatch
  // here is re-checked after decompressing rather than reported right away.
  bool checksum_matched = true;
#ifdef ASDF_HAVE_OPENSSL
  const bool have_checksum =
      want_checksum !=
      array<unsigned char, 16>{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  if (have_checksum)
    checksum_matched = md5(indata.data(), indata.size()) == want_checksum;
#endif

  // decompress data
  vector<unsigned char> data;
  switch (compression) {

  case compression_t::none:
    ASDF_CHECK(data_space == used_space,
               "Uncompressed block: data_size differs from used_size");
    data = std::move(indata);
    break;

#ifdef ASDF_HAVE_BLOSC
  case compression_t::blosc: {
    const int numinternalthreads = 1;
    data.resize(data_space);
    ASDF_CHECK(data.size() <= size_t(INT_MAX),
               "blosc blocks are limited to 2 GiB");
    int dsize = blosc_decompress_ctx(indata.data(), data.data(), data.size(),
                                     numinternalthreads);
    ASDF_CHECK(dsize > 0 && size_t(dsize) == data.size(),
               "blosc decompression failed");
    break;
  }
#endif

#ifdef ASDF_HAVE_BLOSC2
  case compression_t::blosc2: {
    ensure_blosc2_initialized();
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
      ASDF_CHECK(output_size > 0 && output_size <= total_output_size,
                 "blosc2 decompression failed");
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
      ASDF_CHECK(iret == BZ_OK, "bzip2 decompression failed with error " +
                                    std::to_string(iret));
    }
    BZ2_bzDecompressEnd(&strm);
    ASDF_CHECK(avail_in == 0 && avail_out == 0,
               "bzip2: decompressed size does not match the block header");
    break;
  }
#endif

#ifdef ASDF_HAVE_LIBLZ4
  case compression_t::lz4: {
    // ASDF standard lz4 encoding, as written by the Python reference
    // implementation: a sequence of chunks, each a 4-byte big-endian length
    // followed by that many bytes of LZ4 block-format data, which in turn
    // begin with the 4-byte little-endian uncompressed chunk size
    data.resize(data_space);
    size_t inpos = 0;
    size_t outpos = 0;
    while (inpos < indata.size()) {
      ASDF_CHECK(inpos + 4 <= indata.size(), "lz4: truncated chunk header");
      uint32_t chunk_size = 0;
      for (int i = 0; i < 4; ++i)
        chunk_size = (chunk_size << 8) | indata[inpos + i];
      inpos += 4;
      ASDF_CHECK(chunk_size >= 4, "lz4: invalid chunk size");
      ASDF_CHECK(inpos + chunk_size <= indata.size(),
                 "lz4: chunk extends past the end of the block");
      uint32_t uncompressed_size = 0;
      for (int i = 3; i >= 0; --i)
        uncompressed_size = (uncompressed_size << 8) | indata[inpos + i];
      ASDF_CHECK(outpos + uncompressed_size <= data.size(),
                 "lz4: decompressed size exceeds data_size");
      const int nbytes = LZ4_decompress_safe(
          reinterpret_cast<const char *>(indata.data() + inpos + 4),
          reinterpret_cast<char *>(data.data() + outpos), int(chunk_size - 4),
          int(uncompressed_size));
      ASDF_CHECK(nbytes >= 0 && uint32_t(nbytes) == uncompressed_size,
                 "lz4 decompression failed");
      inpos += chunk_size;
      outpos += uncompressed_size;
    }
    ASDF_CHECK(outpos == data.size(),
               "lz4: decompressed size does not match the block header");
    break;
  }

  case compression_t::lz4f: {
    data.resize(data_space);

    LZ4F_decompressOptions_t dOpt;
    std::memset(&dOpt, 0, sizeof dOpt);
    dOpt.stableDst = true;
#if LZ4_VERSION_NUMBER >= 10904
#ifdef ASDF_HAVE_OPENSSL
    dOpt.skipChecksums = true; // this is faster, and we have our own checksum
#endif
#endif

    LZ4F_dctx *dctx;
    LZ4F_errorCode_t ierr =
        LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
    ASDF_CHECK(!LZ4F_isError(ierr),
               "lz4f: cannot create decompression context");
    assert(dctx);

    size_t dstSize = data.size();
    size_t srcSize = indata.size();
    const std::size_t nbytes_expected = LZ4F_decompress(
        dctx, data.data(), &dstSize, indata.data(), &srcSize, &dOpt);
    ASDF_CHECK(nbytes_expected == 0,
               "lz4f decompression failed or the frame is incomplete");

    ierr = LZ4F_freeDecompressionContext(dctx);
    ASDF_CHECK(!LZ4F_isError(ierr), "lz4f: cannot free decompression context");
    break;
  }
#endif

#ifdef ASDF_HAVE_LIBZSTD
  case compression_t::libzstd: {
    data.resize(data_space);
    // The uncompressed size is known, so a single-shot decompression suffices
    const size_t ret =
        ZSTD_decompress(data.data(), data.size(), indata.data(), indata.size());
    ASDF_CHECK(!ZSTD_isError(ret), std::string("zstd decompression failed: ") +
                                       ZSTD_getErrorName(ret));
    ASDF_CHECK(ret == data_space,
               "zstd: decompressed size does not match the block header");
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
      ASDF_CHECK(iret == Z_OK, "zlib decompression failed with error " +
                                   std::to_string(iret));
    }
    inflateEnd(&strm);
    ASDF_CHECK(avail_in == 0 && avail_out == 0,
               "zlib: decompressed size does not match the block header");
    break;
  }
#endif

  default:
    ASDF_ERROR("Block uses compression \"" + compression_name(compression) +
               "\", which is not available in this build");
  }

#ifdef ASDF_HAVE_OPENSSL
  // For an uncompressed block the stored bytes *are* the data, so the second
  // domain cannot differ from the first
  if (have_checksum && !checksum_matched && compression != compression_t::none)
    checksum_matched = md5(data.data(), data.size()) == want_checksum;
#endif
  ASDF_CHECK(checksum_matched,
             "Block checksum mismatch: the block data are corrupted");

  return make_shared<typed_block_t<unsigned char>>(std::move(data));
}

std::tuple<memoized<block_t>, block_info_t>
ndarray::read_block(const shared_ptr<istream> &pis) {
  istream &is = *pis;
  // block_magic_token
  // Other writers may pad between the YAML tree and the first block, so scan
  // forward until the magic token is found. Stop at the end of the file or at
  // the block index ("#ASDF BLOCK INDEX"), leaving the stream in a good state
  // and positioned at whatever was found.
  array<unsigned char, 4> token;
  for (;;) {
    const auto pos = is.tellg();
    is.read(reinterpret_cast<char *>(token.data()), token.size());
    if (!is) {
      // End of file: no more blocks
      is.clear();
      is.seekg(pos);
      return {};
    }
    if (token == block_magic_token)
      break;
    if (token[0] == '#') {
      // Block index
      is.seekg(pos);
      return {};
    }
    // Padding: skip one byte and try again
    is.seekg(pos + streamoff(1));
  }
  // header_size
  uint16_t header_size;
  input(is, header_size);
  auto header_prefix_end = is.tellg();
  // flags
  uint32_t flags;
  input(is, flags);
  ASDF_CHECK(flags == 0, "Unsupported block flags " + std::to_string(flags) +
                             " (streamed blocks are not supported)");
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
  else if ((comp == array<unsigned char, 4>{'l', 'z', '4', 0}))
    compression = compression_t::lz4;
  else if ((comp == array<unsigned char, 4>{'l', 'z', '4', 'f'}))
    compression = compression_t::lz4f;
  else if ((comp == array<unsigned char, 4>{'z', 's', 't', 'd'}))
    compression = compression_t::libzstd;
  else if ((comp == array<unsigned char, 4>{'z', 'l', 'i', 'b'}))
    compression = compression_t::zlib;
  else {
    std::string token;
    for (const auto ch : comp)
      token += std::isprint(ch) ? char(ch) : '?';
    ASDF_ERROR("Unknown block compression token \"" + token + "\"");
  }
  // allocated_space
  uint64_t allocated_space;
  input(is, allocated_space);
  // used_space
  uint64_t used_space;
  input(is, used_space);
  ASDF_CHECK(used_space <= allocated_space,
             "Block header: used_size exceeds allocated_size");
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
  ASDF_CHECK(header_read <= header_size,
             "Block header is shorter than expected");
  if (header_read < header_size)
    is.seekg(header_size - header_read, ios_base::cur);
  // read data
  auto block_begin = is.tellg();
  auto fdata = memoized<block_t>([=]() {
    return read_block_data(pis, block_begin, used_space, data_space,
                           compression, checksum);
  });
  // This would ensure synchronous reading, which might be useful for
  // debugging
  // fdata.fill_cache();

  // skip the block, including its padding
  is.seekg(block_begin + streamoff(allocated_space));

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

#ifdef ASDF_HAVE_BLOSC
  case compression_t::blosc: {
    comp = {'b', 'l', 's', 'c'};
    const int level = compression_level;
    const int doshuffle = BLOSC_BITSHUFFLE;
    // The shuffle filter works on fixed-size items; a structured datatype has
    // no scalar type id, so use the whole element size
    const size_t typesize =
        min(datatype->type_size(), size_t(BLOSC_MAX_TYPESIZE));
    const char *const compressor = BLOSC_BLOSCLZ_COMPNAME;
    const int blocksize = 0;
    const int numinternalthreads = 1;

    ASDF_CHECK(get_data()->nbytes() <= size_t(INT_MAX),
               "blosc blocks are limited to 2 GiB");

    // Allocate `BLOSC_MAX_OVERHEAD` more
    outdata = make_shared<typed_block_t<unsigned char>>(
        vector<unsigned char>(get_data()->nbytes() + BLOSC_MAX_OVERHEAD));
    int bytes_written =
        blosc_compress_ctx(level, doshuffle, typesize, get_data()->nbytes(),
                           get_data()->ptr(), outdata->ptr(), outdata->nbytes(),
                           compressor, blocksize, numinternalthreads);
    ASDF_CHECK(bytes_written > 0, "blosc compression failed");
    outdata->resize(bytes_written);
    if (outdata->nbytes() >= get_data()->nbytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = get_data().get();
    }
    break;
  }
#endif

#ifdef ASDF_HAVE_BLOSC2
  case compression_t::blosc2: {
    comp = {'b', 'l', 's', '2'};
    ensure_blosc2_initialized();

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.compcode = BLOSC_BLOSCLZ;
    cparams.clevel = compression_level;
    cparams.typesize = min(datatype->type_size(), size_t(BLOSC_MAX_TYPESIZE));
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
      ASDF_CHECK(nchunks > 0, "blosc2 compression failed");
      input_ptr += input_size;
      total_input_size -= input_size;
    }

    uint8_t *cframe;
    bool needs_free;
    const int64_t size = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);

    // TODO: Reuse `cframe`, at least if `needs_free== true`
    outdata =
        make_shared<typed_block_t<unsigned char>>(vector<unsigned char>(size));
    std::memcpy(outdata->ptr(), cframe, outdata->nbytes());

    blosc2_schunk_free(schunk);
    if (needs_free)
      std::free(cframe);

    break;
  }
#endif

#ifdef ASDF_HAVE_BZIP2
  case compression_t::bzip2: {
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
      ASDF_CHECK(iret == BZ_RUN_OK,
                 "bzip2 compression failed with error " + std::to_string(iret));
    }
    ASDF_CHECK(avail_in == 0, "bzip2 compression did not consume all input");
    outdata->resize(outdata->nbytes() - avail_out);
    if (outdata->nbytes() >= get_data()->nbytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = get_data().get();
    }
    break;
  }
#endif

#ifdef ASDF_HAVE_LIBLZ4
  case compression_t::lz4: {
    comp = {'l', 'z', '4', 0};
    // ASDF standard lz4 encoding (see the decoder for the layout). Use the
    // same chunk size as the Python reference implementation.
    const size_t chunk_size = size_t(1) << 22;
    // LZ4HC treats levels below 1 as its default level
    const int level = compression_level < 1
                          ? LZ4HC_CLEVEL_DEFAULT
                          : min(LZ4HC_CLEVEL_MAX, compression_level);
    const char *const src = static_cast<const char *>(get_data()->ptr());
    const size_t src_size = get_data()->nbytes();
    vector<unsigned char> out;
    vector<char> buf(LZ4_compressBound(int(min(chunk_size, src_size))));
    for (size_t pos = 0; pos < src_size; pos += chunk_size) {
      const size_t n = min(chunk_size, src_size - pos);
      const int nbytes = LZ4_compress_HC(src + pos, buf.data(), int(n),
                                         int(buf.size()), level);
      ASDF_CHECK(nbytes > 0, "lz4 compression failed");
      // big-endian length of the chunk, including the size prefix below
      const uint32_t chunk_len = uint32_t(nbytes) + 4;
      for (int i = 3; i >= 0; --i)
        out.push_back((chunk_len >> (8 * i)) & 0xff);
      // little-endian uncompressed size
      for (int i = 0; i < 4; ++i)
        out.push_back((uint32_t(n) >> (8 * i)) & 0xff);
      out.insert(out.end(), buf.data(), buf.data() + nbytes);
    }
    outdata = make_shared<typed_block_t<unsigned char>>(std::move(out));
    if (outdata->nbytes() >= get_data()->nbytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = get_data().get();
    }
    break;
  }

  case compression_t::lz4f: {
    comp = {'l', 'z', '4', 'f'};

    LZ4F_preferences_t preferences = LZ4F_INIT_PREFERENCES;
    preferences.compressionLevel = compression_level;

    const size_t max_nbytes =
        LZ4F_compressFrameBound(get_data()->nbytes(), &preferences);
    outdata = make_shared<typed_block_t<unsigned char>>(
        vector<unsigned char>(max_nbytes));

    const size_t nbytes =
        LZ4F_compressFrame(outdata->ptr(), outdata->nbytes(), get_data()->ptr(),
                           get_data()->nbytes(), &preferences);
    outdata->resize(nbytes);
    break;
  }
#endif

#ifdef ASDF_HAVE_LIBZSTD
  case compression_t::libzstd: {
    comp = {'z', 's', 't', 'd'};

    // A level of zero means "use the default level"
    int level =
        compression_level == 0 ? ZSTD_CLEVEL_DEFAULT : compression_level;
    level = max(ZSTD_minCLevel(), min(ZSTD_maxCLevel(), level));

    const size_t max_nbytes = ZSTD_compressBound(get_data()->nbytes());
    outdata = make_shared<typed_block_t<unsigned char>>(
        vector<unsigned char>(max_nbytes));

    const size_t nbytes =
        ZSTD_compress(outdata->ptr(), outdata->nbytes(), get_data()->ptr(),
                      get_data()->nbytes(), level);
    ASDF_CHECK(!ZSTD_isError(nbytes), std::string("zstd compression failed: ") +
                                          ZSTD_getErrorName(nbytes));
    outdata->resize(nbytes);
    if (outdata->nbytes() >= get_data()->nbytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = get_data().get();
    }
    break;
  }
#endif

#ifdef ASDF_HAVE_ZLIB
  case compression_t::zlib: {
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
    ASDF_CHECK(iret == Z_OK, "zlib: deflateInit failed");
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
      ASDF_CHECK(iret == Z_OK,
                 "zlib compression failed with error " + std::to_string(iret));
    }
    ASDF_CHECK(avail_in == 0, "zlib compression did not consume all input");
    outdata->resize(outdata->nbytes() - avail_out);
    if (outdata->nbytes() >= get_data()->nbytes()) {
      // Skip compression if it does not reduce the size
      comp = {0, 0, 0, 0};
      outdata = get_data().get();
    }
    break;
  }
#endif

  default:
    ASDF_ERROR("Cannot write a block with compression \"" +
               compression_name(compression) +
               "\": not available in this build");
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
  ASDF_CHECK(mdctx, "OpenSSL: EVP_MD_CTX_new failed");
  int ires = EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
  ASDF_CHECK(ires == 1, "OpenSSL: MD5 digest failed");
  ires = EVP_DigestUpdate(mdctx, outdata->ptr(), outdata->nbytes());
  ASDF_CHECK(ires == 1, "OpenSSL: MD5 digest failed");
  assert(EVP_MD_size(EVP_md5()) == checksum.size());
  unsigned int digest_size;
  ires = EVP_DigestFinal_ex(mdctx, checksum.data(), &digest_size);
  assert(digest_size == checksum.size());
  ASDF_CHECK(ires == 1, "OpenSSL: MD5 digest failed");
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
  ASDF_CHECK(classify_core_tag(node.Tag()) == core_tag_t::ndarray,
             "Expected tag core/ndarray-1.0.0 or -1.1.0, found \"" +
                 node.Tag() + "\"");
  if (node["source"].IsDefined())
    block_format = block_format_t::block;
  else if (node["data"].IsDefined())
    block_format = block_format_t::inline_array;
  else
    ASDF_ERROR("An ndarray must have either \"source\" or \"data\"");

  switch (block_format) {

  case block_format_t::block: {
    int64_t source;
    yaml_decode(node["source"], source);
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
      // Rejects negative extents and a shape whose size overflows, so that
      // the strides below cannot overflow either
      packed_nbytes();
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
    // Remember the block's compressor so that a copy preserves it. The
    // original compression level is not recorded in the file.
    compression = block_info->compression;
    compression_level = compression == compression_t::none ? 0 : 9;
    // The block header knows the uncompressed size, so this does not load
    // the block data
    check_bounds(block_info->data_space);
    break;
  }

  case block_format_t::inline_array: {
    // Inline arrays are not compressed. Use "none" so that converting to
    // block format produces a valid (uncompressed) block.
    compression = compression_t::none;
    compression_level = 0;
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
    packed_nbytes();
    int rank = shape.size();
    strides.resize(rank);
    int64_t str = datatype->type_size();
    for (int d = rank - 1; d >= 0; --d) {
      strides.at(d) = str;
      str *= shape.at(d);
    }
    check_bounds(mdata->nbytes());
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

namespace {

// Walk a datatype, including the fields of a structured one, and record what
// the standard versions have to say about the scalar types it uses
void collect_datatype_requirements(content_requirements &req,
                                   const string &path,
                                   const datatype_t &datatype) {
  if (!datatype.is_scalar) {
    for (const auto &field : datatype.fields)
      collect_datatype_requirements(req, path + "/" + field->name,
                                    *field->datatype);
    return;
  }
  switch (datatype.scalar_type_id) {
  case id_float16:
    // A legitimate feature of standard 1.6.0, never nonstandard
    req.needs_float16 = true;
    break;
  case id_complex32:
    // No version of the standard has `complex32`, but it consists of
    // `float16` components, so writing it also needs 1.6.0
    req.needs_float16 = true;
    req.nonstandard.push_back(path + ": complex32 datatype");
    break;
  case id_int128:
    req.nonstandard.push_back(path + ": int128 datatype");
    break;
  case id_uint128:
    req.nonstandard.push_back(path + ": uint128 datatype");
    break;
  default:
    break;
  }
}

} // namespace

void ndarray::collect_requirements(content_requirements &req,
                                   const string &path) const {
  collect_datatype_requirements(req, path, *datatype);
  // A rank-0 array is fine as a block -- the schema puts no lower bound on
  // `shape`, and Python asdf writes `np.array(5.0)` that way -- but it has no
  // inline representation, because `data` has to be a list
  if (shape.empty() && block_format == block_format_t::inline_array)
    req.nonstandard.push_back(path + ": inline rank-0 array");
}

writer &ndarray::to_yaml(writer &w) const {
  // `asdf::write` has already checked the whole tree, but an `asdf` built
  // from raw `nodes` or `writers` cannot be walked, so repeat the check here
  if (!w.allow_nonstandard()) {
    content_requirements req;
    collect_requirements(req, "");
    ASDF_CHECK(req.nonstandard.empty(),
               "Cannot write an array with nonstandard content (" +
                   req.nonstandard.front() + ")");
    ASDF_CHECK(!req.needs_float16 || w.standard().has_float16,
               "This array requires ASDF standard version " +
                   req.minimum_version().str() + ", but standard version " +
                   w.standard().version.str() + " is being written");
  }
  w << YAML::LocalTag(w.standard().ndarray_tag);
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
  ASDF_CHECK(mask.empty(), "Writing masked arrays is not supported");
  // datatype
  w << YAML::Key << "datatype" << YAML::Value << datatype->to_yaml(w);
  if (block_format == block_format_t::block) {
    // byteorder
    w << YAML::Key << "byteorder" << YAML::Value << yaml_encode(byteorder);
  }
  // shape
  w << YAML::Key << "shape" << YAML::Value << YAML::Flow << shape;
  if (block_format == block_format_t::block) {
    // offset and strides: both have defaults, and the standard's examples
    // leave them out where they hold
    if (offset != 0)
      w << YAML::Key << "offset" << YAML::Value << offset;
    if (!is_c_contiguous())
      w << YAML::Key << "strides" << YAML::Value << YAML::Flow << strides;
  }
  w << YAML::EndMap;
  return w;
}

namespace {

// Checked signed 64-bit arithmetic. A file can claim any shape, offset and
// strides it likes, so the bounds check must not overflow before it can
// reject them.
bool add_overflows(int64_t a, int64_t b, int64_t &result) {
  if (b > 0 && a > numeric_limits<int64_t>::max() - b)
    return true;
  if (b < 0 && a < numeric_limits<int64_t>::min() - b)
    return true;
  result = a + b;
  return false;
}

bool mul_overflows(int64_t a, int64_t b, int64_t &result) {
  const bool negative = (a < 0) != (b < 0);
  // |a| and |b|, computed without overflowing at INT64_MIN
  const uint64_t ua = a < 0 ? uint64_t(-(a + 1)) + 1 : uint64_t(a);
  const uint64_t ub = b < 0 ? uint64_t(-(b + 1)) + 1 : uint64_t(b);
  if (ua != 0 && ub > numeric_limits<uint64_t>::max() / ua)
    return true;
  const uint64_t product = ua * ub;
  if (product == 0) {
    result = 0;
    return false;
  }
  const uint64_t limit = uint64_t(numeric_limits<int64_t>::max()) + negative;
  if (product > limit)
    return true;
  // `-int64_t(product)` would overflow at INT64_MIN
  result = negative ? -int64_t(product - 1) - 1 : int64_t(product);
  return false;
}

string describe_extents(const vector<int64_t> &values) {
  ostringstream buf;
  buf << "[";
  for (size_t d = 0; d < values.size(); ++d)
    buf << (d == 0 ? "" : ", ") << values.at(d);
  buf << "]";
  return buf.str();
}

} // namespace

void ndarray::check_bounds(uint64_t nbytes) const {
  if (num_elements() == 0)
    return;
  const int rank = shape.size();
  const int64_t elemsize = datatype->type_size();
  // A datatype of zero size -- an empty field list, or `[ascii, 0]` -- would
  // make every element occupy no bytes, so the bounds below hold for any
  // block and the extraction loop would index past the end of its own result.
  // An array with no elements is fine and has already returned above.
  ASDF_CHECK(elemsize > 0,
             "Array datatype has zero size: an array with elements needs a "
             "datatype of at least one byte");
  // The lowest and the highest byte offset any element occupies. A negative
  // stride runs downwards from `offset`, a positive one upwards.
  int64_t lo = offset, hi = offset;
  bool ok = true;
  for (int d = 0; d < rank && ok; ++d) {
    int64_t extent;
    ok = !mul_overflows(strides.at(d), shape.at(d) - 1, extent);
    if (ok)
      ok = extent < 0 ? !add_overflows(lo, extent, lo)
                      : !add_overflows(hi, extent, hi);
  }
  int64_t hi_end = 0;
  if (ok)
    ok = !add_overflows(hi, elemsize, hi_end);
  if (ok)
    ok = lo >= 0 && hi_end >= 0 && uint64_t(hi_end) <= nbytes;
  ASDF_CHECK(ok, "Array data (offset " + std::to_string(offset) + ", shape " +
                     describe_extents(shape) + ", strides " +
                     describe_extents(strides) + ", element size " +
                     std::to_string(elemsize) + ") extends beyond the block (" +
                     std::to_string(nbytes) + " bytes)");
}

void ndarray::check_scalar_type(scalar_type_id_t requested) const {
  ASDF_CHECK(datatype->is_scalar && datatype->scalar_type_id == requested,
             "get_data_vector: the requested element type does not match the "
             "array's datatype");
}

vector<unsigned char> ndarray::get_data_bytes() const {
  const size_t elemsize = datatype->type_size();
  const int64_t npoints = num_elements();
  vector<unsigned char> result(size_t(npoints) * elemsize);
  if (npoints == 0)
    return result;

  // storage management: a block that was not loaded yet is forgotten again,
  // so that reading one array of a large file does not keep it in memory
  const bool old_ready = mdata.ready();
  const shared_ptr<block_t> block = mdata.get();
  check_bounds(block->nbytes());
  const auto *const base = static_cast<const unsigned char *>(block->ptr());

  // Odometer over `shape` in C order, i.e. with the last index fastest
  const int rank = shape.size();
  vector<int64_t> index(rank, 0);
  for (int64_t n = 0; n < npoints; ++n) {
    int64_t linear = offset;
    for (int d = 0; d < rank; ++d)
      linear += strides.at(d) * index.at(d);
    convert_element_to_host(base + linear, &result.at(size_t(n) * elemsize),
                            *datatype, byteorder);
    for (int d = rank - 1; d >= 0; --d) {
      if (++index.at(d) < shape.at(d))
        break;
      index.at(d) = 0;
    }
  }

  if (!old_ready)
    mdata.forget();
  return result;
}

} // namespace ASDF
