# [asdf-cxx](https://github.com/eschnett/asdf-cxx)

[![CI](https://github.com/eschnett/asdf-cxx/actions/workflows/CI.yml/badge.svg)](https://github.com/eschnett/asdf-cxx/actions/workflows/CI.yml)
[![codecov](https://codecov.io/gh/eschnett/asdf-cxx/graph/badge.svg?token=JOF2CKYN52)](https://codecov.io/gh/eschnett/asdf-cxx)

asdf-cxx: A C++ implementation for ASDF, the Advanced Scientific Data
Format

## Overview

[ASDF](https://github.com/spacetelescope/asdf-standard) is an
efficient file format for structure scientic data, backed by NASA's
[Space Telescope Science Institute (STScI)](http://www.stsci.edu).
ASDF stores both array data (supporting efficient binary
representations) as well as accompanying metadata in "key-value" form.
Metadata are stored in the human-readable [YAML](http://yaml.org)
format.

ASDF exists as standard, and there are (so far) implementations in
[Python](https://github.com/spacetelescope/asdf),
[C++](https://github.com/spacetelescope/asdf-cpp), and
[Go](https://github.com/astrogo/asdf).

This library [asdf-cxx](https://github.com/eschnett/asdf-cxx) provides
an independent implementation in C++, suitable for HPC environments.
This library `asdf-cxx` and the other C++ library `asdf-cpp` are very
similar in terms of the features they offer while providing different
APIs.

## Standard conformance

asdf-cxx supports most of the "core" ASDF standard. Notable exceptions
are:

- `history` is preserved and round-trips unchanged; asdf-cxx adds no
  history entry of its own. Writing to standard 1.0.0 or 1.1.0, which
  have no `history.extensions`, keeps only the `entries` list.
- Masked arrays are not supported: a `mask` key, or a `null` element in
  inline data, is an error.
- Simple ndarray references to other files (e.g. "exploded files") are
  not supported. (Full URI references are supported.)
- Streaming writes and reading streamed datasets is not supported.
- Errors (malformed files, unsupported features, failing compression
  libraries) are reported by throwing `ASDF::error`. The command line
  tools print the message and exit with status 1.

A scalar's YAML spelling decides its type and survives a copy: a quoted
scalar stays a string however it looks (`"42"` does not become an
integer), and a plain `y` or `n` stays a string too, matching what the
reference implementation's parser resolves it to. A plain float keeps a
decimal point in its mantissa, so a value written `1.0` is not copied out
as `1` and read back as an integer, and one written `3.0e-10` is not
copied out as `3e-10` and read back as a string (YAML 1.1 resolves a
float only with that point).

Complex numbers are written the way the `core/complex-1.0.0` grammar
prescribes, `1.5-2.25i`, with the non-finite components spelled `inf`,
`-inf` and `nan` rather than in YAML's own `.inf` / `.nan` form (which
that grammar rejects). Both spellings are read, so files written by
earlier versions of this library stay readable.

Tags that asdf-cxx does not interpret — an extension's, another
project's, or a future version of a core tag — are read and written back
unchanged, with the node under them, so a copy round-trips them
silently. A tagged scalar keeps its text exactly as it stands, so a
timestamp gains no quotes and `1.0` does not become `1`. Nested
`core/ndarray` nodes inside such a map (a quantity's value, a table
column) are parsed as arrays and their blocks are copied as usual. The
one thing that is refused is writing a map under an unknown tag that has
an integer `source`: it refers to a binary block that asdf-cxx does not
know how to copy, so the `source` in the copy would point at the wrong
block. `--allow-nonstandard` writes it anyway.

String datatypes are supported: the standard's `[ascii, N]` (N bytes per
element) and `[ucs4, N]` (N four-byte code units in the array's byte order)
are read, written, copied and converted between block and inline form, both
as whole arrays and as fields of a structured array. `ndarray` exposes them
as `get_data_vector<std::string>()` and `get_data_vector<std::u32string>()`,
with the trailing null padding removed.

### Standard versions

ASDF standard versions 1.0.0 to 1.6.0 are read and written. The declared
version and the tags in the tree always agree, because both come from one
table (`include/asdf/version.hxx`).

- A file written from scratch declares the **lowest version that fits its
  content**: 1.2.0 normally, and 1.6.0 when the tree holds `float16` arrays,
  which no earlier version describes.
- `asdf-copy` **preserves the input file's declared version** by default. An
  input that declares no version, or one this library does not know, falls
  back to the lowest version that fits.
- `--standard-version=minimal|latest|input|X.Y.Z` overrides this;
  `write_options` and `set_standard_version()` do the same for the library.
  Asking for a version that cannot hold the content (for instance 1.0.0 for a
  `float16` array) is an error naming both versions.
- Content that **no** version of the standard describes -- `int128`,
  `uint128` and `complex32` datatypes, and a rank-0 array in *inline* form,
  whose `data` would have to be a list -- is refused unless
  `--allow-nonstandard` (`write_options::allow_nonstandard`) is given. Note
  that `float16` is a legitimate 1.6.0 datatype and never counts as
  nonstandard, and neither does a rank-0 array stored as a block.

Other minor limitations are:
- Non-YAML Comments (using a `//` key) are ignored, and there is no
  way to generate such comments when writing ASDF files.
- Integers using more than 52 bits are not rejected.
- The block index is not used; instead, it is always re-created.
- Output files cannot be padded.
- The ASDF standard requires that certain maps are output in a certain
  order, and that certain elements are output in a certain style
  ("block" or "flow"). However, it also requires that an ASDF reader
  must not rely on this. asdf-cxx does probably not yet honour all
  these "optional requirements".
- JSON URI references are not percent encoded
- Two lz4 encodings are supported. `lz4` is the standard's encoding and
  interoperates with the Python `asdf` library. `lz4f`, the LZ4 frame
  format, is an asdf-cxx extension that other implementations do not
  read; it is kept so that files written by earlier versions of asdf-cxx,
  which used it exclusively, stay readable.

Things that should be improved:
- More tests should compare to the Python reference `asdf` library
  (the fixtures in `tests/` are a start)

Also, the `yaml-cpp` library outputs the YAML 1.2 format, whereas ASDF
requires the YAML 1.1 format. The differences between these two
version is small, and asdf-cxx currently "cheats" by declaring the
output to be YAML 1.1 (which might not be true).

## Comments on the ASDF standard (version 1.1.0)

These are random comments and ideas regarding the ASDF file format.
Eventually, they need to be discussed with the ASDF standard
developers and/or others in the community.

- Why are rank-zero arrays (i.e. scalars) not supported?
- Why are complex numbers encoded in such a complicated way instead of
  simply as a two-element array?
- The tree could contain a pointer to the block index. Initially, one
  would write a placeholder saying "there is no index", and after
  writing the index, the placeholder could be overwritten by the block
  index location. That would simplify finding the index and would
  provide an additional validity check.
- It would be interesting to be able to split arrays into multiple
  blocks. This would allow tiled representations (which can be much
  faster for partial reading), and would allow not storing large
  masked regions.

## Build instructions

Requirements:

- C++17-capable C++ compiler (tested with
  [Clang](https://clang.llvm.org) and [GCC](https://gcc.gnu.org))
- [cmake](https://cmake.org)
- [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) library
- [OpenSSL](https://www.openssl.org) (optional, for MD5 checksums)
- [bzip2](http://bzip.org) library (optional, for compression)
- [c-blosc](https://www.blosc.org) library (optional, for compression)
- [c-blosc2](https://www.blosc.org) library (optional, for compression)
- [lz4](https://lz4.org) library (optional, for compression)
- [zlib](http://zlib.net) library (optional, for compression)
- [zstd](https://github.com/facebook/zstd) library (optional, for compression)

To build:

```sh
git clone https://github.com/eschnett/asdf-cxx
cd asdf-cxx
cmake -B build -S .
cmake --build build
ctest --test-dir build
cmake --install build
```

All compression and checksum libraries are optional. Configure with
`-DASDF_REQUIRE_ALL_DEPENDENCIES=ON` to make CMake fail instead of
silently leaving a feature out; CI does this.
