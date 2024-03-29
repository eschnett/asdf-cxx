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

- History entries are not supported (i.e. are not written, and are
  ignored when reading).
- Simple ndarray references to other files (e.g. "exploded files") are
  not supported. (Full URI references are supported.)
- Streaming writes and reading streamed datasets is not supported.
- String types (i.e. arrays of fixed length strings) are not supported.
- Errors are not handled gracefully; the code will simply abort on
  most errors.

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

Things that should be improved:
- Tests should compare to the Python reference `asdf` library

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
