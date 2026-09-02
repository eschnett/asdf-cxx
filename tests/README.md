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

## `lz4.asdf`

Arrays `b` and `c` use the standard `lz4` block encoding, which needs the
Python `lz4` package to write. Its tests run only when CMake found
liblz4. asdf-cxx also writes its own LZ4 frame encoding under the token
`lz4f`, which Python does not read.

## Deliberately broken files

`corrupt-tag.asdf`, `corrupt-checksum.asdf`, `corrupt-truncated.asdf` and
`not-asdf.asdf` are derived from `python-default.asdf` by the fixture
script. The `error-*` tests run `asdf-ls` or `asdf-copy` on them through
`expect-error.sh`, which requires exit status 1 and an `error:` message
on stderr, so a crash does not count as passing. `error-checksum` only
runs when asdf-cxx was built with OpenSSL.
