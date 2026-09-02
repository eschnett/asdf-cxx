# asdf-cxx — Code Reference

A developer-oriented map of this repository: what lives where, how the
pieces fit together, how reading and writing actually work, and the
gotchas you will trip over. This complements `README.md` (user-facing
overview and standard conformance) and is not a substitute for reading
the headers, which are short.

Snapshot: project version 8.0.0 (`CMakeLists.txt`), claims ASDF
standard 1.2.0, ASDF file-format version 1.0.0. ~5,800 lines total
including demos and the SWIG file.

---

## 1. Repository layout

```
asdf-cxx/
├── CMakeLists.txt            Build, tests, install, pkg-config generation
├── cmake/Modules/            Find modules (pkg-config based) for blosc, blosc2,
│                             liblz4, libzstd, yaml-cpp
├── include/asdf/             Public headers (.hxx) — installed to include/asdf/
│   ├── asdf.hxx              Top-level `asdf` class; includes every other header
│   ├── config.hxx.in         Template for generated config.hxx (versions, HAVE_* flags)
│   ├── byteorder.hxx         byteorder_t, host_byteorder(), xtoh/htox byte swapping
│   ├── datatype.hxx          Scalar type ids, C++ type mapping, datatype_t / field_t
│   ├── entry.hxx             The `entry` class hierarchy (tree nodes) + make_entry()
│   ├── io.hxx                block_format_t, compression_t, reader_state, writer, copy_state
│   ├── memoized.hxx          memoized<T>: lazily evaluated, cached shared_ptr<T>
│   ├── ndarray.hxx           block_t storage classes, block_info_t, ndarray
│   ├── reference.hxx         `reference` ($ref / JSON-pointer style links)
│   ├── stl.hxx               yaml_encode/yaml_decode for std::vector and std::map
│   └── table.hxx             table / column (dormant, see §4.8)
├── src/                      One .cxx per header (asdf, byteorder, config, datatype,
│                             entry, io, ndarray, reference, table)
├── utils/
│   ├── copy.cxx              asdf-copy: read → copy(copy_state) → write
│   └── ls.cxx                asdf-ls: dump YAML tree + per-block info
├── demo/
│   ├── demo.cxx              asdf-demo: standard-conformant sample file (demo.asdf)
│   ├── demo-nonstandard.cxx  asdf-demo-nonstandard: adds 0-d arrays, float16, int128
│   ├── demo-external.cxx     asdf-demo-external: external + local references, resolves them
│   ├── demo-compression.cxx  asdf-demo-compression: round-trips every available compressor
│   ├── demo-large.cxx        asdf-demo-large: 1000×1000×250 float64 (2 GB) — not a test
│   └── *.py                  Python demos for the (stale) SWIG binding
├── asdf.i                    SWIG interface (stale, not built — see §10)
├── cmp.cpp                   Unfinished compare tool (stale, not built — see §10)
├── diff-commands.sh          Test helper: diff two commands' output, ignoring
│                             lines containing "compress" or "checksum"
├── test-std.sh               Manual script: asdf-copy the asdf-standard reference files
├── pkg-config.pc.in          Template for asdf-cxx.pc
├── .github/workflows/CI.yml  GitHub Actions (4 OS matrix, coverage on Ubuntu)
├── .clang-format             80 columns, 2-space indent, no tabs
└── BUILD.md                  Personal build notes (git-ignored)
```

Header dependency order (each header includes only those above it):

```
memoized.hxx   byteorder.hxx   config.hxx (generated)
      └──► io.hxx
           ├──► datatype.hxx ──► stl.hxx
           ├──► reference.hxx
           └──► ndarray.hxx ──► table.hxx
                 └──► entry.hxx ──► asdf.hxx
```

Every header ends with a `#define <GUARD>_DONE` and a trailing
`#ifndef <GUARD>_DONE / #error "Cyclic include depencency"` check.

---

## 2. Build system

