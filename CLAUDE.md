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

The fixtures in `tests/` were written by the Python reference
implementation; `tests/make_fixtures.py` regenerates them (needs `asdf`
and `numpy`; use an isolated environment). Python asdf is also the best
way to check files this library writes: `asdf.open` validates against
the schemas.

Format with `clang-format -i <file>` (config in `.clang-format`, 80
columns, 2 spaces).

## Things that are easy to get wrong

- Error handling is `assert` and `exit`. `-DNDEBUG` is deliberately
  stripped from Release flags in `CMakeLists.txt`; do not add it back.
- Every serialisable class has three constructors: value,
  `(shared_ptr<reader_state>, YAML::Node)` for reading, and
  `(copy_state, const T&)` for copying, plus `to_yaml(writer&)`. Follow
  this pattern when adding types.
- Reading dispatches on YAML tag first, then node type, in
  `make_entry(rs, node)` (`src/entry.cxx`). Tags are compared as exact
  strings.
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
- `TODO.md` in the repo root is the maintainer's private notes; do not
  edit it.
