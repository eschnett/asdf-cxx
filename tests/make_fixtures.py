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

# Not an ASDF file at all
write("not-asdf.asdf", b"This is not an ASDF file.\n")

print("wrote", os.path.join(here, "padded.asdf"))
print("wrote", os.path.join(here, "python-default.asdf"))
print("wrote", os.path.join(here, "lz4.asdf"))