- CMake ≥ 3.13, C++17 (`target_compile_features(... cxx_std_17)`).
- Out-of-source builds are enforced (`CMAKE_DISABLE_IN_SOURCE_BUILD`).
- **`-DNDEBUG` is deliberately stripped** from `CMAKE_CXX_FLAGS_RELEASE`
  and `..._RELWITHDEBINFO`. The library uses `assert` as its primary
  error-handling mechanism, so asserts must stay live in all build types.
- Required dependency: yaml-cpp. Optional: OpenSSL (MD5 checksums),
  bzip2, c-blosc, c-blosc2, lz4, zlib, zstd. Each optional dependency
  sets a `HAVE_*` CMake variable that becomes `ASDF_HAVE_*` in the
  generated `config.hxx`.
- `check_cxx_source_compiles` probes for `_Float16` and `__int128`,
  setting `ASDF_HAVE_FLOAT16` / `ASDF_HAVE_INT128`.
- `include/asdf/config.hxx.in` → `${build}/include/asdf/config.hxx`.
  Both `include/` and `${build}/include/` are on the include path; code
  always includes as `<asdf/xxx.hxx>`.
- Targets: static library `asdf-cxx`, executables `asdf-copy`,
  `asdf-ls`, `asdf-demo`, `asdf-demo-compression`, `asdf-demo-external`,
  `asdf-demo-large`, `asdf-demo-nonstandard`.
- `CODE_COVERAGE=ON` adds `--coverage` (used by CI).
- Install: headers to `include/asdf/`, library to `lib/`, executables
  to `bin/`, `asdf-cxx.pc` to `lib/pkgconfig/`.
- SWIG/Python: the `find_package(PythonInterp/PythonLibs)` calls are
  commented out (`#TODO`), so the SWIG section never activates.

Quick build:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build --output-on-failure
```

### Version consistency check

`config.hxx` defines `ASDF_CHECK_VERSION()`, which calls
`ASDF::check_version(header_version, have_float16, have_int128)`
(`src/config.cxx`). It compares the header's compiled-in version and
type-availability flags against the linked library's and exits with a
diagnostic on mismatch. Every executable calls it first thing in `main`.

---

## 3. Data model in one picture

```
asdf                                  (asdf.hxx)
 ├── tags: map<string,string>         extra %TAG directives for writing
 ├── grp: shared_ptr<group>           the tree root (normal case)
 ├── nodes: map<string,YAML::Node>    alt. root content: raw YAML nodes
 └── writers: map<string,fn(writer&)> alt. root content: emit callbacks

group : entry                         map<string, shared_ptr<entry>>
 └── entry (abstract)
      ├── null_entry                  ~
      ├── bool_entry                  true/false
      ├── int_entry                   int64
      ├── float_entry                 float64
      ├── complex_entry               !core/complex-1.0.0 "1+2i"
      ├── string_entry                always double-quoted
      ├── software                    !core/software-1.0.0 {name,author,homepage,version}
      ├── ndarray_entry ──► ndarray   !core/ndarray-1.0.0
      ├── reference_entry ──► reference   {$ref: "file#/json/pointer"}
      ├── sequence                    vector<shared_ptr<entry>>
      └── group                       (recursive)

ndarray                               (ndarray.hxx)
 ├── mdata: memoized<block_t>         lazily-loaded raw bytes
 ├── block_info: optional<block_info_t>  header fields, only after reading
 ├── block_format_t                   block | inline_array
 ├── compression_t, compression_level
 ├── mask: vector<bool>               must be empty on write
 ├── datatype: shared_ptr<datatype_t>
 ├── byteorder_t
 └── shape, offset, strides           C-order strides computed if empty

datatype_t                            (datatype.hxx)
 ├── is_scalar + scalar_type_id_t     e.g. id_float64
 └── fields: vector<shared_ptr<field_t>>   structured types (write-only, §11)
