# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

asdf-cxx is a C++17 library (plus small CLI tools) for reading and writing
ASDF files: a YAML tree followed by binary blocks. Detailed architecture,
read/write paths, block header layout, and known gaps are in `CODE.md`;
read that before touching `src/ndarray.cxx` or `src/io.cxx`.

## Build and test

Out-of-source builds only. yaml-cpp is required; OpenSSL, bzip2, blosc,
blosc2, lz4, zlib, zstd are optional and auto-detected.

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Run a single test: `ctest --test-dir build -R compare-demo`. Tests run
from the build directory and write `*.asdf` files there. The tests are
the demo executables plus `asdf-ls` / `asdf-copy` round trips; there is
no unit-test framework. `compare-demo` diffs `asdf-ls` output of
`demo.asdf` and its copy via `diff-commands.sh`, ignoring lines that
mention compression or checksums.

Two further test families are opt-in, and a plain build needs neither:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DASDF_PYTHON=$PWD/asdf-env/bin/python \
  -DASDF_REFERENCE_FILES_DIR=$(tests/fetch-reference-files.sh)
```

`ASDF_REFERENCE_FILES_DIR` switches on the `ref-*` tests, which round
trip the ASDF standard's own reference files for all seven standard
versions; `ASDF_PYTHON` (an interpreter with `tests/requirements.txt`,
needs Python 3.11+) switches on the `py-*` tests, which cross-check
against the Python reference implementation. Without them ctest
registers 133 tests and passes; with them, about 840. See
`tests/README.md`.

The fixtures in `tests/` were written by the Python reference
implementation; `tests/make_fixtures.py` regenerates them (needs `asdf`
and `numpy`; use an isolated environment). Python asdf is also the best
way to check files this library writes: `asdf.open` validates against
the schemas.

Format with `clang-format -i <file>` (config in `.clang-format`, 80
columns, 2 spaces).

## Things that are easy to get wrong

- Errors from input files, unsupported features, caller misuse, or
  library calls are thrown as `ASDF::error` via `ASDF_CHECK(cond, msg)`
  and `ASDF_ERROR(msg)` from `include/asdf/error.hxx`. `assert` is only
  for internal invariants and is compiled out in Release. Tools catch
  exceptions in `main` and exit with status 1; `tests/expect-error.sh`
  checks that.
- Every serialisable class has three constructors: value,
  `(shared_ptr<reader_state>, YAML::Node)` for reading, and
  `(copy_state, const T&)` for copying, plus `to_yaml(writer&)`. Follow
  this pattern when adding types.
- Reading dispatches on YAML tag first, then node type, in
  `make_entry(rs, node)` (`src/entry.cxx`), through
  `classify_core_tag()`.
- All core tags come from `include/asdf/version.hxx`. Never write a
  `core/...` tag anywhere else; `src/version.cxx` is the one table that
  maps an ASDF standard version to its tags. The version a file is
  written as comes from `write_options`; `asdf-copy` preserves the
  input's version by default.
- Blocks are written lazily: `ndarray::to_yaml` only registers a task
  with `writer::add_task`; the bytes go out in `writer::flush()`, which
  must be called (the destructor asserts on it).
- Block data read from a file is loaded lazily through `memoized<block_t>`
  and forgotten again after `write_block` so `asdf-copy` streams.
- All headers do `using namespace std;` inside `namespace ASDF`.
- `include/asdf/config.hxx` is generated from `config.hxx.in`; add new
  `ASDF_HAVE_*` flags there and in `CMakeLists.txt`, not by hand.
- `asdf.i` (SWIG) and `cmp.cpp` are stale and not built; ignore them
  unless asked to revive the Python binding.
- `demo.asdf` must stay readable by stock Python asdf, which is used for
  cross-checks. Standard-conformant cases that Python cannot read (such
  as inline structured arrays) belong in `demo-nonstandard.cxx`.
- Nonstandard content (`int128`, `uint128`, `complex32`, an inline
  rank-0 array, an unknown-tagged map with an integer `source`) is
  refused on write unless `write_options::allow_nonstandard` /
  `--allow-nonstandard` is set. `float16` is standard in 1.6.0 and must
  never be treated as nonstandard.
- The error wording for unsupported features is asserted by tests
  (`tests/expect-error.sh -m <substring>`), so changing a message can
  break a test; the contract table is in
  `docs/standard-conformance-plan.md`.
- Roman Space Telescope files must stay readable: never turn a
  recognised datatype or a foreign tag into a whole-file error. A
  `float16` block must be readable and copyable even without `_Float16`,
  and `[ascii, N]` / `[ucs4, N]` datatypes and foreign tags must be
  preserved rather than refused.
- `TODO.md` in the repo root is the maintainer's private notes; do not
  edit it.
- `docs/standard-conformance-plan.md` is the conformance plan the test
  suite and the version handling were built from; keep its Status
  section current.
