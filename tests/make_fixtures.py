#!/usr/bin/env python
"""Regenerate the ASDF test fixtures in this directory.

asdf-cxx always writes blocks with allocated_size == used_size, no padding
between the YAML tree and the first block, and a block index, so files that
exercise the other cases have to come from another writer. This script uses
the Python reference implementation (https://pypi.org/project/asdf/):

    mamba create -p ./asdf-env -c conda-forge python asdf lz4   # or: pip install asdf lz4
    ./asdf-env/bin/python tests/make_fixtures.py

The fixtures are committed so that the test suite does not depend on Python.
"""

import os

import asdf
import numpy as np

here = os.path.dirname(os.path.abspath(__file__))


def make_tree():
    return {
        "a": np.arange(24, dtype=np.int64).reshape(2, 3, 4),
        "b": np.linspace(0, 1, 100, dtype=np.float64),
        "c": np.arange(200, dtype=np.int32),
    }


def compress(af):
    # Only compressors that asdf-cxx supports unconditionally in CI
    af.set_array_compression(af["b"], "zlib")
    af.set_array_compression(af["c"], "bzp2")


# padded.asdf: ASDF standard 1.0.0; padded blocks (allocated_size >
# used_size); padding between the YAML tree and the first block; no block
# index, so the last block ends exactly at the end of the file.
af = asdf.AsdfFile(make_tree(), version="1.0.0")
compress(af)
with asdf.config_context() as cfg:
    cfg.io_block_size = 64  # keep the padding small
    af.write_to(
        os.path.join(here, "padded.asdf"),
        pad_blocks=True,
        include_block_index=False,
        version="1.0.0",
    )

# python-default.asdf: exactly what a current Python asdf writes by default
# (a recent standard version, core/ndarray-1.1.0 tags, a `history` entry with
# tagged extension metadata, a block index).
af = asdf.AsdfFile(make_tree())
compress(af)
af.write_to(os.path.join(here, "python-default.asdf"))

# lz4.asdf: the standard lz4 encoding (block token "lz4"), which requires the
# Python `lz4` package to write. asdf-cxx also has its own LZ4 frame encoding
# (token "lz4f") that Python cannot read.
af = asdf.AsdfFile(make_tree())
af.set_array_compression(af["b"], "lz4")
af.set_array_compression(af["c"], "lz4")
af.write_to(os.path.join(here, "lz4.asdf"))

# Deliberately broken files, derived from python-default.asdf, to check that
# the tools report an error instead of crashing.
default = open(os.path.join(here, "python-default.asdf"), "rb").read()


def write(name, data):
    path = os.path.join(here, name)
    open(path, "wb").write(data)
    print("wrote", path)


# An unknown tag (same length, so the block offsets stay valid)
write(
    "corrupt-tag.asdf",
    default.replace(b"!core/ndarray-1.1.0", b"!core/ndarray-9.9.9", 1),
)

# A flipped byte inside the first block's payload; detected through the MD5
# checksum when asdf-cxx was built with OpenSSL
first = default.index(b"\xd3BLK")
payload = first + 4 + 2 + 48  # magic, header_size, header
corrupt = bytearray(default)
corrupt[payload + 8] ^= 0xFF
write("corrupt-checksum.asdf", bytes(corrupt))

# The file ends in the middle of the last block and has no block index
index = default.index(b"#ASDF BLOCK INDEX")
write("corrupt-truncated.asdf", default[: index - 100])

# A structured datatype whose sub-array shape multiplies to 2**64, so that an
# unchecked element size would come out as zero. Dropping the block index (the
# replacement changes every block offset) leaves the last block ending exactly
# at the end of the file, which the reader accepts.
bad_shape = default.replace(
    b"  datatype: int64\n",
    b"  datatype: [{name: f, datatype: uint8,"
    b" shape: [4294967296, 4294967296]}]\n",
    1,
)
assert bad_shape != default
write("bad-field-shape.asdf", bad_shape[: bad_shape.index(b"#ASDF BLOCK INDEX")])

# Not an ASDF file at all
write("not-asdf.asdf", b"This is not an ASDF file.\n")

print("wrote", os.path.join(here, "padded.asdf"))
print("wrote", os.path.join(here, "python-default.asdf"))
print("wrote", os.path.join(here, "lz4.asdf"))

# ---------------------------------------------------------------------------
# Fixtures for the standard-conformance tests (docs/standard-conformance-plan.md)
#
# These cover input that asdf-cxx cannot write itself. Each stays well under
# 4 KB; see tests/README.md for the policy.
# ---------------------------------------------------------------------------


def write_asdf(name, tree, version=None, storage=None, **kwargs):
    af = asdf.AsdfFile(tree, version=version)
    for key, mode in (storage or {}).items():
        af.set_array_storage(af[key], mode)
    path = os.path.join(here, name)
    af.write_to(path, version=version, **kwargs)
    print("wrote", path)