```

---

## 4. Class-by-class reference

### 4.1 `asdf` (`asdf.hxx`, `src/asdf.cxx`)

The document. Constructors come in three flavours that recur throughout
the codebase (see §8, Conventions):

| Constructor | Purpose |
|---|---|
| `asdf(tags, shared_ptr<group>)` | Build for writing from a tree |
| `asdf(tags, map<string,YAML::Node>)` / `asdf(tags, map<string, fn(writer&)>)` | Write raw YAML or custom emitters at the root |
| `asdf(const string &filename, readers={})` / `asdf(shared_ptr<istream>, filename, readers={})` | Read from disk / stream |
| `asdf(shared_ptr<reader_state>, YAML::Node, readers={})` | Read from an already-parsed tree |
| `asdf(const copy_state &, const asdf &)` | Copy with altered block format/compression |

Key methods: `write(ostream&)` / `write(filename)`, `copy(copy_state)`,
`get_group()`, `static from_yaml(istream&)` (reads the YAML head of a
file, see §5.2), `to_yaml(writer&)`.

The `readers` parameter (custom tag → callback) is accepted but
`assert(readers.empty())` — it is not implemented.

### 4.2 `entry` hierarchy (`entry.hxx`, `src/entry.cxx`)

`entry` is an abstract base with:
- `get_entry_type()` → `entry_type_t` enum
- `copy(const copy_state&)` → deep copy
- `to_yaml(writer&)` and friend `operator<<(writer&, ...)`
- a family of `get_maybe_*()` virtuals returning `std::optional` or a
  null `shared_ptr` when the entry is not of that kind. This is the
  idiomatic way to inspect a tree without `dynamic_cast`:
  `ent->get_maybe_ndarray()`, `ent->get_maybe_group()`, etc.

Concrete subclasses also have a typed getter (`get_int()`,
`get_ndarray()`, …) and a `value_type` alias.

`group` (a `std::map<string, shared_ptr<entry>>` behind a shared_ptr)
and `sequence` (a `std::vector<...>`) offer `insert`/`emplace`,
`push_back`/`emplace_back`, `at`, `count`, `size`. `emplace` accepts any
value that `make_entry(...)` knows how to wrap.

`make_entry(...)` overloads (bottom of `entry.hxx`) wrap plain C++
values — `bool`, integral, floating, `complex<T>`, `string`, `ndarray`,
`reference`, vectors/maps of entries, and `shared_ptr` versions of the
same — into the right entry type. This is what lets you write
`grp->emplace("beta", make_shared<ndarray>(...))`.

`make_entry(rs, node)` in `src/entry.cxx` is the **read-side
dispatcher** (see §5.3).

### 4.3 `ndarray` and block storage (`ndarray.hxx`, `src/ndarray.cxx`)

Storage abstraction:

| Class | Owns data? | Notes |
|---|---|---|
| `block_t` (abstract) | — | `ptr()`, `nbytes()`, `reserve()`, `resize()` |
| `typed_block_t<T>` | yes, `vector<T>` | Specialised for `bool` to store `vector<unsigned char>` |
| `ptr_block_t` | no | Non-owning view; `reserve`/`resize` assert |

`ndarray` holds its bytes as `memoized<block_t>` so that blocks read
from a file are only loaded (seeked, read, checksummed, decompressed)
on first access. `get_data()` returns the memoized handle; `->ptr()`
triggers the load.

Constructors:
- `ndarray(vector<T> data, block_format, compression, level, mask, shape, offset=0, strides={})`
  — the typical write-side constructor. Infers `datatype` from `T` via
  `get_scalar_type_id<T>`, sets host byte order, wraps data in
  `typed_block_t<T>` inside a constant memoized.
- `ndarray(memoized<block_t>, optional<block_info_t>, block_format, compression, level, mask, datatype, byteorder, shape, offset, strides)`
  — the general form; validates shape/mask/strides and fills C-order
  strides when `strides` is empty.
- `ndarray(rs, node)` — read; `ndarray(cs, arr)` — copy.

Accessors: `get_datatype()`, `get_shape()`, `get_offset()`,
`get_strides()`, `get_block_info()` (only meaningful after reading),
`get_data_vector<T>()` (copies out, asserts type match),
`linear_index(idx)`.

`static read_block(shared_ptr<istream>)` parses one binary block header
and returns `{memoized<block_t>, block_info_t}` (§5.2).
`write_block(ostream&)` (private) compresses and writes one block (§6).

### 4.4 `datatype_t`, `field_t`, scalar types (`datatype.hxx`, `src/datatype.cxx`)

- `scalar_type_id_t` enumerates `bool8, int8…int128, uint8…uint128,
  float16, float32, float64, complex32, complex64, complex128, ascii, ucs4`.
- Compile-time mapping both ways: `get_scalar_type_id<T>::value` and
  `get_scalar_type_t<id>`. `static_assert`s at the top of
  `src/datatype.cxx` pin them together.
- `int128`/`uint128`, `float16`/`complex32` exist only under
  `ASDF_HAVE_INT128` / `ASDF_HAVE_FLOAT16`.
- `ascii` and `ucs4` are declared but have no size, parse or emit
  support (all switch statements comment them out).
- Per-type `yaml_decode(node, T&)` / `yaml_encode(T)` overloads.
  Complex numbers are encoded as the string `re±imi` (e.g. `-4.4-5.5i`)
  with tag `!core/complex-1.0.0`; decoding uses a regex.
- `parse_scalar(node, unsigned char *dst, type_or_datatype, byteorder)`
  and `emit_scalar(const unsigned char *src, ...)` convert between a
  YAML scalar and raw bytes, byte-swapping via `htox<N>` / `xtoh<T>`.
- `datatype_t` is either scalar or a list of `field_t` (name, datatype,
  optional byteorder, shape). `type_size()` sums fields for structured
  types.

### 4.5 `reader_state`, `writer`, `copy_state` (`io.hxx`, `src/io.cxx`)

`reader_state` is the shared context for everything read from one file:
- `tree` — the whole YAML document
- `filename` — used to resolve relative external references
- `blocks` / `block_infos` — one lazily-loading memoized block per
  binary block, in file order; `get_block(i)`, `get_block_info(i)`
- `other_files` — cache of `reader_state` for external files opened via
  references
- `resolve_reference(path)` walks the YAML tree by JSON-pointer path
  (sequence index via `stoi`, or map key). The static overload picks
  the same or an external file first.

`writer` wraps a `YAML::Emitter` on an `ostream`:
- constructor writes the `#ASDF …`, `#ASDF_STANDARD …`, comment,
  `%YAML 1.1`, `%TAG ! tag:stsci.edu:asdf/` lines plus any user tags,
  then `BeginDoc`
