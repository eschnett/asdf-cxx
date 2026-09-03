# Tests

This directory holds the fixtures, helper scripts and the extra test
registration (`conformance.cmake`) that the ctest suite uses. The tests
themselves are declared in `../CMakeLists.txt` and here.

## What may be committed

A fixture belongs in this directory only if it is under about 4 KB **and**
tests something asdf-cxx cannot write itself. Anything larger, and any
corpus of files, is fetched or generated instead:

- the ASDF standard's reference files come from
  `fetch-reference-files.sh`, which does a sparse, depth-1 clone of
  `asdf-format/asdf-standard` at the commit in `asdf-standard.pin`;
- everything in this directory that ends in `.asdf` is written by
  `make_fixtures.py`.

## Running the conformance tests

Both extra test families are opt-in through CMake cache variables, so a
plain `cmake -B build` still configures and passes:

```bash
python3 -m venv asdf-env
asdf-env/bin/pip install -r tests/requirements.txt
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DASDF_PYTHON=$PWD/asdf-env/bin/python \
  -DASDF_REFERENCE_FILES_DIR=$(tests/fetch-reference-files.sh)
cmake --build build && ctest --test-dir build --output-on-failure
```

`asdf==5.3.*` needs Python 3.11 or newer. `ASDF_REFERENCE_VERSIONS`
selects which standard versions the `ref-*` tests cover (all seven by
default).

## Helpers

- `expect-error.sh [-m <substring>]... <command>...` — the command must
  exit with status 1 and print `error:` on stderr; every `-m` substring
  must occur there too (case-insensitive, fixed string).
- `expect-output.sh <expected-file> <command>...` — `diff -u` of the
  command's stdout against a committed file in `expected/`.
- `check-header.sh <file> [options]` — assertions about a file's YAML
  head: the `#ASDF`/`#ASDF_STANDARD` lines, the root tag, the
  `core/ndarray` tag version, and strings that must or must not occur.
  `--root-tag` also insists that the root tag sits on the `---` line and
  that the head holds no bare `---`. The `header-*` and `ref-*-header`
  tests use it to pin down which standard version a written file
  declares and that its tags belong to that version.
- `python_check.py validate|compare` — opens files with the Python
  reference implementation. `validate` insists that the tree comes back
  fully deserialised (an `AsdfConversionWarning` is an error, so a file
  whose tags disagree with its declared version fails); `compare` checks
  that a copy holds the same arrays and scalars as its original.
- `read-check.cxx` builds `asdf-read-check`, which prints one line per
  array (`<path>: <datatype> [<shape>] <values>`). It is the oracle for
  the library's data access: the `values-*` tests diff its output
  against `expected/*.txt`. Values come from the library's
  `get_data_vector<T>()` where that accessor exists and from
  `get_data_bytes()` otherwise, so the expected outputs test the
  library's byte-order, offset and stride handling rather than a
  reimplementation of it. The output is platform-independent by
  construction (fixed float precision, NaN always unsigned, float16
  converted in software).

## Expected outputs

`expected/*.txt` are `asdf-read-check` outputs, one per file whose values
a `values-*` test pins: the reference files `basic`, `compressed`,
`endian`, `float`, `int` and `shared` (their content is the same in every
standard version, so one file covers all seven), `fixture-abc.txt` for
the three Python-written fixtures that share the same three arrays,
`demo.txt`, `bigendian.txt` and `float16.txt`. Regenerate one with

```bash
build/asdf-read-check <file> > tests/expected/<name>.txt
```

and check the values by eye before committing. `complex.asdf` has no
expected file on purpose: 400 complex numbers printed to full precision
would be 9 kB, more than all the other expected outputs together, and
`ref-*-complex-py-compare` plus the complex arrays in `asdf-demo-strided`
cover the same ground.

## Fixtures

Regenerate all of them with `make_fixtures.py` (see its docstring); the
exact bytes depend on the Python `asdf` version, which is recorded in
each file's `asdf_library` entry.

