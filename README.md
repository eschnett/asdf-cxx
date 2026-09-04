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

asdf-cxx implements the "core" part of the ASDF standard for standard
versions 1.0.0 to 1.6.0. This section lists what it reads, what it
writes, what it refuses, and what it will only do when asked explicitly.

### What is read

- **Any file header.** The `#ASDF` magic is the only part that is
  required. The `#ASDF_STANDARD` line may be absent, or may name a
  version this library does not know; either way the file is read, and
  only a *copy* of it has to settle on a version that can be written.
  Both header lines are recognised on the first two lines of the file
  only, so a `#ASDF_STANDARD`-looking comment further down is an
  ordinary YAML comment and does not override the header.
- **The core tags of every standard version:** `core/asdf-1.0.0` and
  `core/asdf-1.1.0` as the root tag, `core/ndarray-1.0.0` and
  `core/ndarray-1.1.0`, `core/software-1.0.0`, and `core/complex-1.0.0`.
  A root node with no tag at all is accepted as well. The tags in the
  tree need not agree with the declared version; a file that mixes them
  is read, and a copy of it is either internally consistent again or
  refused (see *Standard versions* below).
- **Every datatype the standard defines:** `bool8`, `int8` … `int64`,
  `uint8` … `uint64`, `float16`, `float32`, `float64`, `complex64`,
  `complex128`, the string datatypes `[ascii, N]` and `[ucs4, N]`, and
  structured (record) datatypes built from those, with per-field byte
  order and per-field sub-array shapes. `float16` arrays stored as blocks
  are read, bounds-checked and copied even on a build whose compiler has
  no `_Float16`; only parsing or emitting *inline* float16 values needs
  the type.
- **Arrays as blocks and inline,** in either byte order, with an `offset`
  and with arbitrary `strides`, including negative and non-contiguous
  ones, and with rank 0. Blocks are read lazily, one at a time, so
  copying a large file does not load it all.
- **Compressed blocks** (`blosc`, `blosc2`, `bzp2`, `lz4`, `lz4f`,
  `zstd`, `zlib`, as far as the corresponding optional libraries were
  found at build time) and their MD5 checksums.
- **Files the writer does not produce:** blocks with
  `allocated_size > used_size`, padding between the tree and the first
  block, and a missing block index.
- **Unknown tags,** preserved (see below).

### What is written

- The root tag sits on the `---` line (`--- !core/asdf-1.1.0`), as the
  standard's own examples do, and core tags are written in their local
  form (`!core/ndarray-1.1.0`) rather than as verbatim URIs.
- `offset` and `strides` are written only when they differ from their
  defaults; `byteorder` only for blocks.
- `history` is preserved and round-trips unchanged; asdf-cxx adds no
  history entry of its own. Writing to standard 1.0.0 or 1.1.0, which
  have no `history.extensions`, keeps only the `entries` list, and drops
  the key entirely if there is none. `--allow-nonstandard` keeps the map
  as it stands.
- A scalar's YAML spelling decides its type and survives a copy: a quoted
  scalar stays a string however it looks (`"42"` does not become an
  integer), and a plain `y` or `n` stays a string too, matching what the
  reference implementation's parser resolves it to. A plain float keeps a
  decimal point in its mantissa, so a value written `1.0` is not copied
  out as `1` and read back as an integer, and one written `3.0e-10` is
  not copied out as `3e-10` and read back as a string (YAML 1.1 resolves
  a float only with that point).
- Complex numbers are written the way the `core/complex-1.0.0` grammar
  prescribes, `1.5-2.25i`, with the non-finite components spelled `inf`,
  `-inf` and `nan` rather than in YAML's own `.inf` / `.nan` form (which
  that grammar rejects). Both spellings are read, so files written by
  earlier versions of this library stay readable.
- The sign of a NaN is normalised, so every platform writes the same
  bytes for the same tree.

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

The declared version and the tags in the tree always agree, because both
come from one table (`include/asdf/version.hxx`).

| Standard | `core/asdf` | `core/ndarray` | `float16` | `history.extensions` |
|---|---|---|---|---|
| 1.0.0, 1.1.0 | 1.0.0 | 1.0.0 | no | no |
| 1.2.0 … 1.5.0 | 1.1.0 | 1.0.0 | no | yes |
| 1.6.0 | 1.1.0 | 1.1.0 | yes | yes |