- `operator<<` forwards to the emitter, with a special overload for
  `std::complex<T>`
- `add_task(fn(ostream&))` queues a block-writing closure and returns
  its index — this index **is** the `source:` value in the YAML
- `flush()` emits `EndDoc`, runs the queued tasks in order (writing the
  binary blocks and recording their offsets), then writes the block
  index
- the destructor asserts that `flush()` was called

`copy_state` is a small struct of `set_X` / `X` pairs for block format,
compression and compression level. `ndarray(cs, arr)` applies the set
ones; every other class's copy constructor just recurses.

### 4.6 `reference` (`reference.hxx`, `src/reference.cxx`)

Represents `{$ref: "<file>#<fragment>"}`.
- `reference(base_target, doc_path)` builds the target string: the
  path elements are tilde-encoded (`~`→`~0`, `/`→`~1`), joined with
  `/`, then percent-encoded as a URI fragment.
- `get_split_target()` reverses this.
- `resolve()` returns `{reader_state, YAML::Node}` for the target; the
  caller then constructs whatever it expects (e.g. `ndarray(rs, node)`),
  as `demo/demo-external.cxx` shows.
- Written as a flow map with a double-quoted value (Python's YAML
  rejects unquoted strings containing `:`).

### 4.7 `memoized<T>` (`memoized.hxx`)

`memoized<T>` is a copyable handle to a shared `memoized_state<T>`
holding a `function<shared_ptr<T>()>` and its cached result.
`get()` / `*` / `->` compute on first use; `ready()` tells whether the
value is cached; `forget()` drops the cache so it can be recomputed
later. `make_constant_memoized(shared_ptr<T>)` wraps already-present
data. `valid()` is false for a default-constructed handle (used as the
"no more blocks" sentinel by `read_block`).

### 4.8 `table` / `column` (`table.hxx`, `src/table.cxx`) — dormant