### Written by Python asdf

`padded.asdf`, `python-default.asdf` and `lz4.asdf` hold the same three
arrays: `a` (int64, 2×3×4, uncompressed), `b` (float64, 100 values,
zlib/lz4), `c` (int32, 200 values, bzip2/lz4).

| file | what it exercises |
|---|---|
| `padded.asdf` | ASDF 1.0.0; blocks with `allocated_size > used_size`; padding between the tree and the first block; no block index |
| `python-default.asdf` | current Python defaults: a recent standard version, `core/ndarray-1.1.0` tags, a `history` map with tagged `core/extension_metadata-1.0.0` entries, a block index |
| `lz4.asdf` | the standard `lz4` block encoding (needs the Python `lz4` package). asdf-cxx also writes its own LZ4 frame encoding under `lz4f`, which Python does not read |
| `float16.asdf` | a float16 block, as in Roman WFI level-2 products: readable and copyable even where the compiler has no `_Float16` |
| `float16-inline.asdf` | the same array inline, so that parsing its values does need `_Float16` |
| `structured.asdf` | a record array with per-field byte order, a sub-array field and a `[ucs4, 16]` field, as in Roman skycell reference files |
| `strings.asdf` | `ascii` and `ucs4` arrays as blocks and inline, including a non-BMP code point and a big-endian `ucs4` block whose 4-byte code units are swapped one at a time |
| `masked.asdf` | an ndarray `mask`, which asdf-cxx does not support |
| `bigendian.asdf` | one block shared by two views, negative and non-contiguous strides, Fortran order, big-endian data, and bool8. Note that the two views share one block on read but are written as two blocks by a copy |
| `roman-like.asdf` | the Roman WFI feature set in one file: foreign tags around nested ndarrays, tagged time/unit scalars, a float16 block, a `[ucs4, 16]` structured field, an astropy table with `core/column` entries, a YAML alias, `history.extensions`, standard 1.6.0 |

### Written as literal text

These have no blocks, so the fixture script writes them out verbatim;
that also pins the exact spelling that a copy has to preserve.

| file | what it exercises |
|---|---|
| `unknown-tags.asdf` | unknown tags on maps, sequences and scalars, and tagged scalar text (`1.0`, a timestamp, a unit) that must survive a copy unchanged |
| `alias.asdf` | YAML anchors and aliases; yaml-cpp resolves them on load and a copy expands them |
| `untagged-root.asdf` | a bare `---` root, which the standard allows |

### Deliberately broken files

`corrupt-tag.asdf`, `corrupt-checksum.asdf`, `corrupt-truncated.asdf`,
`bad-field-shape.asdf` and `not-asdf.asdf` are derived from
`python-default.asdf` by the fixture script. `bad-field-shape.asdf` gives
one array a structured field whose sub-array `shape` multiplies to 2^64,
which an unchecked element-size computation would turn into zero; it has no
block index, because the longer datatype moves every block.
`zero-size-datatype.asdf` (`[ascii, 0]`) and `bad-string-length.asdf`
(`[ucs4, abc]`) are written as literal text; they are datatypes a file may
claim but no writer produces, and each must be refused with an
`ASDF::error` naming the problem rather than with a yaml-cpp message or a
bounds-checked container throwing `vector`. The `error-*` tests run `asdf-ls` or `asdf-copy` on them
through `expect-error.sh`, which requires exit status 1 and an `error:`
message on stderr, so a crash does not count as passing.
`error-checksum` only runs when asdf-cxx was built with OpenSSL.

## Note on block checksums

The block header's MD5 covers the bytes as stored, that is, the
compressed payload; this is what Python asdf writes and verifies today.
Files written before that convention was settled — among them
asdf-standard's `compressed.asdf` reference files — checksum the
uncompressed data instead, and Python refuses to open them with
`validate_checksums=True`. asdf-cxx accepts either, so those reference
files still round-trip; the `ref-*-compressed-py-compare` tests pass
`--no-validate-checksums` for the same reason.