# float16.asdf / float16-inline.asdf: float16 exists only in core/ndarray-1.1.0
# (standard 1.6.0), and Roman WFI level-2 products use it. Reading and copying
# the block form must work on builds without `_Float16`; the inline form needs
# the type to parse its values.
half = np.arange(6, dtype=np.float16).reshape(2, 3)
write_asdf("float16.asdf", {"half": half}, storage={"half": "internal"})
write_asdf("float16-inline.asdf", {"half": half}, storage={"half": "inline"})

# structured.asdf: a record array with per-field byte order, a sub-array field
# and a [ucs4, 16] string field, as in Roman skycell reference files
record_dtype = np.dtype(
    [("a", ">u1"), ("b", "<f4"), ("c", ">i2", (2,)), ("name", "<U16")]
)
records = np.array(
    [
        (1, 1.5, (10, 11), "one"),
        (2, 2.5, (20, 21), "two"),
        (3, 3.5, (30, 31), "three"),
    ],
    dtype=record_dtype,
)
write_asdf("structured.asdf", {"records": records})

# strings.asdf: ascii and ucs4 arrays, as blocks and inline, including a
# non-BMP code point, an empty element (which the inline form has to quote)
# and a big-endian ucs4 block (whose 4-byte code units have to be swapped one
# at a time). The inline entries are copies, since Python
# asdf would otherwise write the same array once and refer to it by a YAML
# alias.
ascii_array = np.array(["ab", "cde", ""], dtype="S3")
ucs4_array = np.array(["αβ", "\U0001f600x", ""], dtype="<U2")
write_asdf(
    "strings.asdf",
    {
        "ascii": ascii_array,
        "ucs4": ucs4_array,
        "ucs4_be": ucs4_array.astype(">U2"),
        "ascii_inline": ascii_array.copy(),
        "ucs4_inline": ucs4_array.copy(),
    },
    storage={
        "ascii": "internal",
        "ucs4": "internal",
        "ucs4_be": "internal",
        "ascii_inline": "inline",
        "ucs4_inline": "inline",
    },
)

# rank0.asdf: a rank-0 array (`shape: []`). The core/ndarray schema puts no
# lower bound on `shape` and Python asdf writes `np.array(5.0)` as a block, so
# reading and copying it must work; only the inline form is unrepresentable,
# because `data` has to be a list.
write_asdf(
    "rank0.asdf",
    {"scalar": np.array(5.0), "int": np.array(-7, dtype="<i4")},
    storage={"scalar": "internal", "int": "internal"},
)

# masked.asdf: ndarray `mask`, which asdf-cxx refuses
write_asdf(
    "masked.asdf",
    {"masked": np.ma.array(np.arange(6), mask=[0, 1, 0, 0, 1, 0])},
)

# bigendian.asdf: one block shared by two views, negative and non-contiguous
# strides, Fortran order, and bool8
base = np.arange(24, dtype=">i4").reshape(4, 6)
write_asdf(
    "bigendian.asdf",
    {
        "base": base,
        "view": base[::-1, ::2],
        "fortran": np.asfortranarray(np.arange(6, dtype=">f8").reshape(2, 3)),
        "flags": np.array([True, False, True]),
    },
)

# roman-like.asdf: the Roman WFI feature set in one small file -- foreign tags
# with nested arrays, tagged scalars, a float16 block, a [ucs4, 16] structured
# field, an astropy-style table with core/column entries, and a YAML alias
def tagged_dict(tag, mapping):
    return asdf.tagged.TaggedDict(mapping, tag)


def tagged_string(tag, value):
    string = asdf.tagged.TaggedString(value)
    string._tag = tag
    return string


coordinate_frame = tagged_dict(
    "tag:stsci.edu:gwcs/frame2d-1.0.0",
    {"name": "detector", "axes_names": ["x", "y"]},
)
roman_tree = {
    "meta": tagged_dict(
        "asdf://stsci.edu/datamodels/roman/tags/wfi_image_meta-1.0.0",
        {
            "start_time": tagged_string(
                "tag:stsci.edu:asdf/time/time-1.1.0", "2027-01-01T00:00:00.000"
            ),
            "exposure_time": tagged_dict(
                "tag:stsci.edu:asdf/unit/quantity-1.1.0",
                {
                    "value": np.arange(4, dtype="<f4"),
                    "unit": tagged_string(
                        "tag:stsci.edu:asdf/unit/unit-1.0.0", "DN"
                    ),
                },
            ),
        },
    ),
    "err": np.arange(9, dtype=np.float16).reshape(3, 3),
    "skycells": np.array(
        [("r274dp63x31y80", 1.0), ("r274dp63x31y81", 2.0)],
        dtype=np.dtype([("name", "<U16"), ("ra", "<f8")]),
    ),
    "table": tagged_dict(
        "tag:astropy.org:astropy/table/table-1.0.0",
        {
            "columns": [
                tagged_dict(
                    "tag:stsci.edu:asdf/core/column-1.0.0",
                    {"name": "id", "data": np.arange(3, dtype="<i4")},
                )
            ],
            "colnames": ["id"],
        },
    ),
    "wcs": tagged_dict(
        "tag:stsci.edu:gwcs/wcs-1.2.0",
        {
            "name": "wcs",
            "steps": [
                tagged_dict(
                    "tag:stsci.edu:gwcs/step-1.1.0", {"frame": coordinate_frame}
                ),
                # the same frame object again, so that YAML writes an alias
                tagged_dict(
                    "tag:stsci.edu:gwcs/step-1.1.0", {"frame": coordinate_frame}
                ),
            ],
        },
    ),
}
af = asdf.AsdfFile(roman_tree, version="1.6.0")
af.write_to(os.path.join(here, "roman-like.asdf"), version="1.6.0")
print("wrote", os.path.join(here, "roman-like.asdf"))