Implements `!core/table-1.0.0` / `!core/column-1.0.0` read/write, but
nothing in `asdf` or `make_entry` references them (the wiring is
commented out in `asdf.hxx`/`asdf.cxx`). Usable only by constructing
them directly.

### 4.9 `byteorder.hxx`

`byteorder_t {undefined, big, little}`, `host_byteorder()` (runtime
check), `xtoh<T>(bytes, order)` → host value, `htox<T>(value, order)`
→ bytes, `htox<N>(bytes*, order)` in-place swap. Block headers are
always big-endian and handled separately by `input`/`output` helpers in
`src/ndarray.cxx`.

---

## 5. Read path

### 5.1 Entry points

`asdf(filename)` → `asdf(make_shared<ifstream>, filename)` →
`from_yaml(*is)` + `make_shared<reader_state>(node, is, filename)` →
`asdf(rs, node)`.

### 5.2 `asdf::from_yaml` and block discovery

1. Read 5 bytes; must be `#ASDF`, otherwise print a diagnostic and
   `exit(2)`. The format version after it is **not** checked.
2. Read the stream line by line, appending to a string, until a line
   equal to `...` (YAML end-of-document). `YAML::Load` the accumulated
   text. The tree is therefore fully buffered in memory as text.
3. `reader_state`'s constructor then loops `ndarray::read_block(pis)`
   from the current stream position:
   - read 4 bytes; if not the block magic, seek back 4 bytes and stop
     (this is how the block index or EOF terminates the scan);
   - parse the fixed header fields (§7), assert `flags == 0`, map the
     4-byte compression code to `compression_t`;
   - skip any extra header bytes beyond those understood;
   - create a `memoized<block_t>` whose closure captures the stream,
     the data offset, sizes, compression and checksum and calls
     `read_block_data` on first use;
   - seek to `block_begin + used_space` and continue.

   The `#ASDF BLOCK INDEX` trailer is never read; blocks are always
   located by this sequential scan.

### 5.3 Tree construction: `make_entry(rs, node)`

`asdf(rs, node)` asserts the root tag is `core/asdf-1.0.0`, `1.1.0` or
`1.2.0` and builds `group(rs, node)`, which calls `make_entry` for each
value. Dispatch order in `src/entry.cxx`:

1. **By tag**: `core/complex-1.0.0` → `complex_entry`;
   `core/software-1.0.0` → `software`; `core/ndarray-1.0.0` →
   `ndarray_entry`. Any other non-empty tag hits `assert`.
2. **Null** node → `null_entry`.
3. **Scalar**: try `as<bool>`, then `as<int64_t>`, then `as<double>`,
   else `string_entry`.
4. **Sequence** → `sequence`.
5. **Map** with a `$ref` key → `reference_entry`, otherwise `group`.

### 5.4 `ndarray(rs, node)`

- `source:` present → block format. Reads `datatype`, `byteorder`,
  `shape`, optional `offset` (default 0) and `strides` (default
  C-order). `mdata = rs->get_block(source)`; `block_info` recorded.
  **`compression` is set to `zlib` level 9 as a placeholder** (the
  original compressor is only in `block_info`), which is what a later
  copy/write will use unless overridden.
- `data:` present → inline format. `shape` is inferred from nesting if
  absent; `datatype` is inferred by attempting to parse as int64, then
  float64, then complex128. Data is parsed into a
  `typed_block_t<unsigned char>` in host byte order.
- `mask:` is ignored on read.

### 5.5 `read_block_data` (lazy block load)

Seeks to the data offset, reads `allocated_space` bytes, verifies the
MD5 checksum if OpenSSL is available and the stored checksum is
non-zero, then decompresses according to the compression code into a
buffer of `data_space` bytes. Returns a `typed_block_t<unsigned char>`.

---

## 6. Write path

`asdf::write(filename)` opens an `ofstream` (binary, truncate) and calls
`write(ostream&)`:

1. Construct `writer w(os, tags)` — writes the textual header lines and
   `BeginDoc`.