- A file written from scratch declares the **lowest version that fits its
  content**: 1.2.0 normally, and 1.6.0 when the tree holds `float16` arrays,
  which no earlier version describes. This is the library default
  (`write_options`' `minimal` mode).
- `asdf-copy` **preserves the input file's declared version** by default
  (`input` mode). An input that declares no version, or one this library
  does not know, falls back to the lowest version that fits.
- `--standard-version=minimal|latest|input|X.Y.Z` overrides this;
  `write_options` and `set_standard_version()` do the same for the library.
  `latest` is 1.6.0. Asking for — or preserving — a version that cannot
  hold the content, for instance 1.0.0 for a `float16` array, is an error
  naming both versions:

  ```
  asdf-copy: error: This tree requires ASDF standard version 1.6.0, but standard version 1.0.0 was requested
  ```

  An unknown version is refused with the list of the supported ones.

### What is refused

These are recognised features of the format that asdf-cxx does not
implement. Each is reported by throwing `ASDF::error`; the command line
tools print the message and exit with status 1, and nothing is written.

| Feature | Message |
|---|---|
| Streamed block (header flag) | `Unsupported block flags 1 (streamed blocks are not supported)` |
| Streamed array (`*` in `shape`) | `This array has "*" in its shape; streamed arrays are not supported` |
| Exploded file (`source` names a file) | `This array's "source" is "…", the name of an external file; exploded files are not supported` |
| Masked array (`mask` key) | `This array has a "mask"; masked arrays are not supported` |
| Masked element (`null` in inline data) | `This inline array has a "null" element, which marks a masked value; masked arrays are not supported` |
| Input file cannot be opened | `Cannot open file "…"` |
| An external file a reference names cannot be opened | `Cannot open the external file "…" referenced from "…"` |
| Inline `float16`, `complex32` or `int128` value on a build whose compiler lacks the type | `Cannot parse float16 values: this build has no 16-bit …` |
| Block compressed with a codec this build does not have | `Block uses compression "…", which is not available in this build` |

The messages are quoted without the `[file.cxx:NNN]` source location
that `ASDF::error` appends to every message and the tools print. Writing
a masked array is refused too; a streamed array cannot be built in the
first place, as no constructor accepts one. Full URI references between
files *are* supported (`demo/demo-external.cxx`); it is only the ndarray
`source`-as-file-name form that is not.

### Nonstandard content (opt-in)

Some content no version of the standard describes can still be written,
but only on request: pass `--allow-nonstandard` to `asdf-copy`, or set
`write_options::allow_nonstandard`. Without it the write is refused
before the output file is touched, and the message lists every offending
node by path:

```
asdf-copy: error: This tree holds nonstandard content that no version of the ASDF standard describes:
  /alpha: inline rank-0 array
  /delta128: int128 datatype
  /delta16: complex32 datatype
Pass --allow-nonstandard (write_options::allow_nonstandard) to write it anyway.
```

The nonstandard items are:

- the `int128`, `uint128` and `complex32` datatypes, which exist in no
  version of the standard (they are always readable, whatever the flag);
- a rank-0 array in *inline* form, whose `data` would have to be a list.
  A rank-0 array stored as a block is standard, and Python asdf writes
  such arrays, so it always copies;
- a map under an unknown tag that carries an integer `source` (see
  above). This one is reported while the tree is being emitted, with its
  own message naming the tag, rather than in the list above.

`--allow-nonstandard` has one further effect that is not a refusal: it
keeps a `history` mapping as it stands when writing to standard 1.0.0 or
1.1.0, instead of reducing it to the `entries` list those versions
describe.

`float16` is **not** nonstandard: it is a legitimate 1.6.0 datatype and
selects that version instead.

One more asdf-cxx extension is not gated, because it only ever comes out
of asdf-cxx's own older files: `lz4f` block compression, the LZ4 frame
format. `lz4` is the standard's encoding and is what interoperates with
the Python `asdf` library; `lz4f` is kept readable and writable so that
files written by earlier versions of asdf-cxx, which used it
exclusively, stay usable. Use `--compression=lz4` for anything another
implementation has to read.

### Command line tools

```
asdf-ls <file>...
```

prints the YAML tree, the standard version the file declares, and one
line per array with its compressor, sizes, ratio and checksum.

```
asdf-copy [--array=block|inline]
          [--compression=none|blosc|blosc2|bzip2|lz4|lz4f|libzstd|zlib]
          [--compression-level=0..9]
          [--standard-version=minimal|latest|input|X.Y.Z]
          [--allow-nonstandard]
          <input file> <output file>
```

reads a file and writes it out again. Without `--array` each array keeps
the storage it had, without `--compression` each block keeps its
compressor, and without `--standard-version` the copy declares what the
original declared. A bad option prints `error:` followed by the usage and
exits with status 1.

### Roman Space Telescope files

Files written for the Nancy Grace Roman Space Telescope (WFI products
produced by `roman_datamodels`) are a supported input: they are read,
listed and copied with their data and metadata intact. That covers their
`float16` arrays, their `[ucs4, N]` string fields inside structured
arrays, the `gwcs`, `astropy` and `roman_datamodels` tags around nested
`core/ndarray` nodes, tagged `time` and `unit` scalars,
`history.extensions`, and YAML aliases — the last of which a copy
expands into equal but no longer shared nodes.
`tests/roman-like.asdf` combines these features in one small fixture;
see `tests/README.md`.

### Remaining limitations

- Non-YAML comments (using a `//` key) are ignored, and there is no
  way to generate such comments when writing ASDF files.
- Integers using more than 52 bits are not rejected.
- The block index is not used; instead, it is always re-created.
- Output files cannot be padded.
- A block that two arrays share is read correctly by both, but a copy
  writes it twice, once per array.
- Compression levels are not validated against the compressor, and the
  level a block was compressed with is not stored in the file.
- The ASDF standard requires that certain maps are output in a certain
  order, and that certain elements are output in a certain style
  ("block" or "flow"). However, it also requires that an ASDF reader
  must not rely on this. asdf-cxx does probably not yet honour all
  these "optional requirements".
- JSON URI references are not percent encoded.
- `yaml-cpp` outputs the YAML 1.2 format, whereas ASDF requires the
  YAML 1.1 format. The differences between these two versions are small,
  and asdf-cxx currently "cheats" by declaring the output to be YAML 1.1
  (which might not be true).

The conformance test suite — round trips of the ASDF standard's own
reference files for all seven versions, and cross-checks against the
Python reference implementation — is described in `tests/README.md`.

## Comments on the ASDF standard (version 1.1.0)

These are random comments and ideas regarding the ASDF file format.
Eventually, they need to be discussed with the ASDF standard
developers and/or others in the community.

- Why can rank-zero arrays (i.e. scalars) not be stored inline? A
  block-format rank-zero array is fine, but inline `data` has to be a
  list, so there is no way to spell one.
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

The test suite above needs nothing beyond the build. Two further test
families — round trips of the ASDF standard's reference files, and
cross-checks against the Python reference implementation — are opt-in
through `-DASDF_REFERENCE_FILES_DIR=...` and `-DASDF_PYTHON=...`; see
`tests/README.md`.
