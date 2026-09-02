# Test fixtures

Small ASDF files written by the Python reference implementation, used by
the ctest suite to check that asdf-cxx reads what other writers produce.
Regenerate them with `make_fixtures.py` (see its docstring); the exact
bytes depend on the Python `asdf` version, which is recorded in each
file's `asdf_library` entry.

Each file holds the same three arrays: `a` (int64, 2×3×4, uncompressed),
`b` (float64, 100 values, zlib), `c` (int32, 200 values, bzip2).

## `padded.asdf`

ASDF standard 1.0.0. Exercises three things asdf-cxx never writes itself:

- blocks whose header says `allocated_size > used_size` (padding after
  the payload);
- padding between the end of the YAML tree and the first block;
- no block index, so the last block ends exactly at the end of the file.

## `python-default.asdf`

Whatever a current Python `asdf` writes with default settings: a recent
standard version, `core/ndarray-1.1.0` tags, a `history` map with tagged
`core/extension_metadata-1.0.0` entries, and a block index. asdf-cxx
ignores `history` when reading and writes `core/ndarray-1.0.0` tags, so
the copy the tests produce is not byte-identical, but its arrays are.