2. `w << *this` → `asdf::to_yaml`:
   - `!core/asdf-1.1.0` map (note: header says standard 1.2.0, tag says
     1.1.0);
   - `asdf/library:` → `software(ASDF_CXX_NAME, AUTHOR, HOMEPAGE, VERSION)`;
   - every entry of `grp` except a key literally named `asdf/library`;
   - then `nodes`, then `writers` (the alternative root contents).
   Each entry's `to_yaml` recurses. Map keys come out in
   `std::map` order (alphabetical), which is why `attributed` precedes
   `beta` in `demo.asdf`.
3. `ndarray::to_yaml` for block format does **not** write bytes yet: it
   captures a copy of the ndarray in a closure, registers it with
   `w.add_task(...)`, and writes the returned index as `source:`. For
   inline format it emits nested YAML sequences via `emit_inline_array`
   (honouring `strides` and `offset`). It always writes `datatype` and
   `shape`; `byteorder`, `offset`, `strides` only for block format.
   `assert(mask.empty())`.
4. `w.flush()`:
   - `EndDoc` (`...`);
   - for each task, record `os.tellp()` then run it →
     `ndarray::write_block(os)` (§6.1);
   - write `#ASDF BLOCK INDEX`, `%YAML 1.1`, and a one-line flow
     sequence of the recorded offsets as a YAML document.

### 6.1 `ndarray::write_block`

Builds the header in a `vector<unsigned char>` with big-endian
`output()` helpers, compresses the payload according to `compression`
(blosc, blosc2, bzip2, lz4 frame, zlib — each guarded by its
`ASDF_HAVE_*`), computes the MD5 of the **compressed** bytes (zeros
without OpenSSL), back-patches `header_size`, then writes header,
payload, and a zero-length padding. For blosc, bzip2 and zlib, if the
compressed size is not smaller than the input, it falls back to
`compression = none` for that block.

Memory management: if the block data was not already loaded before
writing (`!old_ready`), `get_data().forget()` is called afterwards so
that `asdf-copy` streams one block at a time through memory.

---

## 7. Binary block header as implemented

All multi-byte integers big-endian. Written `header_size` is 48.

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 4 | magic | `0xD3 'B' 'L' 'K'` |
| 4 | 2 | header_size | bytes following this field |
| 6 | 4 | flags | must be 0 |
| 10 | 4 | compression | `\0\0\0\0` none, `blsc`, `bls2`, `bzp2`, `lz4f`, `zstd`, `zlib` |
| 14 | 8 | allocated_space | bytes read as compressed payload |
| 22 | 8 | used_space | on write equals allocated_space; on read the reader seeks `block_begin + used_space` |
| 30 | 8 | data_space | uncompressed size |
| 38 | 16 | checksum | MD5 of compressed payload; all zeros = none |

Read-side asserts `used_space >= allocated_space`, which is the
opposite of the standard's `used_size <= allocated_size`. Since asdf-cxx
always writes them equal this round-trips its own files, but a padded
file produced by another writer (allocated > used) will fail that
assert. See §11.

---

## 8. Conventions and idioms

- Everything is in `namespace ASDF`, and every header does
  `using namespace std;` **inside** that namespace. Expect unqualified
  `string`, `vector`, `shared_ptr` throughout.
- File extensions: headers `.hxx`, sources `.cxx`. Include as
  `<asdf/asdf.hxx>` (which pulls in everything).
- **Three-constructor pattern** on every serialisable class `X`:
  1. value constructor(s) for building in memory;
  2. `X(const shared_ptr<reader_state> &rs, const YAML::Node &node)` — read;
  3. `X(const copy_state &cs, const X &other)` — copy with copy_state applied;
  plus `writer &to_yaml(writer &w) const` and a friend
  `operator<<(writer&, const X&)` that calls it.
- Free-function serialisation for plain values: `void yaml_decode(const
  YAML::Node&, T&)` and `YAML::Node yaml_encode(T)`, overloaded per type
  in `datatype.hxx`/`byteorder.hxx`, and generically for `vector<T>` /
  `map<K,T>` in `stl.hxx` (vectors are emitted in flow style).
