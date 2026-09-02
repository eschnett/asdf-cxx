# Test fixtures

## `padded.asdf`

A small ASDF file with **padded** binary blocks, i.e. blocks whose
header says `allocated_size > used_size`. asdf-cxx itself never writes
padding (it always emits `allocated_size == used_size`), so this fixture
has to come from another writer; it guards against regressions of the
inverted `used_space` / `allocated_space` semantics that used to make
the reader abort on such files.

It contains three arrays, one per compressor that the C++ library
supports unconditionally:

| Array | Type      | Compression | allocated / used |
|-------|-----------|-------------|------------------|
| `a`   | `int64`   | none        | 320 / 192        |
| `b`   | `float64` | `zlib`      | 640 / 517        |
| `c`   | `int32`   | `bzp2`      | 512 / 386        |

The file is committed so that the test suite does not depend on Python
at build time. It was generated with Python `asdf` 5.3.1 and `numpy`
using the script below.

Two details are worth knowing before regenerating it:

- ASDF standard version 1.0.0 is requested deliberately. Newer versions
  tag arrays `core/ndarray-1.1.0` and add `core/extension_metadata-1.0.0`
  history entries, neither of which this reader recognises.
- `asdf` pads between the YAML document and the first block as well, and
  asdf-cxx expects the first block to start immediately after the `...`
  terminator. The script therefore strips that padding again and shifts
  the trailing block index offsets to match. The block index has to stay
  in the file: without it the last block ends exactly at EOF, which
  trips an unrelated reader bug (the failed magic-token read leaves the
  stream in a failed state).

```python
import sys

import asdf
import numpy as np

out = sys.argv[1]

tree = {
    "a": np.arange(24, dtype=np.int64).reshape(2, 3, 4),
    "b": np.linspace(0, 1, 100, dtype=np.float64),
    "c": np.arange(200, dtype=np.int32),
}
af = asdf.AsdfFile(tree, version="1.0.0")
af.set_array_compression(af["b"], "zlib")
af.set_array_compression(af["c"], "bzp2")
with asdf.config_context() as cfg:
    cfg.io_block_size = 64          # keep the padding small
    af.write_to(out, pad_blocks=True, version="1.0.0")

# asdf also pads between the YAML document and the first block; asdf-cxx
# expects the first block right after the "..." terminator, so remove
# that padding and shift the block index offsets accordingly.
data = open(out, "rb").read()
yaml_end = data.index(b"...\n") + 4
first = data.index(b"\xd3BLK")
shift = first - yaml_end
assert set(data[yaml_end:first]) <= {0}
data = data[:yaml_end] + data[first:]
i = data.index(b"#ASDF BLOCK INDEX\n")
head, tail = data[:i], data[i:].decode("ascii")
tail = "\n".join(
    "- %d" % (int(ln.strip()[2:]) - shift) if ln.strip().startswith("- ") else ln
    for ln in tail.split("\n"))
open(out, "wb").write(head + tail.encode("ascii"))
```