# Text fixtures. These have no blocks, so they can be written literally; that
# also pins the exact spelling the reader has to preserve.
TEXT_FIXTURES = {
    # Unknown tags of every shape (map, sequence, scalar), plus tagged scalars
    # whose text must survive a copy byte for byte
    "unknown-tags.asdf": """\
#ASDF 1.0.0
#ASDF_STANDARD 1.5.0
%YAML 1.1
%TAG ! tag:stsci.edu:asdf/
--- !core/asdf-1.1.0
answer: !core/constant-1.0.0 42
custom: !<asdf://example.org/foo-1.0.0>
  x: 1
  y: [1, 2]
  nested: !<asdf://example.org/bar-1.0.0> {z: 3}
when: !time/time-1.1.0 2027-01-01T00:00:00.000
unit: !unit/unit-1.0.0 DN
ratio: !<asdf://example.org/scalar-1.0.0> 1.0
counts: !core/ndarray-1.0.0
  data: [1, 2, 3]
  datatype: int64
  shape: [3]
...
""",
    # A YAML comment inside the tree that looks like the header's
    # `#ASDF_STANDARD` line. Only the first two lines carry the declared
    # versions; this file declares 1.6.0 and a copy has to keep 1.6.0.
    "header-comment.asdf": """\
#ASDF 1.0.0
#ASDF_STANDARD 1.6.0
%YAML 1.1
%TAG ! tag:stsci.edu:asdf/
--- !core/asdf-1.1.0
#ASDF 9.9.9
#ASDF_STANDARD 1.0.0
counts: !core/ndarray-1.1.0
  data: [1, 2, 3]
  datatype: int64
  shape: [3]
...
""",
    # YAML anchors and aliases; a copy expands them
    "alias.asdf": """\
#ASDF 1.0.0
#ASDF_STANDARD 1.5.0
%YAML 1.1
%TAG ! tag:stsci.edu:asdf/
--- !core/asdf-1.1.0
a: &f {name: x, k: [1, 2]}
b: *f
c: [*f]
...
""",
    # A datatype of zero size. Every element then occupies no bytes, so the
    # array bounds check holds for any block; numpy normalises `S0` to `S1`,
    # so only a hand-written file can say this.
    "zero-size-datatype.asdf": """\
#ASDF 1.0.0
#ASDF_STANDARD 1.5.0
%YAML 1.1
%TAG ! tag:stsci.edu:asdf/
--- !core/asdf-1.1.0
empty: !core/ndarray-1.0.0
  data: ["", ""]
  datatype: [ascii, 0]
  shape: [2]
...
""",
    # A string datatype whose length is not a number
    "bad-string-length.asdf": """\
#ASDF 1.0.0
#ASDF_STANDARD 1.5.0
%YAML 1.1
%TAG ! tag:stsci.edu:asdf/
--- !core/asdf-1.1.0
bad: !core/ndarray-1.0.0
  data: [x]
  datatype: [ucs4, abc]
  shape: [1]
...
""",
    # An untagged root, which the standard allows
    "untagged-root.asdf": """\
#ASDF 1.0.0
#ASDF_STANDARD 1.5.0
%YAML 1.1
%TAG ! tag:stsci.edu:asdf/
---
counts: !core/ndarray-1.0.0
  data: [1, 2, 3]
  datatype: int64
  shape: [3]
...
""",
    # A core/software node missing the keys its schema requires. Now that
    # `history` is preserved, every core/software inside history.extensions
    # goes through the same constructor.
    "bad-software.asdf": """\
#ASDF 1.0.0
#ASDF_STANDARD 1.5.0
%YAML 1.1
%TAG ! tag:stsci.edu:asdf/
--- !core/asdf-1.1.0
tool: !core/software-1.0.0 {name: foo}
...
""",
    # Scalars whose YAML spelling decides their type. A quoted scalar is a
    # string however it looks; a plain `y` or `n` is a string too, because
    # that is what the reference implementation's parser resolves it to. An
    # inline array may omit `shape`, which is inferred from the data.
    "scalar-types.asdf": """\
#ASDF 1.0.0
#ASDF_STANDARD 1.5.0
%YAML 1.1
%TAG ! tag:stsci.edu:asdf/
--- !core/asdf-1.1.0
quoted:
  int: "42"
  float: "1.0"
  bool: "true"
  hex: "0x10"
  exponent: "1e3"
  no: "no"
plain:
  int: 42
  bool: true
  axes: [x, y, n]
noshape: !core/ndarray-1.0.0
  data: [1, 2, 3]
  datatype: int64
...
""",
}

for name, text in TEXT_FIXTURES.items():
    write(name, text.encode())