- Ownership: trees are `shared_ptr` everywhere; `group`/`sequence` hold
  their containers behind a `shared_ptr` so `get_group()` /
  `get_sequence()` hand out a live view.
- **Error handling is `assert` and `exit`.** There is no exception-based
  error path except a few `throw std::invalid_argument` in
  `get_scalar_type_size`. Do not build with `NDEBUG`.
- Formatting: clang-format (LLVM base, 80 cols, 2 spaces, no tabs).
  Run `clang-format -i` on touched files.
- Feature probes at runtime: `have_compression_*()`, `have_checksum()`,
  `have_float16()`, `have_int128()` in `io.hxx` mirror the compile-time
  `ASDF_HAVE_*` macros. Demos use them to skip unavailable compressors.

---

## 9. Tools, demos, tests, CI

### Utilities

- **asdf-ls** `<file>...` — prints the YAML tree (via `from_yaml`), then
  re-reads the file as an `asdf` and walks the tree printing, for every
  ndarray, its block's compressor, compressed/uncompressed sizes, ratio
  and checksum. Scalars are not printed in the second pass (TODO in code).
- **asdf-copy** `[--array=block|inline] [--compression=none|blosc|blosc2|bzip2|libzstd|zlib] [--compression-level=0..9] <in> <out>`
  — read, `copy(copy_state)`, write. Without `--compression`, every
  block read from a file is re-written with zlib level 9 (§5.4). lz4 is
  not offered on the command line although the library supports it.

### Demos (also serve as the test suite)

| Executable | Output | Purpose |
|---|---|---|
| asdf-demo | demo.asdf | Mix of block/inline arrays, scalars, nested group, sequence, reference; bzip2 and zlib blocks |
| asdf-demo-nonstandard | nonstandard.asdf | Same plus 0-d arrays and, if available, float16/complex32/int128 arrays with blosc |
| asdf-demo-external | external.asdf, metadata.asdf | Writes a file and a second file referencing it, then resolves local, remote, and remote-to-local references and prints the data |
| asdf-demo-compression | compression.asdf | Writes a 101³ float64 array with every available compressor, reads back, verifies equality |
| asdf-demo-large | large.asdf | 2 GB single block; stress/perf only |

### ctest (`CMakeLists.txt`)

`demo` → `ls demo.asdf` → `demo-nonstandard` → `ls2 nonstandard.asdf` →
`copy demo.asdf demo2.asdf` → `ls3 demo2.asdf` → `compare-demo`
(`diff-commands.sh` diffs `asdf-ls` output of original and copy,
filtering lines mentioning compress/checksum because the copy
re-compresses with zlib) → `external`. asdf-demo-compression and
asdf-demo-large are built but not registered as tests.

There is no unit-test framework and no comparison against the Python
reference implementation (`test-std.sh` is a manual helper that expects
the asdf-standard reference files checked out under `~/src/asdf/`).

### CI (`.github/workflows/CI.yml`)

Matrix: macOS 15 (Intel and ARM), Ubuntu 24.04 (x86-64 and ARM). Ninja,
Debug, `CODE_COVERAGE=ON`, build → ctest → install → lcov + Codecov
(Ubuntu only; lcov errors on macOS). Note the install prefix is written
`"{$HOME}/install"` (braces outside the `$`), so it lands in a literal
`{…}` directory; harmless.

---

## 10. Stale or unbuilt files

- `asdf.i` (SWIG) and `cmp.cpp` both `#include <asdf/asdf.hpp>`, a
  header name that no longer exists (everything is `.hxx`). The SWIG
  interface also mirrors an older API (`entry.create_from_ndarray(name,
  arr, "")`, `group.create(...)`, `asdf.create_from_group`) and its
  `compression_t` enum lacks `liblz4`. `demo/*.py` target that old API.
  None of this is compiled because the Python detection in
  `CMakeLists.txt` is commented out. Treat the Python binding as
  historical.
- `cmp.cpp` is not referenced by CMake and would not compile
  (uses undeclared `inputfilename`).

---

## 11. Known gaps and gotchas (from reading the code)

1. **zstd is wired everywhere except where it matters.** `compression_t::libzstd`
   exists, CMake detects libzstd, `config.hxx` gets `ASDF_HAVE_LIBZSTD`,
   `<zstd.h>` is included, the `zstd` block token is recognised on read,
   and `asdf-copy` accepts `--compression=libzstd` — but neither
   `read_block_data` nor `write_block` has a `case compression_t::libzstd`,
   so both fall into `default: assert(0)`.
2. **`have_datatype_int128()` / `have_datatype_float16()`** in
   `src/datatype.cxx` test `HAVE_INT128` / `HAVE_FLOAT16` (no `ASDF_`
   prefix) and therefore always return false. Use `have_int128()` /
   `have_float16()` from `io.hxx`, which test the right macros.
3. **Structured datatypes cannot be read.** `field_t(rs, node)` and
   `field_t(cs, field)` / `datatype_t(cs, datatype)` are `assert(0)`.
   Writing structured types works (`field_t::to_yaml`).
4. **Masks are not supported.** Read ignores `mask:`; write asserts
   the mask is empty.
5. **Read-side default compression is zlib 9** for every block, so
   `asdf-copy` without `--compression` changes the compressor of
   anything that was not zlib.
6. **Block header size semantics inverted** on read
   (`assert(used_space >= allocated_space)`; payload length taken from
   `allocated_space`). Padded blocks from other writers will abort.
7. **Root tag mismatch**: header advertises `#ASDF_STANDARD 1.2.0`, tree
   is tagged `!core/asdf-1.1.0`. The reader accepts 1.0.0–1.2.0.
8. **Block index** is written but never read; files are always scanned
   sequentially. Streamed blocks and exploded (`source:` as a URI
   string) files are unsupported.
9. **YAML head is read line-by-line until `...`** and buffered as text.
   A file whose YAML lacks the `...` terminator fails with "Stream input
   error".
10. `ascii`/`ucs4` scalar types are declared but unimplemented.
11. `asdf-copy` accepts `--compression-level` only as ten literal
    strings (`--compression-level=0` … `=9`); anything else asserts.
12. Reading compares the YAML tag strings exactly; e.g. an ndarray
    tagged `core/ndarray-1.1.0` would not be recognised.
13. The `asdf(readers=...)` hook for custom tags asserts if non-empty.
14. yaml-cpp emits YAML 1.2 syntax while the header declares
    `%YAML 1.1` (documented in README).

---

## 12. Where to start for common changes

**Add a compressor** (zstd is 80% done and a good template):
1. `io.hxx`: enum value; `io.cxx`: `operator<<` name and `have_compression_*()`.
2. `ndarray.cxx`: 4-byte token in `read_block`; `case` in
   `read_block_data` (decompress into `data_space` bytes); `case` in
   `write_block` (compress, set `comp`, optionally fall back to none).
3. `CMakeLists.txt` + `cmake/Modules/FindX.cmake` + `config.hxx.in`
   `ASDF_HAVE_X`, and the pkg-config `Requires`/`Libs` lines.
4. `utils/copy.cxx` CLI option; `demo/demo-compression.cxx` round-trip.

**Add an entry type**:
1. `entry.hxx`: `entry_type_t` value, forward declaration, subclass
   following an existing one (`value_type`, three constructors,
   `get_entry_type`, `copy`, `to_yaml`, `get_maybe_X` override), a
   `get_maybe_X` virtual on `entry`, and `make_entry` overloads.
2. `entry.cxx`: `operator<<(entry_type_t)`, `to_yaml`, read
   constructor, and a branch in `make_entry(rs, node)` (tag first).

**Add a scalar type**:
`datatype.hxx` (`scalar_type_id_t`, typedef, `get_scalar_type_id`,
`get_scalar_type<>`, `yaml_decode`/`yaml_encode` declarations) and
`datatype.cxx` (static_asserts, `get_scalar_type_size`, name
string ↔ id, `yaml_decode`/`yaml_encode`, `parse_scalar`,
`emit_scalar`).

**Inspect a file during debugging**: `./build/asdf-ls file.asdf`, or
`head -c 4096 file.asdf | strings` to see the YAML head, and
`tail -c 200 file.asdf` to see the block index.
