# Plan: ASDF standard conformance for asdf-cxx

Handoff plan. Implementation happens elsewhere; the "Verification" section at
the end is what the reviewer will run. Repository: `/Users/eschnett/src/asdf-cxx`
(C++17, version 8.0.0 unreleased). Read `CLAUDE.md` and `CODE.md` first.

## Status

| Phase | State | PR |
|---|---|---|
| 0 — test harness, CI, small cleanups | done | [#20](https://github.com/eschnett/asdf-cxx/pull/20) |
| 1 — data access correctness | done | [#21](https://github.com/eschnett/asdf-cxx/pull/21) |
| 1b — string datatypes | done | [#23](https://github.com/eschnett/asdf-cxx/pull/23) |
| 2 — version table, write options, CLI | done | [#24](https://github.com/eschnett/asdf-cxx/pull/24) |
| 3 — lossless reader | done | [#25](https://github.com/eschnett/asdf-cxx/pull/25) |
| 4 — local complex tags | not started | |
| 5 — documentation | not started | |

Deviations from this plan are recorded in the PR that made them, and the
ones that change what a later phase has to do are repeated here.

**Phase 0** (PR #20) lists nine deviations in its description. The three
that matter for later phases:

- Block checksums cover the stored (compressed) bytes; the reader now
  accepts either domain because asdf-standard's `compressed.asdf`
  reference files use the older uncompressed convention, which Python asdf
  5.3.1 itself cannot verify. `python_check.py` gained
  `--no-validate-checksums` for them (CODE.md §5.5/§7,
  `tests/README.md`).
- `ndarray::get_byteorder()` landed in Phase 0 rather than Phase 1, because
  `asdf-read-check` needs it.
- The `tests/` size budget as literally written ("no file over ~6 KB")
  cannot hold, because the plan also asks for `read-check.cxx`,
  `conformance.cmake`, `python_check.py` and `make_fixtures.py` to live
  there. It is met for the data: fixtures under 4 KB each.

The other six concern `--root-tag` (deferred to Phase 2), registering
`-py-compare` for all nine reference names instead of the little-endian
ones only, `asdf-read-check` gathering bytes itself until Phase 1 (now
done), gating `copy-compression-*` on `ASDF_PYTHON`, a second bug in
`demo/demo.cxx`, and `--expect-history` accepting
`asdf.tags.core.ExtensionMetadata`.

**Phase 1** (PR #21) deviations:

- `py-compare-float16` is deferred to Phase 2. `asdf-copy` still declares
  standard 1.2.0 while tagging arrays `core/ndarray-1.0.0`, whose schema
  forbids `float16`, so Python rejects the copy. `float16-{ls,copy,ls2}`
  and `values-float16` do pass, so the Roman requirement — float16 blocks
  readable and copyable on every build — is met.
- `ref-*-complex-values` is not registered and there is no
  `expected/complex.txt`: the file would be 9 kB, more than all other
  expected outputs together. The plan does not list `complex` among the
  expected-output files either. The complex byte-order fix is covered by
  `ref-*-complex-py-compare` and by big-endian complex arrays (block and
  inline) plus an `emit_scalar`/`parse_scalar` round trip in
  `asdf-demo-strided`.
- The `is_complex` trait moved to `byteorder.hxx` rather than staying in
  `datatype.hxx`, because `byteorder.hxx` is the header that needs it and
  is included by `datatype.hxx`. It now matches every `std::complex<T>`,
  not only floating-point ones.
- The overflow guards in `check_bounds` are hand-written checked
  arithmetic rather than `__builtin_*_overflow`, so they need no compiler
  extension.
- Added beyond the plan: `float16-inline-ls` and `values-float16-inline`
  (only where the build has `_Float16`), `py-compare-bigendian-inline`,
  and `py-validate-strided` / `strided-copy` / `strided-ls2` /
  `py-compare-strided`, which prove that Python asdf reads what
  `asdf-demo-strided` writes.

**Review of Phase 1** (2026-09-03) found two pre-existing gaps that the new
tests made visible. Neither blocked the merge; they are assigned to the
phases below and repeated in those sections.

- Structured field sizes are not overflow-checked. `field_t::type_size()`
  multiplies the sub-array extents in unchecked `size_t` arithmetic and the
  field-count loop in `convert_field_to_host` in unchecked `int64_t`. A file
  whose field shape multiplies to 2^64 gets an element size of 0, passes
  `check_bounds`, and is only stopped by a bounds-checked `vector` access
  (exit status 1, message `vector`). **Fixed in Phase 1b** (PR #23):
  `field_t::num_elements()` is the one checked count, and
  `tests/bad-field-shape.asdf` covers it.
- Complex NaN and infinity do not round-trip through inline form.
  `yaml_encode_complex` spells them the yaml-cpp way (`.nan+.nani`), which
  the `complex-1.0.0` schema does not allow (`nan`, `inf`, `-inf`, no
  leading dot) and which asdf-cxx's own `yaml_decode_complex` rejects with
  "Cannot parse complex number". `asdf-copy --array=inline` of the reference
  `complex.asdf` therefore produces a file neither asdf-cxx nor Python can
  read. Fix in Phase 4, which reworks complex emission; the reader should
  accept both spellings.

**Phase 1b** (PR #23) deviations:

- `roman-like-values` is not registered. `roman-like.asdf` is wrapped in
  foreign tags, which the reader still refuses (Phase 3); its `[ucs4, 16]`
  field is covered by `tests/structured.asdf` instead. The test moves to
  Phase 3 together with the rest of that fixture.
- `bad-field-shape.asdf` is derived from `python-default.asdf` as the plan
  asks, but with the block index dropped: the longer `datatype:` line moves
  every block, and the reader scans sequentially anyway, so without an index
  the last block still ends exactly at the end of the file.
- `structured.asdf` gets no `--array=inline` copy, because Python asdf 5.3
  with numpy 2 cannot read inline structured arrays (CODE.md §11), so
  `py-compare` could not check it.
- `ref-*-{ascii,unicode_bmp,unicode_spp,structured}-header` is not switched
  on: `check-header.sh` is registered by Phase 2 for every reference name at
  once.
- Added beyond the plan: a big-endian `ucs4` block (`ucs4_be`) in
  `strings.asdf`, so that a little-endian host has to swap the 4-byte code
  units; `values-strings2` and `values-structured2` on the copies;
  `py-compare-strings-inline`; and `ascii`/`ucs4` cases in
  `asdf-read-check`'s typed-accessor path, so the expected outputs exercise
  the two new accessors rather than a reimplementation.
- `get_data_vector<std::u32string>()` also strips trailing null code units,
  which the plan only asks for on the `ascii` side; that is what numpy's `U`
  dtype means and what `asdf-read-check` already printed.
- `emit_scalar` refuses to emit an `ascii` element with a byte >= 0x80
  instead of writing invalid UTF-8 into the YAML. Such a block is malformed
  input, and `asdf-copy` without `--array=inline` still copies it verbatim.
- `datatype_t(scalar_type_id_t)` now rejects `id_ascii`/`id_ucs4`; string
  datatypes go through the new two-argument constructor. `field_t` gained a
  public `num_elements()`, and structured `datatype_t::type_size()` also
  checks the sum of the field sizes.
- Nothing was removed from the error-message contract: it never had
  `ascii`/`ucs4` rows.

Review of Phase 1b found four non-blocking issues, all fixed in the same PR:

- A **zero-size datatype** was the second route to the `vector` failure the
  Phase 1 review described: `[ascii, 0]` or `[ucs4, 0]` (and, already on
  `main`, an empty field list `datatype: []`) gives `type_size() == 0`, so
  every element occupies no bytes, `check_bounds` holds against any block,
  and `get_data_bytes` threw `std::out_of_range("vector")` from its own
  result. `ndarray::check_bounds` now rejects a zero-size datatype for an
  array that has elements; `tests/zero-size-datatype.asdf` covers it.
- `yaml_decode(scalar_type_id_t)` gained the `ASDF_CHECK(node.IsScalar())`
  the plan asked for, and names the two-element form when it sees a bare
  `ascii` / `ucs4` type name.
- `yaml_encode(scalar_type_id_t)` gained the explicit `id_ascii`/`id_ucs4`
  cases its three siblings had, pointing the caller at
  `datatype_t::to_yaml()`.
- `[ucs4, abc]` reported yaml-cpp's `bad conversion`; the length is now
  parsed defensively and reported at datatype level
  (`tests/bad-string-length.asdf`).

**Phase 2** (PR #24) deviations:

- `content_requirements::minimum_version()` returns the **lowest** known
  standard version (1.0.0) when the tree needs no `float16`, not
  `default_standard_version()` as the sketch's comment says. Folding the
  default floor into the content minimum would make the "requires" check
  reject every legitimate write below 1.2.0, including `asdf-copy` of the
  1.0.0 and 1.1.0 reference files. `minimal` is still
  `max(default_standard_version(), minimum_version())`, exactly as specified.
- `--standard-version=input` on a file whose declared version this library
  does not know falls back to `minimal` rather than raising an error naming
  the supported range. The two behaviours the plan asks for are not
  distinguishable: `asdf-copy`'s default *is* `input`, so an explicit
  `--standard-version=input` and the default reach `asdf::write` identically.
- Test names: the plan's `copy-X` become `X-copy` (`demo-1.6.0-copy`,
  `demo-1.0.0-copy`, `demo-latest-copy`, `python-default-minimal-copy`,
  `nonstandard-allowed-copy`, `float16-minimal-copy`,
  `float16-1.0.0-allowed-copy`), and `error-copy-nonstandard` becomes
  `error-nonstandard-copy`. `copy-` contains `py-`, and these tests are
  registered unconditionally, so a leading `copy-` would break the
  `ctest -N | grep -c 'ref-\|py-'` check for a plain configure. Phase 1b
  named `strided-copy` for the same reason.
- The Verification line `grep -c '^---' demo.asdf` is **2**, not 1: the block
  index is a second YAML document and starts with `---`, exactly as the files
  Python asdf writes do. Inside the YAML head (everything up to `...`) the
  count is 1 and the line is `--- !core/asdf-1.1.0`; that is what
  `check-header.sh --root-tag` enforces, and it is the rule the plan means.
- `roman-like-copy` is still not registered: `roman-like.asdf` is wrapped in
  foreign tags, which the reader refuses until Phase 3.
  `ref-1.5.0/1.6.0-basic-history` likewise waits for Phase 3.
- `ref-<version>-shared-strides` is registered here (for 1.6.0, as written).
  The plan lists it under Phase 0 "Extra", but it never was registered, and
  it belongs with the other header assertions: it proves that `offset` and
  `strides` are still written where they are *not* the defaults.
- `version_t`'s members are named `major`/`minor`/`patch` as specified;
  `version.hxx` `#undef`s the `major`/`minor` function-like macros that
  `<sys/sysmacros.h>` defines on some systems and that would otherwise
  mangle them.
- `asdf(rs, node)` no longer accepts a `core/asdf-1.2.0` root tag. No version
  of the standard defines that tag; the accepted set is now exactly what the
  version table holds (`core/asdf-1.0.0` and `-1.1.0`).
- `asdf::write(filename, options)` resolves the version and checks the
  content *before* opening the output file, so a refused write no longer
  truncates an existing file.
- `standard_info_t::history_entry_tag` and `extension_metadata_tag` are in
  the table but not used yet; Phase 3 is what writes `history`.
- `yaml_encode(const complex<T> &)` still emits the tag verbatim, so inline
  complex *arrays* keep their `!<tag:...>` spelling. Only
  `complex_entry::to_yaml` moved to `w << value` and the local tag, which is
  what 2a asks for; the array path is Phase 4's `emit_scalar` rework.
- `asdf-ls` prints `standard version: <x>` (or `unknown`) under `Project:`.
- Added beyond the plan: `header-python-default2`, `header-padded2`,
  `header-python-default-minimal`, `py-validate-python-default-minimal`,
  `header-float16-minimal`, `py-validate-float16`, and
  `error-compression-level` (the `--compression-level=10` range check).

Review of Phase 2 found four issues, all fixed in the same PR:

- **A rank-0 *block* array is not nonstandard.** The first version followed
  the plan's "empty shape → nonstandard" literally, so `asdf-copy` refused
  files Python asdf writes (`np.array(5.0)` is a block array with
  `shape: []`, and `core/ndarray` puts no lower bound on `shape`). Only the
  inline form is unrepresentable. The rule now tests
  `block_format == inline_array` as well, the plan's 2b wording and decision 3
  above are corrected, and `tests/rank0.asdf` covers both sides.
- **`#ASDF_STANDARD` was picked up from any line of the YAML head**, so a
  YAML comment inside the tree overrode the real header and the preserving
  copy silently downgraded the file and its tags. Only the first two lines
  count now, which is where the standard puts them.
  `tests/header-comment.asdf` covers it.
- `asdf::write(filename)` ran the requirements pre-pass twice. A private
  `write_prepared(os, standard, options)` now takes the resolved
  `standard_info_t`, so each `write` walks the tree once.
- `--compression-level=N` accepted only a single digit, rejecting an in-range
  `05`; it now parses the number and range-checks it. CODE.md's known gap 7
  described the fixed behaviour and was reworded.

**Phase 3** (PR #25) deviations:

- `header-untagged-root-copy` asserts standard **1.5.0**, not the 1.2.0 the
  plan names. `untagged-root.asdf` declares 1.5.0 on line 2, and since Phase 2
  `asdf-copy` preserves the input's version; the root tag is
  `core/asdf-1.1.0` either way, which is what the item is about. The same
  applies to `unknown-tags.asdf` (also 1.5.0).
- `py-validate-unknown-tags2` and `py-validate-roman-like2` use a new
  `--expect-unknown-tags` flag rather than `--allow-unknown-tags`. The plan
  asks separately that "opening `unknown-tags2.asdf` with
  `AsdfConversionWarning` as error must raise"; that is not a check a passing
  test can express directly, because `--allow-unknown-tags` has to switch the
  escalation off (every foreign tag raises the same warning, so
  `py-compare-roman-like` could not open either file otherwise). The new flag
  implies `--allow-unknown-tags` and additionally requires that Python asdf
  hand back at least one undeserialised tagged node, which is the same proof
  stated positively.
- `python_check.py compare` gained `--allow-unknown-tags`, and
  `compare_values` falls back to comparing `vars(a) == vars(b)` for two
  objects of the same type that are unequal. Python asdf deserialises
  `!core/constant-1.0.0` into an `asdf.tags.core.Constant`, which has no
  `__eq__`, so `unknown-tags.asdf` would otherwise compare by identity and
  never match.
- **A bare `y` or `n` was read as a boolean.** yaml-cpp follows the YAML 1.1
  type repository here and PyYAML does not, so `axes_names: [x, y]` in
  `roman-like.asdf`'s gwcs frame came back as `["x", true]` and
  `py-compare-roman-like` failed. `make_entry` now skips the boolean
  conversion for a one-character `y`/`n` scalar. This is a reader bug the new
  fixture exposed rather than something the plan lists.
- **`copy`, `ls`, `ls2`, `ls3` and `compare-demo` had no `DEPENDS`.** They
  read files the `demo` and `copy` tests write, and only ever passed because
  ctest happened to schedule them in registration order; the phase's extra
  tests changed that schedule and they failed under `-j8`. Fixed in
  `CMakeLists.txt`.
- `error-missing-file` is registered beyond the plan's list, for the
  `Cannot open` row of the contract table. `asdf-ls` opens the file itself
  before handing it to `asdf(filename)`, so the check exists in both places.
- A tagged scalar spelled `~` round-trips as `!<tag> "~"`: the text is
  preserved and the file is stable under further copies, but yaml-cpp quotes
  `~` to keep it a string, so the null-ness of a tagged null is lost. No ASDF
  schema uses one; recorded as known gap 9 in CODE.md.
- `ref-1.5.0/1.6.0-basic-history` is registered here. The plan lists it under
  Phase 0 "Extra", but `history` was dropped on read until this phase.
- `software(rs, node)` now requires `name` and `version` to be present, as
  3c asks. Nothing else in 3c changed the accepted root tags beyond what is
  written there.

Review of Phase 3 found three issues, all fixed in the same PR:

- **An inline array without `shape` was refused.** The new streamed-array
  check called `node["shape"].IsSequence()` on a const node, and yaml-cpp's
  const `operator[]` returns an invalid node for a missing key whose type
  queries throw. `shape` is optional for an inline array (it is inferred from
  the data), so a legal file failed with exactly the kind of yaml-cpp message
  this phase set out to eliminate. `IsDefined()` is now asked first.
- **The two new `software` checks never fired for a missing key**, for the
  same reason: the invalid node threw before the `ASDF_CHECK` could report
  anything. This matters more now that `history` is preserved, because every
  `core/software` inside `history.extensions` goes through this constructor.
  `tests/bad-software.asdf` covers it.
- **Quoted strings were retyped on a copy** (`"42"` to `42`, `"1.0"` to `1`,
  `"true"` to `true`, `"no"` to `false`). This is pre-existing on `main`, but
  it is the same losslessness problem as the `y`/`n` case above and the
  information was available: yaml-cpp reports the non-specific tag `!` for a
  quoted scalar and `?` for a plain one, and `is_trivial_tag` lumped them
  together. `make_entry` now reads a `!`-tagged scalar as a string, which
  closes the whole class. `tests/scalar-types.asdf` covers it.

One neighbouring case is **deliberately left for Phase 4**: a plain float
scalar written `1.0` is emitted as `1`, so Python asdf reads the copy's value
as an `int`. The `yaml_encode(float32_t/float64_t)` overloads that spell it
also spell the elements of inline float arrays, so changing them here would
change the bytes of every file this library writes and would split a spelling
decision across two phases. Phase 4 already reworks scalar emission
(`emit_node`, the `nan`/`inf` spelling); this belongs with it. Recorded as
known gap 9 in CODE.md.

---

## Context

A conformance review against ASDF standard 1.0.0 to 1.6.0 and the Python
reference implementation (asdf 5.3.1) found:

- **Writer.** Output is valid ASDF 1.2.0 for standard datatypes, but float16
  and complex32 are written under `core/ndarray-1.0.0`, which does not allow
  them (float16 exists only in `ndarray-1.1.0` = standard 1.6.0; complex32,
  int128, uint128 exist in no version), and rank-0 arrays are unrepresentable
  (inline `data` must be a list). `#ASDF_STANDARD` is a compile-time constant.
  Python asdf selects its tag set from that line: a file whose tag versions
  disagree with the declared version opens, but its arrays come back as raw
  dicts. Declared version and tags must move together.
- **Reader.** Any unknown tag aborts the file, and `history` is dropped; the
  standard requires unknown versions to be preserved undeserialised, which
  Python does. Recognised-but-unsupported features (exploded files,
  streaming, string datatypes, masks, inline nulls, missing external files)
  surface as yaml-cpp exceptions or misleading messages.
- **Data access bug.** `ndarray::get_data_vector<T>()` ignores byte order,
  offset and strides. Verified: a big-endian int32 array reads back as
  `0, 16777216, ...`; a Fortran-order 2x3 array as `0 3 1 4 2 5`. No
  `get_byteorder()` accessor exists; `check_shape()` is dead code.
- **Cosmetics.** `---` and the root tag on separate lines (standard examples:
  `--- !core/asdf-1.1.0`); `offset: 0` and default strides always written;
  inline complex scalars use verbatim `!<tag:...>` while everything else uses
  local `!core/...` tags.
- **Dead code.** `table`/`column` are unreachable; `core/table-1.0.0` was
  dropped from the standard's core manifest in 1.6.0.

Decisions confirmed by the maintainer on 2026-09-02:

1. Default written version for new files: the **lowest that fits the
   content** (1.2.0 normally; 1.6.0 when float16, or complex32 with opt-in,
   is present). Tags follow the version.
2. **`asdf-copy` preserves the input file's declared version** by default
   (the standard's "preserve" mode) when the library knows that version,
   falling back to lowest-that-fits otherwise; explicit flags override.
3. Nonstandard content (int128, uint128, complex32, inline rank-0 arrays):
   **refuse unless opted in** via `allow_nonstandard`. A rank-0 array stored
   as a block is standard and must keep copying.
4. **Remove** the table/column code.
5. Unknown tags: **preserve silently** and round-trip. No warnings, no callback
   (one can be added later as a member of a read-options struct).
6. **No large example files in the repository.** CI fetches the standard's
   reference files at a pinned commit; committed fixtures stay under ~4 KB
   each and only cover what the library cannot write itself.
7. **Roman Space Telescope files must stay readable** (see next section).

## Roman Space Telescope compatibility (constraint on the whole plan)

Roman WFI products are ASDF files written by Python asdf via
`roman_datamodels`; their schemas live in `spacetelescope/rad`. Checked
against the current RAD schemas (700 files):

| Roman feature | Where | Plan consequence |
|---|---|---|
| `float16` arrays | `wfi_image` (L2) `err`, `var_poisson`, `var_rnoise`, `var_flat` since rad 1.6.0; written as `core/ndarray-1.1.0`, standard 1.6.0 | **Required.** float16 blocks must be readable and copyable on every build, including builds without `_Float16`; writing float16 selects 1.6.0 (never "nonstandard"). |
| `[ucs4, N]` string fields (N = 16, 30, 40) in structured arrays | skycells reference files, L1 guide-window products, catalog tables | **Required.** String datatypes must be supported at least at byte level (blocks) so files read and copy; the plan implements them fully (Phase 1b). Refusing them, as the first draft did, would make these files unreadable. |
| `complex128` arrays | 15 schema uses | Supported today. |
| `bool8`, `uint8/16/32`, `int32`, `float32/64` | everywhere | Supported. |
| Foreign tags: `asdf://stsci.edu/datamodels/roman/tags/*`, `tag:stsci.edu:gwcs/wcs-*`, `tag:stsci.edu:asdf/transform/*`, `tag:astropy.org:astropy/table/table-1.*`, `tag:astropy.org:astropy/units/unit-1.*`, `tag:stsci.edu:asdf/unit/quantity-1.*`, `unit/unit-1.*`, `time/time-1.*`, coordinate frames | every product | Preserved verbatim by Phase 3; nested `core/ndarray` inside them (quantities, table columns) are parsed and their blocks copied. |
| `core/column-1.0.0` maps inside astropy tables (asdf-astropy still uses this tag URI) | catalogs, reference tables | Handled as a preserved tag containing an ndarray; better than the deleted dead code. |
| Tagged scalars (`!time/time-1.1.0 2027-…`, `!unit/unit-1.0.0 DN`) | metadata | Phase 3 must keep the scalar text verbatim (no re-typing `1.0` → `1`, no forced quoting). |
| `history.extensions` with many `extension_metadata` entries | every product | Preserved (Phase 3). |
| Large blocks (4088x4088 float32, L1 ramps) | L1/L2 | Lazy loading and one-block-at-a-time copying already exist; bounds checks use header sizes, no extra loads. |
| YAML anchors/aliases (gwcs may share frame objects) | gwcs trees | yaml-cpp resolves aliases on load; copies expand them (equal content, identity lost). Add a fixture to prove it. |
| Not used by Roman: ndarray `mask` key (Roman's `mask` properties are ordinary arrays), streaming, exploded files, inline nulls | | These remain refused with clear messages. |

A real Roman file cannot be committed (size, access), so Phase 0 adds a
small `roman-like.asdf` fixture written with plain Python asdf that combines
exactly these features (float16 block, `[ucs4, 16]` structured field,
foreign tagged maps with nested ndarrays, a `core/column` inside an
astropy-table-tagged map, tagged time/unit scalars, `history.extensions`,
standard 1.6.0). If the reviewer has a real Roman file locally, `asdf-ls`,
`asdf-copy` and the Python `compare` on it are the final check.

Facts the design relies on (verified against the `asdf_standard` manifests
and schemas, the asdf-standard repository, RAD, and yaml-cpp 0.8 sources):

| Standard | core/asdf | core/ndarray | float16 | history.extensions |
|---|---|---|---|---|
| 1.0.0, 1.1.0 | 1.0.0 | 1.0.0 | no | no (list of history_entry only) |
| 1.2.0 to 1.5.0 | 1.1.0 | 1.0.0 | no | yes (extension_metadata-1.0.0) |
| 1.6.0 | 1.1.0 | 1.1.0 | yes | yes |

- `software-1.0.0` and `complex-1.0.0` are identical in every version.
  String datatypes `[ascii, N]` (N bytes) and `[ucs4, N]` (4N bytes, 4-byte
  code units in the array's byte order) exist in every version.
- yaml-cpp: `Node::Tag()` returns the full resolved URI after `%TAG`
  expansion (`!foo` with no matching `%TAG` arrives literally as `!foo`; `!!x`
  as `tag:yaml.org,2002:x`; plain scalars `?`, quoted scalars `!`).
  `EmitFromEvents::EmitProps` always emits node tags verbatim (`!<...>`), so
  local tags need the Emitter path. `Emitter << BeginDoc` always writes
  `---\n`, so a same-line root tag requires the writer to write `--- ` itself.
- Reference files (`asdf-format/asdf-standard`, `reference_files/<version>/`,
  versions 1.0.0 to 1.6.0; 22 to 52 KB per version): `anchor` (YAML alias),
  `basic` (arange(8) int64), `complex` (four block arrays c8/c16 in both byte
  orders with nan/inf/-0.0), `compressed` (zlib, bzp2), `endian` (arange(42)
  as `>i4` and `<i4`), `float` (special values, both orders), `int` (limits,
  both orders), `scalars`, `shared` (two views of one block: `subset` has
  `offset: 8, strides: [16]` = `[1,3,5,7]`), `structured` (has a field with
  `datatype: [ascii, 3]`), `ascii`, `unicode_bmp`, `unicode_spp` (string
  datatypes), `exploded` + `exploded0000.asdf` (external block,
  unsupported), `stream` (streamed block, unsupported). 1.5.0/1.6.0 files
  carry `history/extensions`; 1.0.0 files have no history. Python asdf reads
  all versions.
- `stream.asdf` fails in the block scan (flag bit 1) before the tree is
  built, so the streaming message must come from `read_block`'s flags check.
- `compression.asdf` from the demo contains an `lz4f` block that Python cannot
  read; Python checks of it go through an `asdf-copy --compression=lz4`.
- `demo.asdf` has `eta: {$ref: "#/group/1"}`, a dangling reference.
- `asdf-copy` prints usage without an `error:` prefix for bad options.

Conventions to keep: three-constructor pattern (value / `(rs, node)` /
`(copy_state, other)`) plus `to_yaml(writer&)`; `ASDF_CHECK`/`ASDF_ERROR`
for input-caused errors, `assert` only for invariants; clang-format on touched
files; negative tests through `tests/expect-error.sh`; fixtures only via
`tests/make_fixtures.py`; once `version.hxx` exists, never hard-code a tag
string elsewhere.

## Implementer notes (read before starting)

- **Workflow.** One pull request per phase against `main`; branch protection
  requires the four CI jobs, so nothing lands without green CI. Do not merge;
  the maintainer merges after the reviewer has run the Verification section.
  Put in each PR description: the tests switched on, the tests added, the
  verification commands with their output, and every deviation from this
  plan with its reason.
- **Where this plan lives.** Add it to the repository in the Phase 0 PR as
  `docs/standard-conformance-plan.md` (verbatim, then keep it current: tick
  off phases, record deviations). It is the shared reference for
  implementer, reviewer and maintainer.
- **Precedence.** The Roman compatibility constraints and the error-message
  contract win over any other wording in this plan. Never turn a recognised
  datatype or a foreign tag into a whole-file error. When a "verify" item
  about yaml-cpp or Python turns out differently than assumed, adapt the
  mechanism (for example post-process the header line instead of relying on
  emitter behaviour), note it in the PR and in CODE.md.
- **Environment.** `asdf==5.3.*` needs Python 3.11 or newer; the maintainer's
  macOS machine has a 3.9 system `python3`, so create the environment with
  mamba (`mamba create -p ./asdf-env -c conda-forge python=3.12 asdf lz4`) or
  another 3.11+ interpreter plus `pip install -r tests/requirements.txt`.
  Network is needed once for `tests/fetch-reference-files.sh` and pip. The
  repository's `build/` directory is stale and fails to reconfigure; use a
  fresh build directory. Run the project's clang-format on every touched
  C++ file. Compression libraries on the maintainer's machine: zstd, lz4,
  bzip2, zlib, OpenSSL are present; blosc and blosc2 are not (CI has them),
  so guard blosc usage in demos with `have_compression_blosc()`.
- **Do not commit** reference files, `asdf-standard/`, `asdf-env/`, any
  generated `.asdf` output, or changes to `TODO.md` (the maintainer's private
  file). Committed fixtures come only from `tests/make_fixtures.py` and stay
  under ~4 KB each.
- **Definition of done** for a phase is its acceptance list plus the
  Verification bullet for that phase; add tests in the same PR as the
  behaviour they check so CI stays green at every merge.

## Error-message contract (shared by library and tests)

Tests assert case-insensitive substrings of stderr. The library must use
these words; the exact sentence is the implementer's choice.

| Situation | Where raised | Required substrings |
|---|---|---|
| Streamed block (header flag bit 1) | `ndarray::read_block` flags check | `stream`, `not supported` |
| `source` is a string (exploded file) | `ndarray(rs, node)` before `yaml_decode(source)` | `exploded`, `external`, `not supported` |
| `shape` contains `*` | before `yaml_decode(shape)` | `stream`, `not supported` |
| `mask` key present | `ndarray(rs, node)` | `mask`, `not supported` |
| inline `null` element | `parse_inline_array_nd` rank-0 | `mask`, `not supported` |
| File cannot be opened (main or external reference) | `asdf(filename)`, `reader_state::resolve_reference` | `Cannot open` |
| float16/complex32/int128 value parsing or emission on a build without the type | `parse_scalar`/`emit_scalar` | the type name, `this build` |
| Nonstandard content refused | `asdf::write` | `nonstandard`, the offending item (e.g. `rank-0`, `int128`) |
| Content needs a higher version than explicitly requested | `asdf::write` | `requires`, both version strings |
| Unknown or unsupported `--standard-version` | `set_standard_version` / `asdf::write` | `standard version` |
| Unknown-tagged node with integer `source` on write | `group::to_yaml` | the tag, `block` |
| Bad `asdf-copy` option | `utils/copy.cxx` check lambda | `error:` prefix plus the option text |

Phases are separately reviewable PRs, in this order. Phase 0 first because it
supplies the harness the later phases are verified with; it registers only
tests that pass today and lists the ones each later phase switches on.

---

## Phase 0: test harness, CI, small cleanups

### Reference files, not committed

- `tests/asdf-standard.pin`: one line, the pinned asdf-standard commit (the
  only place it lives; e.g. `786797692c6185cfbacfaf0e1de3b55fb2f33e9e`, main
  on 2026-06-23; re-check with `git ls-remote` when implementing).
- `tests/fetch-reference-files.sh [dest]`: sparse (`reference_files` only),
  depth-1 fetch of that commit into `dest` (default `./asdf-standard`); no-op
  when already at the pin; prints `dest/reference_files` so it can be used as
  `-DASDF_REFERENCE_FILES_DIR=$(tests/fetch-reference-files.sh)`. Add
  `asdf-standard/` and `asdf-env/` to `.gitignore`.
- CMake cache variables (in a new `tests/conformance.cmake`, included from
  `CMakeLists.txt` after the existing tests): `ASDF_REFERENCE_FILES_DIR`
  (PATH, default empty) and `ASDF_REFERENCE_VERSIONS` (default all seven).
  Missing version directories warn and are skipped. Without the variable, no
  `ref-*` test is registered.

### Python in CI, optional locally

- `tests/requirements.txt`: `asdf==5.3.*`, `numpy>=2,<3`, `lz4>=4`.
- CMake cache variable `ASDF_PYTHON` (FILEPATH). Helper functions in
  `conformance.cmake`: `asdf_add_python_test(name args...)` registers
  `${ASDF_PYTHON} tests/python_check.py args...` only when set;
  `asdf_test_depends(name deps...)` is a no-op for unregistered tests.
- `tests/python_check.py` (argparse, exit 0/1, one summary line per file):
  - `validate <file>... [--standard X.Y.Z] [--root-tag T] [--ndarray-tag T]
    [--allow-unknown-tags] [--expect-history]`: header greps on the raw bytes
    up to `...`; `asdf.open(lazy_load=False, memmap=False,
    validate_checksums=True)` with `AsdfConversionWarning` escalated to an
    error (the "tags did not match the declared version" signal); every
    `NDArrayType` must convert to a real `numpy.ndarray`; no
    `TaggedDict/TaggedList/TaggedString` unless `--allow-unknown-tags`; with
    `--expect-history`, `history.extensions` is a non-empty list of
    `ExtensionMetadata`.
  - `compare <original> <copy> [--skip <path>]...`: never
    `resolve_references()`; collect `{path: value}` for arrays and plain
    scalars (descending into tagged dicts/lists too), skipping
    `asdf_library`, `history` and references; identical path sets; per array
    equal shape, equal dtype after `newbyteorder('=')` (structured via
    normalised `.descr`), `numpy.testing.assert_array_equal` (NaN-aware,
    string dtypes included). `--skip` for inline structured arrays, which
    Python 5.3 with numpy 2 cannot read.

### Shell helpers

- Extend `tests/expect-error.sh` with `[-m <substring>]...` (case-insensitive
  fixed-string match on stderr, each must be present; no `-m` keeps today's
  behaviour).
- New `tests/check-header.sh <file> [--standard X.Y.Z] [--root-tag T]
  [--ndarray-tag T] [--present <str>]... [--absent <str>]...`: works on the
  YAML head (`sed -n '1,/^\.\.\.$/p'`); asserts line 1 is `#ASDF 1.0.0`, line
  2 the given standard, exactly one line matching `^--- !core/asdf-` and no
  bare `---` line (the same-line root tag rule; switched on in Phase 2),
  every `!core/ndarray-X` occurrence equals the given tag, and the
  present/absent fixed strings.
- New `tests/expect-output.sh <expected-file> <command>...`: `diff -u` of
  stdout against a committed expected file.

### `asdf-read-check` tool (data-access oracle)

`tests/read-check.cxx`, target `asdf-read-check` (not installed; add to the
coverage target list). Walks the tree in deterministic order (descending into
preserved tagged groups) and prints one line per ndarray: `<path>:
<datatype> [<shape>] v0 v1 ...` from `get_data_bytes()`/`get_data_vector<T>`
by `scalar_type_id` (structured arrays: one `(f0, f1, ...)` tuple per record;
strings quoted, ucs4 as UTF-8; int128/uint128/complex32 as `(skipped)`).
float16 is printed via a small software half-to-float conversion inside the
tool so the output does not depend on `_Float16` availability. Portable
formatting: integers plain, float32 `%.9g`, float64 `%.17g`, NaN always
`nan`, complex `(re,im)`. Expected outputs in `tests/expected/`: one per
reference name (`basic`, `compressed`, `endian`, `float`, `int`, `shared`,
`ascii`, `unicode_bmp`, `unicode_spp`, `structured`; version-independent),
`fixture-abc.txt` (padded, python-default, lz4 share the same arrays),
`demo.txt`, and one per new fixture that has arrays. Generate them after
Phase 1 and check by eye against the values listed in Context (e.g.
`endian.txt` must show `big` and `little` both as `0 1 2 ... 41`;
`shared.txt` must show `subset: int64 [4] 1 3 5 7`).

### Test registration (`tests/conformance.cmake`)

Lists: `ASDF_REF_SUPPORTED = anchor basic complex compressed endian float int
scalars shared ascii unicode_bmp unicode_spp structured` (the last four are
switched on in Phase 1b); `ASDF_REF_UNSUPPORTED = exploded stream`;
`ASDF_REF_VALUES` = every supported name except `anchor` and `scalars`.
For each version directory `v` and supported name `n`: `ref-<v>-<n>-ls`,
`-copy`, `-ls2`, `-header` (`--standard <v>` and the root/ndarray tags that
belong to `v`, since `asdf-copy` preserves the input version; plus `--absent
"offset: 0"`), `-py-compare`, and for names in `ASDF_REF_VALUES` `-values`
(original) and `-values2` (copy). For each unsupported name:
`ref-<v>-<n>-unsupported` via `expect-error.sh -m <contract substrings>` on
`asdf-ls`. Extra: `ref-1.5.0/1.6.0-basic-history` (`--present history:
--present extension_metadata` on the copy), `ref-1.6.0-shared-strides`
(`--present "offset: 8" --present "strides: [16]"`). Python tests on
existing outputs: `py-validate-demo`, `py-validate-demo2` (`--standard 1.2.0
--root-tag core/asdf-1.1.0 --ndarray-tag core/ndarray-1.0.0`),
`py-compare-demo`, `py-validate-external` (external.asdf, metadata.asdf),
`py-compare-padded/python-default/lz4`, `py-validate-python-default2
--expect-history`, and `copy-compression-lz4` (`asdf-copy --compression=lz4
compression.asdf`) + `py-validate-compression-lz4` + `copy-compression-none`
+ `py-compare-compression`.

In Phase 0 register only what passes today: `ref-*-{ls,copy,ls2}` for the
nine names readable today, `ref-*-unsupported` (including the four string
files for now) without `-m`, `py-validate-demo/demo2`, `py-compare-*` for
the little-endian files, `py-validate-external`, the compression checks, and
`values-*` for `basic`, `compressed`, `fixture-abc`, `demo`. Everything else
is added by the phase that makes it pass (listed there).

### Fixtures (all generated by `tests/make_fixtures.py`, text ones as literals)

| file | content | purpose |
|---|---|---|
| `float16.asdf` | `np.arange(6, dtype=float16).reshape(2,3)`, **block** storage, Python default version (1.6.0) | Roman-style float16: `ls`/`copy` must work on every build; copy stays 1.6.0 with ndarray-1.1.0; refused at `--standard-version=1.0.0` |
| `float16-inline.asdf` | same array, `set_array_storage(arr, "inline")` | value parsing needs `_Float16`: `ls` succeeds where available, fails with the "this build" message elsewhere |
| `structured.asdf` | dtype `[('a','>u1'),('b','<f4'),('c','>i2',(2,)),('name','<U16')]`, 3 records, block storage | structured read with per-field byte order, sub-array field, and a `[ucs4, 16]` field as in Roman skycells |
| `strings.asdf` | `np.array(['ab','cde'], dtype='S3')` and `np.array(['αβ','😀x'], dtype='<U2')` as blocks, plus the same two inline | ascii/ucs4 blocks and inline, non-BMP code point |
| `rank0.asdf` | `{scalar: np.array(5.0), int: np.array(-7, dtype='<i4')}`, block storage | rank-0 blocks are standard: `ls`, `copy`, `values` and `py-compare` must work; only `--array=inline` is refused (Phase 2) |
| `header-comment.asdf` | literal text, `#ASDF_STANDARD 1.6.0` on line 2 and a `#ASDF_STANDARD 1.0.0` YAML comment inside the tree | only the first two lines declare the versions (Phase 2) |
| `masked.asdf` | `np.ma.array(np.arange(6), mask=[0,1,0,0,1,0])` | `error-masked` |
| `bigendian.asdf` | `base = np.arange(24, dtype='>i4').reshape(4,6)`; tree `{base, view: base[::-1, ::2], fortran: np.asfortranarray(np.arange(6, dtype='>f8').reshape(2,3)), flags: np.array([True, False, True])}` | shared block + negative strides + big-endian + Fortran order + bool8 (Phase 1) |
| `unknown-tags.asdf` | literal text, no blocks, `#ASDF_STANDARD 1.5.0`, root `!core/asdf-1.1.0`: `answer: !core/constant-1.0.0 42`, `custom: !<asdf://example.org/foo-1.0.0> {x: 1, y: [1, 2], nested: !<asdf://example.org/bar-1.0.0> {z: 3}}`, `when: !time/time-1.1.0 2027-01-01T00:00:00.000`, `unit: !unit/unit-1.0.0 DN`, `ratio: !<asdf://example.org/scalar-1.0.0> 1.0`, one inline int64 array | unknown-tag round trip, tagged-scalar text preservation (Phase 3) |
| `alias.asdf` | literal text: `a: &f {name: x, k: [1, 2]}`, `b: *f`, `c: [*f]` | YAML aliases read; copy expands them |
| `untagged-root.asdf` | literal text: bare `---` root, one inline int64 array | untagged root accepted (Phase 3) |
| `roman-like.asdf` | Python asdf 1.6.0 with `asdf.tagged.TaggedDict`/`TaggedString`: `meta: !<asdf://stsci.edu/datamodels/roman/tags/wfi_image-1.0.0>`-style map holding `!time/time-1.1.0` scalars and a `!unit/quantity-1.1.0 {value: <float32 block>, unit: !unit/unit-1.0.0 DN}`; `err: <float16 block 3x3>`; `skycells: <structured block with [ucs4, 16] field>`; `table: !<tag:astropy.org:astropy/table/table-1.0.0> {columns: [!core/column-1.0.0 {name: id, data: <int32 block>}], colnames: [id]}`; `wcs: !<tag:stsci.edu:gwcs/wcs-1.2.0> {steps: [...]}` with an alias between two entries; Python writes `history.extensions` itself | the Roman feature set in ~3 KB; `ls`, `copy` (preserves 1.6.0), `values`, `py-validate --allow-unknown-tags`, `py-compare` |

Total `tests/*.asdf` stays under ~40 KB. Policy for `tests/README.md`: a
fixture is committed only if it is under ~4 KB and tests something the
library cannot write itself; anything larger or any corpus is fetched or
generated. Delete `test-std.sh` (superseded).

### CI (`.github/workflows/CI.yml`; job names unchanged)

Before `Configure`: `actions/setup-python@v5` (3.12, pip cache keyed on
`tests/requirements.txt`); create `$RUNNER_TEMP/asdf-env` venv, `pip install
-r tests/requirements.txt`, export `ASDF_PYTHON`; run
`tests/fetch-reference-files.sh "$RUNNER_TEMP/asdf-standard"` and export
`ASDF_REFERENCE_FILES_DIR`. Append `-DASDF_PYTHON="$ASDF_PYTHON"
-DASDF_REFERENCE_FILES_DIR="$ASDF_REFERENCE_FILES_DIR"` to both Configure
steps. Expected extra runtime about 3 minutes per job. No cache of the
reference files (a 3-second fetch is not worth a failure mode).

### Small cleanups in this phase

- Delete `include/asdf/table.hxx`, `src/table.cxx`; remove them from
  `CMakeLists.txt`, the include in `include/asdf/asdf.hxx`, and the commented
  `tab`/`table` remnants in `asdf.hxx` and `src/asdf.cxx`; drop CODE.md §4.8
  and its layout/diagram lines. `asdf.i` is stale and unbuilt; leave it.
- `utils/copy.cxx` check lambda: print `argv[0] << ": error: " << msg` before
  the usage so option errors are testable.
- `demo/demo.cxx`: point `eta` at an existing path (`#/zeta/1`).

Acceptance: 22 existing tests still pass; with the two variables set, the
registered `ref-*`/`py-*`/`values-*` tests pass; without them, `ctest -N |
grep -c 'ref-\|py-'` is 0; `grep -rn "table\|column" include src` is empty;
CI green on all four platforms with `ref-1.6.0-basic-copy` and
`py-compare-demo` visible as executed.

---

## Phase 1: data access correctness

Files: `include/asdf/ndarray.hxx`, `src/ndarray.cxx`,
`include/asdf/byteorder.hxx`, `include/asdf/datatype.hxx`, `src/datatype.cxx`.

**Accessors.** `byteorder_t get_byteorder() const`, `const vector<bool>
&get_mask() const`, `block_format_t get_block_format() const`,
`compression_t get_compression() const`, `int get_compression_level()
const`, `int64_t num_elements() const`, `bool is_c_contiguous() const`
(strides equal the C-order strides from shape and `type_size()`). Document
that `linear_index()` returns a **byte** offset.

**Correct extraction.**

```cpp
// Host byte order, C-contiguous, offset and strides applied; structured
// datatypes converted field by field (a field's own byteorder wins)
vector<unsigned char> get_data_bytes() const;
template <typename T> vector<T> get_data_vector() const; // built on get_data_bytes
```

Odometer over `shape` in C order; byte offset `offset + sum(strides[d] *
idx[d])`; new `convert_element_to_host(const unsigned char *src, unsigned
char *dst, const datatype_t &, byteorder_t)` in `src/datatype.cxx`: scalars
copied and byte-swapped by size, **complex swapped per component**, ucs4
swapped per 4-byte code unit, ascii copied; structured types per field with
sub-array counts and per-field byte order. `T = bool8_t` maps nonzero bytes
to `true`; never `reinterpret_cast` a block to `bool*`. Negative strides are
legal.

**Bounds.** Private `check_bounds(uint64_t nbytes)`: if `num_elements() ==
0` return; `lo = hi = offset`; per dimension add `strides[d]*(shape[d]-1)`
to `lo` if negative else to `hi`; `ASDF_CHECK(lo >= 0 && hi + type_size()
<= nbytes, "Array data (offset O, shape S, strides T, element size E)
extends beyond the block (N bytes)")`. Guard the arithmetic against overflow
(`__builtin_mul_overflow`/`__builtin_add_overflow` or partial-product
checks). Call it in the general constructor (`block_info ?
block_info->data_space : mdata->nbytes()`), in `ndarray(rs, node)` after
`block_info` is set (uses `data_space`, so no block load), and in
`get_data_bytes()` with the real `mdata->nbytes()`. Delete `check_shape()`
(`src/ndarray.cxx` ~1062) and its commented call sites in `get_data()`.

**float16 on every build.** Make `get_scalar_type_size` return sizes
unconditionally (float16 2, complex32 4, int128/uint128 16) so block arrays
of those types are read and copied byte-for-byte on builds lacking the C++
type; `get_data_bytes()` works for them too; only `parse_scalar`/
`emit_scalar` (inline data) and `get_data_vector<float16_t>` need the type
and throw the "this build" message otherwise. Optional follow-up: a software
half↔float conversion so `get_data_vector<float>` can read float16 arrays on
any build.

**Incidental bugs to fix here.**

- `xtoh<complex<T>>` / `htox<sizeof(complex)>` reverse all bytes and so swap
  real and imaginary parts for non-host byte order (affects inline emission
  and parsing of complex data from big-endian files; `complex.asdf` will
  catch it). Fix in `byteorder.hxx` with `if constexpr` on an `is_complex`
  trait (move it out of the anonymous namespace in `datatype.hxx` ~75-80) and
  swap per component in `parse_scalar`.
- `write_block` uses `get_scalar_type_size(datatype->scalar_type_id)` for the
  blosc/blosc2 `typesize` (`src/ndarray.cxx` ~629, ~663); it throws for
  structured datatypes. Use `datatype->type_size()` clamped to
  `BLOSC_MAX_TYPESIZE`.

**Tests switched on / added.** `ref-*-{endian,float,int,shared}-values` and
`-values2`, `ref-*-complex-py-compare`, `ref-*-shared-py-compare`; new
`bigendian-ls/copy/ls2`, `values-bigendian` and `values-bigendian2` (expected
file `tests/expected/bigendian.txt`: `view` is rows reversed, even columns;
`fortran` is `0 1 2 3 4 5`), `bigendian-inline-copy` (`--array=inline`) +
`values-bigendian-inline`, `py-compare-bigendian`; `float16-ls/copy/ls2` +
`py-compare-float16` + `values-float16` (unconditional); new
`demo/demo-strided.cxx` (`asdf-demo-strided`, a test): writes via the general
constructor an offset view, a negative-stride view (offset at the last row,
strides `{-24, 4}`), Fortran order `{4, 16}`, a big-endian int32 array
pre-swapped with `htox`, a bool8 block, and a big-endian structured record;
reads back and `ASDF_CHECK`s `get_data_vector`/`get_data_bytes`.

Docs: CODE.md §4.3 (accessors, byte-offset semantics), §11 add "shared
blocks are duplicated on copy" (optional follow-up: dedupe in
`writer::add_task` keyed on the memoized state pointer).

---

## Phase 1b: string datatypes (required for Roman skycells, guide windows, catalogs)

Files: `include/asdf/datatype.hxx`, `src/datatype.cxx`, `src/ndarray.cxx`.

- `datatype_t` gains `size_t string_length` (code units), meaningful for
  `id_ascii` and `id_ucs4`; `type_size()` returns `N` and `4*N`;
  `datatype_t(rs, node)` recognises the two-element form `[ascii|ucs4, N]`
  before treating a sequence as a field list (a field list's items are maps
  or scalar type names; the string form's second item is an integer);
  `to_yaml` emits `[ascii, N]`/`[ucs4, N]` in flow style; `yaml_encode`/
  `yaml_decode(scalar_type_id_t)` stay scalar-only (`ASDF_CHECK(node.IsScalar())`
  first with a clear message).
- Blocks: nothing else needed; `get_data_bytes()` swaps ucs4 code units per
  the array or field byte order.
- Inline data: `parse_scalar` for ascii copies the UTF-8 bytes (must be
  7-bit; `ASDF_CHECK`), NUL-pads to N, `ASDF_CHECK(len <= N)`; for ucs4
  decodes UTF-8 to code points (`ASDF_CHECK` well-formed, count `<= N`),
  writes 4-byte units in host order then `htox`. `emit_scalar` reverses,
  trimming trailing NULs and encoding UTF-8. Both `get_scalar_type_size`
  callers and `parse_inline_array` datatype inference are unaffected
  (strings are never inferred).
- Accessors: `get_data_vector<std::string>()` for ascii (trailing NULs
  removed) and `get_data_vector<std::u32string>()` for ucs4; both built on
  `get_data_bytes()`.
- Remove the `ascii`/`ucs4` rows from the error contract; update README's
  "String types are not supported" bullet.
- **Checked arithmetic for structured field sizes** (Phase 1 review
  finding). `field_t::type_size()` must reject a sub-array shape whose
  product, or whose product with the field's element size, does not fit
  (mirroring `num_elements()`/`packed_nbytes()`), and the field-count loop
  in `convert_field_to_host` must use the same checked count. Test: a
  fixture derived from `python-default.asdf` whose array `a` has
  `datatype: [{name: f, datatype: uint8, shape: [4294967296, 4294967296]}]`
  must be refused by `asdf-ls` with a message naming the field shape
  (`expect-error.sh -m shape`), not accepted and later tripped up in
  `asdf-read-check`.

**Tests switched on / added.** Reference files `ascii`, `unicode_bmp`,
`unicode_spp`, `structured` move from unsupported to supported for all
versions (`-ls/-copy/-ls2/-header/-py-compare/-values/-values2`);
`strings-ls/copy/ls2`, `values-strings`, `py-compare-strings`,
`strings-inline-copy` (`--array=inline`, then `values-strings-inline` equal
to `values-strings`); `structured-ls/copy/ls2`, `values-structured`,
`py-compare-structured`; `roman-like-values` includes the ucs4 field.

---

## Phase 2: version table, write options, nonstandard gating, CLI

**2a. Version table.** New `include/asdf/version.hxx` + `src/version.cxx`
(depends only on `error.hxx`; included by `io.hxx`; add to `ASDF_HEADERS`/
`ASDF_SOURCES`).

```cpp
struct version_t { int major, minor, patch;
  static version_t parse(const std::string &);  // ASDF_ERROR on malformed
  std::string str() const; /* comparisons */ };
struct standard_info_t {
  version_t version;
  const char *asdf_tag, *ndarray_tag, *software_tag, *complex_tag,
             *history_entry_tag, *extension_metadata_tag /* nullptr < 1.2.0 */;
  bool has_float16;            // >= 1.6.0
  bool has_history_extensions; // >= 1.2.0
};
constexpr const char asdf_tag_prefix[] = "tag:stsci.edu:asdf/";
const std::vector<standard_info_t> &standard_versions(); // 1.0.0 .. 1.6.0
const standard_info_t &standard_info(const version_t &); // ASDF_ERROR listing supported
version_t default_standard_version();                    // 1.2.0
version_t latest_standard_version();                     // 1.6.0
enum class core_tag_t { none, asdf, ndarray, software, complex_ };
core_tag_t classify_core_tag(const std::string &full_tag); // any known version
```

Remove `ASDF_STANDARD_VERSION*` and `asdf_standard_version*()` from
`config.hxx.in` and `src/config.cxx` (`check_version` only uses
`ASDF_CXX_VERSION`). Every emitted tag comes from the table: `src/asdf.cxx`
(root), `src/ndarray.cxx` (`ndarray::to_yaml`), `src/entry.cxx`
(`software::to_yaml`), `include/asdf/io.hxx` (`operator<<` for
`std::complex`). `complex_entry::to_yaml` must use `w << value` (the writer
overload with the local tag) instead of `yaml_encode(value)`, which goes
through a `YAML::Node` and therefore emits verbatim.

**2b. Requirements pre-pass** (not YAML buffering: the tags inside the tree
depend on the version too, so the version must be known before the first tag
is emitted; a pre-pass touches only metadata, never block data, so
`asdf-copy` keeps streaming).

```cpp
struct content_requirements {           // io.hxx
  bool needs_float16 = false;
  std::vector<std::string> nonstandard; // "<path>: <what>"
  version_t minimum_version() const;    // 1.6.0 if needs_float16 else default
};
virtual void entry::collect_requirements(content_requirements &, const std::string &path) const {}
```

`group`/`sequence` recurse with `path + "/" + key`; `ndarray_entry` forwards
to `ndarray::collect_requirements`, which walks the datatype including record
fields: float16 → `needs_float16` (a legitimate 1.6.0 feature, **never**
nonstandard); complex32 → `needs_float16` and nonstandard; int128/uint128 →
nonstandard; empty shape **in inline form** → nonstandard ("inline rank-0
array"). A rank-0 *block* array is standard — `core/ndarray` puts no lower
bound on `shape` and Python asdf writes `np.array(5.0)` that way — so
refusing it would make files the reference implementation produces
uncopyable; only the inline form is unrepresentable, because `data` has to be
a list. `asdf::requirements()`
walks the root group. `nodes` and `writers` cannot be inspected;
`ndarray::to_yaml` keeps an emission-time check as a safety net.

**2c. Write options.**

```cpp
struct write_options {                  // io.hxx
  enum class version_mode_t { minimal, latest, input, explicit_version };
  version_mode_t version_mode = version_mode_t::minimal;
  version_t explicit_version{};
  bool allow_nonstandard = false;
};
void set_standard_version(write_options &, const std::string &spec); // "minimal"|"latest"|"input"|"x.y.z"
void asdf::write(ostream &, const write_options & = {}) const;
void asdf::write(const string &filename, const write_options & = {}) const;
```

Resolution in `asdf::write`: `minimal` → `max(default, req.minimum_version())`;
`latest` → 1.6.0; `input` → the recorded input header version if present and
known, else `minimal` (the CLI relies on this fallback; an explicit
`--standard-version=input` on a file without a known version is an error
naming the supported range); `explicit_version` → `standard_info(v)`. If the
resolved version is below `req.minimum_version()` (e.g. `input` = 1.5.0 but
the tree holds float16), raise the "requires" error unless `allow_nonstandard`.
Unless `allow_nonstandard`, refuse nonstandard content with the contract
wording.

**2d. Input header recording** (for `input` mode and Phase 3). `struct
file_header { std::string asdf_version, standard_version; }` in `io.hxx`;
`asdf::from_yaml(istream &, file_header &)` parses the `#ASDF` and
`#ASDF_STANDARD` lines while reading (record only, never reject);
`reader_state` and `asdf` store it (`get_input_header()`), `asdf(cs,
project)` copies it. `asdf-ls` prints the standard version.

**2e. Writer.** `writer(ostream &, const map<string,string> &tags, const
standard_info_t &, bool allow_nonstandard)`; header lines use
`standard.version.str()`; after the `%TAG` lines write `os << "--- "` and do
**not** emit `YAML::BeginDoc` (gives `--- !core/asdf-1.1.0` on one line;
verify against yaml-cpp). `flush()`: after `EndDoc`,
`ASDF_CHECK(emitter.good(), "YAML emitter error: " + emitter.GetLastError())`
(yaml-cpp otherwise drops output silently). `ndarray::to_yaml`: emit `offset`
only if nonzero and `strides` only if `!is_c_contiguous()`; emission-time
nonstandard/float16 check unless `w.allow_nonstandard()`.

**2f. Demos and CLI.** `demo/demo-nonstandard.cxx` sets `allow_nonstandard =
true`. `demo/demo.cxx` is unchanged and therefore standard by construction.
`utils/copy.cxx`: **default `version_mode = input`** (preserve; falls back to
minimal when the input declares no known version), `--standard-version=
<minimal|latest|input|x.y.z>`, `--allow-nonstandard`, and
`--compression-level=N` parsed with a range check instead of ten literals.

**Tests switched on / added** (`check-header.sh` unless noted; `header-X` and
`py-*-X` depend on `copy-X`): `header-demo` (`--standard 1.2.0 --root-tag
core/asdf-1.1.0 --ndarray-tag core/ndarray-1.0.0 --absent "offset: 0"
--absent "strides:"`), `header-demo2` (same; demo declares 1.2.0 so `input`
keeps it); `header-nonstandard` (1.6.0 with ndarray-1.1.0 if
`ASDF_HAVE_FLOAT16`, else 1.2.0); `copy-demo-1.6.0` + `header-demo-1.6.0` +
`py-validate-demo-1.6.0 --standard 1.6.0 ... --ndarray-tag core/ndarray-1.1.0`
+ `py-compare-demo-1.6.0`; `copy-demo-1.0.0` + `header-demo-1.0.0`
(`--root-tag core/asdf-1.0.0`) + `py-validate-demo-1.0.0`; `copy-demo-latest`
+ `header-demo-latest` (1.6.0); `copy-python-default-minimal`
(`--standard-version=minimal` → 1.2.0, ndarray-1.0.0) and the default copy
`python-default2.asdf` staying 1.6.0 with ndarray-1.1.0; `padded2.asdf`
staying 1.0.0 with root asdf-1.0.0; `error-copy-nonstandard` (`-m
nonstandard`, no flag), `copy-nonstandard-allowed` + `ls-nonstandard2`;
`error-standard-version-unknown` (`--standard-version=2.0.0`, `-m "standard
version"`); `header-float16-copy` (1.6.0, ndarray-1.1.0; unconditional),
`copy-float16-minimal` (`--standard-version=minimal` → still 1.6.0),
`error-float16-1.0.0` (`--standard-version=1.0.0`, `-m requires -m 1.6.0`),
`copy-float16-1.0.0-allowed` + `header-float16-1.0.0`; under
`ASDF_HAVE_FLOAT16` `float16-inline-ls`, otherwise `error-float16-inline`
(`-m float16 -m "this build"`); `ref-*-*-header` and the same-line `---`
assertion in `check-header.sh` switched on; `roman-like-copy` stays 1.6.0.

Docs: README (supported versions, default rules for library and `asdf-copy`,
flags); CODE.md §4.5, §6, §9, §11; CLAUDE.md bullet "all core tags come from
`version.hxx`".

---

## Phase 3: lossless, generous reader

**3a. Tag storage.** On `entry` (`include/asdf/entry.hxx`): `std::string
tag_` (protected), `get_tag()`, `set_tag()`, holding the **full resolved
URI**. Free function `writer &emit_tag(writer &, const std::string
&full_tag)` in `io.hxx`/`io.cxx`: empty, `?`, `!`, or prefix
`tag:yaml.org,2002:` → nothing; prefix `tag:stsci.edu:asdf/` with a suffix
made only of yaml-cpp tag characters → `YAML::LocalTag(suffix)`; otherwise
`YAML::VerbatimTag(full_tag)` (an unresolved `!foo` becomes `!<!foo>`, valid
YAML; verify PyYAML reads it back). Call it first in `to_yaml` of
`null_entry`, `bool_entry`, `int_entry`, `float_entry`, `string_entry`,
`sequence`, `group`, `reference_entry`; never in `complex_entry`, `software`,
`ndarray_entry` (their tags come from the table; a second tag would put the
emitter into its error state). Every `copy()` and `(cs, other)` constructor
copies `tag_`.

**Tagged scalars keep their text.** An unknown-tagged scalar (`!time/time-1.1.0
2027-01-01T00:00:00.000`, `!unit/unit-1.0.0 DN`, `!<asdf://…> 1.0`) is stored
as a `string_entry` holding the exact scalar text with a `plain` flag, and
re-emitted with its tag and the text unchanged (yaml-cpp's default scalar
style, which quotes only when necessary), so `1.0` stays `1.0` and no quotes
are added around timestamps. Untagged scalars keep today's typed handling.

**3b. Dispatch.** `make_entry(rs, node)`: classify with `classify_core_tag`;
for `none` with a non-trivial tag, build by node type (scalars as above) and
`set_tag(tag)`. Remove `entry_type_t::history_entry` and the commented
`history_entry` class. Remove the never-implemented `readers` parameter and
`reader_t` typedef from `asdf`. YAML aliases need no special handling:
yaml-cpp resolves them, each occurrence becomes its own entry, and the copy
contains the expanded content.

**3c. Root and history.** `asdf(rs, node)`: accept an untagged root; accept
any `classify_core_tag == asdf`; accept any other tag starting with
`tag:stsci.edu:asdf/core/asdf-` (unknown asdf version, read anyway); reject
anything else with `Root tag "…" is not a core/asdf tag`. Delete the
`history` skip; `history` becomes an ordinary preserved group. When the write
target lacks `has_history_extensions` (1.0.0/1.1.0) and `history` is a map:
emit only its `entries` list, or omit `history`; with `allow_nonstandard`
emit as-is. `software(rs, node)`: `ASDF_CHECK` that `name` and `version`
exist.

**3d. Block-loss guard.** In `group::to_yaml`, if `tag_` is non-empty and the
group has an integer `source` entry, `ASDF_ERROR` (contract table) unless
`w.allow_nonstandard()`. Without it, a copy of a file with a future
`core/ndarray-9.9.9` would silently point `source` at the wrong block.
Nested known ndarrays inside preserved tagged maps (quantities, columns) are
unaffected: they are parsed as `ndarray_entry` and their blocks are copied.

**3e. Specific messages** for every read-path row of the contract table
(streaming at `read_block`; exploded before `yaml_decode(source)`; `mask`
key; inline `null` in the rank-0 branch of `parse_inline_array_nd`, where an
`ASDF::error` is not caught by the datatype-inference `RepresentationException`
handlers and so propagates; `Cannot open` in `asdf(filename)` and
`reader_state::resolve_reference`).

**Tests switched on / added.** `ref-*-{exploded,stream}-unsupported` gain
their `-m` substrings; `error-unknown-tag` on `tests/corrupt-tag.asdf`
(ndarray-9.9.9 with `source: 0`) becomes: `asdf-ls` succeeds
(`unknown-tag-ls`), `asdf-copy` fails with `-m ndarray-9.9.9 -m block` (3d),
`--allow-nonstandard` succeeds; `unknown-tags-ls/copy` +
`header-unknown-tags-copy --present core/constant-1.0.0 --present
asdf://example.org/foo-1.0.0 --present asdf://example.org/bar-1.0.0 --present
"!time/time-1.1.0 2027-01-01T00:00:00.000" --present "!unit/unit-1.0.0 DN"
--present "1.0"` (grep tag suffixes; either tag form is valid) +
`py-validate-unknown-tags2 --allow-unknown-tags`; `alias-ls/copy` +
`header-alias-copy --absent "&f" --present "name: x"` + `py-compare-alias`;
`header-python-default2 --present history: --present extension_metadata
--present extension_uri` + `py-validate-python-default2 --expect-history`;
`copy-python-default-1.0.0` has no `extensions:` and starts
`--- !core/asdf-1.0.0`; `untagged-root-ls/copy` + `header-untagged-root-copy
--standard 1.2.0 --root-tag core/asdf-1.1.0` + `py-validate-untagged-root2`;
`error-masked` (`-m mask`); `roman-like-ls/copy/ls2`, `header-roman-like-copy
--standard 1.6.0 --root-tag core/asdf-1.1.0 --ndarray-tag core/ndarray-1.1.0
--present wfi_image --present "gwcs/wcs" --present "core/column-1.0.0"
--present history:`, `values-roman-like` and `values-roman-like2`,
`py-validate-roman-like2 --allow-unknown-tags --expect-history`,
`py-compare-roman-like`. Python: opening `unknown-tags2.asdf` with
`AsdfConversionWarning` as error must raise (proves the tags survived).

Docs: README history line ("preserved and round-trip unchanged; asdf-cxx
adds none") and unknown-tag paragraph; CODE.md §5.3, §11; tests/README.md.

---

## Phase 4: emitter-based node emission (local complex tags)

`writer &emit_node(writer &, const YAML::Node &)` in `io.hxx`/`io.cxx`:
re-emits a `YAML::Node` tree through the Emitter, converting tags with
`emit_tag` and preserving `node.Style()` (Flow/Block). Use it in
`ndarray::to_yaml` for the `emit_inline_array` result and `datatype->to_yaml()`,
and in `asdf::to_yaml` for `nodes`. Result: `- !core/complex-1.0.0 1+0i`
instead of `- !<tag:stsci.edu:asdf/core/complex-1.0.0> 1+0i`. Verify
flow-style propagation and `.inf`/`.nan` scalars; `compare-demo` must still
pass; `check-header.sh demo.asdf --present "!core/complex-1.0.0 1+0i" --absent
"!<tag:"`.

**Plain float scalars** (Phase 3 review finding). `yaml_encode(float32_t)`
and `yaml_encode(float64_t)` hand yaml-cpp a C++ floating-point value, which
it emits without a fractional part when there is none: a metadata scalar
written `1.0` copies out as `1`, and Python asdf reads it back as an `int`.
Emit a float that is integral with a trailing `.0` (or route it through the
same spelling logic as the complex components), so that both tree scalars and
inline float array elements keep their type. Test: add the case to
`tests/scalar-types.asdf` and assert it in `header-scalar-types-copy`.

**Complex NaN and infinity** (Phase 1 review finding). `yaml_encode_complex`
and the writer's `operator<<` for `std::complex` must spell non-finite
components as the `complex-1.0.0` schema requires: `nan`, `inf`, `-inf`
(no leading dot; the schema's pattern accepts `inf|INF|nan|NAN`), giving
for example `nan+infi` and `1.5-infi`. `yaml_decode_complex` must accept
both that spelling and yaml-cpp's `.nan`/`.inf`/`-.inf`, so files written by
earlier asdf-cxx versions stay readable. Tests: add a NaN and an infinite
element to the complex arrays in `asdf-demo-strided` (block and inline);
register `ref-<v>-complex-inline-copy` (`asdf-copy --array=inline`) followed
by `ref-<v>-complex-inline-ls` and `ref-<v>-complex-inline-py-compare` (the
reference `complex.asdf` contains nan/inf/-0.0 combinations); the Python
comparison is the proof that the spelling is what the reference
implementation parses.

---

## Phase 5: documentation

- README "Standard conformance": versions written (library default, `asdf-copy`
  default, `latest`, `input`, `minimal`, explicit) and read (any header; core
  tags asdf 1.0.0/1.1.0, ndarray 1.0.0/1.1.0, software, complex; string
  datatypes; unknown tags preserved), features refused with their messages,
  opt-in nonstandard extensions (lz4f, int128/uint128/complex32, rank-0), the
  `asdf-copy` flags, a short note that Roman WFI products are a supported
  input; replace the "more tests should compare to Python" item with a
  pointer to `tests/README.md`.
- CODE.md: version table, `write_options`, requirements pre-pass, tag
  preservation, string datatypes, `get_data_bytes`, the two CMake variables,
  the test families (`header-*`, `values-*`, `error-*`, `ref-<v>-<n>-*`,
  `py-*`), updated §11.
- CLAUDE.md: build/test section mentions `-DASDF_PYTHON` and
  `-DASDF_REFERENCE_FILES_DIR=$(tests/fetch-reference-files.sh)` and that a
  plain build needs neither; pitfalls gain the version-table rule, the
  nonstandard gate, "error wording for unsupported features is asserted by
  tests (`expect-error.sh -m`)", and "Roman files must stay readable: never
  turn a recognised datatype or foreign tag into a whole-file error".
- tests/README.md: fixture policy and table, reference files (pin, fetch
  script, how to update), Python environment, expected-output files.

---

## Cross-cutting edge cases (reviewer checklist)

- 1.6.0 file's map-form `history` written to a 1.0.0 target: `entries` kept,
  `extensions` dropped unless `--allow-nonstandard`.
- Input with inconsistent version/tags (1.2.0 header, ndarray-1.1.0 tags) is
  read; the copy is always internally consistent.
- `asdf-copy` of a 1.6.0 Roman file: stays 1.6.0 with ndarray-1.1.0; of a
  1.6.0 file without float16 also stays 1.6.0 (preserve), `--standard-version=
  minimal` gives 1.2.0.
- Tagged scalar text preserved byte-for-byte (`1.0`, timestamps, units).
- Tagged sequence emits its tag before `BeginSeq`; a tagged `$ref` map before
  its flow map.
- Unknown-tagged map with integer `source` refused on write (3d); tagged maps
  whose children include ndarrays are fine.
- Block shared by two arrays: both views read correctly; block written twice.
- Negative strides accepted; bounds from `lo`; extraction reads backwards.
- bool8 blocks: one byte per element. ucs4: 4-byte units swapped per unit.
- complex32: nonstandard everywhere and, when allowed, bumps the minimal
  version to 1.6.0.
- `writer` used directly: after construction the stream already holds
  `--- `; the caller emits the root tag and map (document in CODE.md §4.5).
- yaml-cpp emitter error state becomes an `ASDF::error` from `flush()`.
- Expected-output files must be identical across the four CI platforms (no
  per-platform variants; NaN sign normalised; float16 via software conversion).

yaml-cpp behaviours to verify while implementing: same-line root tag without
`BeginDoc` and `EndDoc` still writing `...`; `VerbatimTag("!foo")` output and
PyYAML round trip; `LocalTag` rejecting `[ ] , !`; manipulator order
`LocalTag` → scalar for plain tagged text; flow-style propagation in
`emit_node`; asdf-1.1.0 schema accepting the list form of `history`. Python
behaviours to confirm once the venv exists: each reference file opens cleanly
under 5.3.1 (`complex` may warn); `validate_checksums=True` on zero
checksums; `TaggedDict` round trip for the `roman-like.asdf` generator.

---

## Verification (what the reviewer runs after each phase)

Environment: fresh build directories (the repo's `build/` is stale), Python
asdf from `tests/requirements.txt` in a venv, reference files from
`tests/fetch-reference-files.sh`.

```bash
cmake -S . -B build-dbg -G Ninja -DCMAKE_BUILD_TYPE=Debug -DASDF_PYTHON=$PWD/asdf-env/bin/python -DASDF_REFERENCE_FILES_DIR=$(tests/fetch-reference-files.sh) && cmake --build build-dbg && ctest --test-dir build-dbg --output-on-failure
```

Repeat with `-DCMAKE_BUILD_TYPE=Release`. A plain configure without the two
variables must register no `ref-*`/`py-*` tests and pass.

Per phase:

- **Phase 0**: `git ls-files tests | xargs du -ch | tail -1` under ~50 KB and
  no file over ~6 KB; `ctest -N` shows the `ref-*`/`py-*`/`values-*` families;
  CI logs show them executed on all four platforms; `grep -rn "table\|column"
  include src` empty; `asdf-copy --bogus` prints `error:`.
- **Phase 1**: `build-dbg/asdf-read-check <ref>/1.6.0/endian.asdf` prints
  `big` and `little` both as `0 1 ... 41`; `shared.asdf` prints `subset:
  int64 [4] 1 3 5 7`; `ref-*-complex-py-compare` passes (per-component
  complex swap); `asdf-demo-strided` passes; `bigendian.asdf` round-trips
  through `asdf-copy` and `--array=inline`; `asdf-copy tests/float16.asdf`
  works on every platform.
- **Phase 1b**: `ref-*-{ascii,unicode_bmp,unicode_spp,structured}-*` pass;
  `asdf-read-check tests/strings.asdf` prints `ab cde` and `αβ 😀x` for block
  and inline arrays alike; `py-compare-structured` passes with the ucs4
  field; the README no longer lists string types as unsupported.
- **Phase 2**: `head -2 build-dbg/demo.asdf` shows `#ASDF_STANDARD 1.2.0`;
  `grep -c '^---' demo.asdf` is 1 and the line is `--- !core/asdf-1.1.0`;
  `grep -c 'offset: 0' demo.asdf` is 0; `asdf-copy --standard-version=1.6.0
  demo.asdf x.asdf` then `python tests/python_check.py validate --standard
  1.6.0 --ndarray-tag core/ndarray-1.1.0 x.asdf` passes; `asdf-copy
  tests/float16.asdf f.asdf` keeps 1.6.0; `asdf-copy --standard-version=1.0.0
  tests/float16.asdf y.asdf` exits 1 mentioning `requires` and `1.6.0`;
  `asdf-copy nonstandard.asdf z.asdf` exits 1, with `--allow-nonstandard`
  exits 0; `asdf-copy tests/python-default.asdf p.asdf` keeps 1.6.0 and
  `--standard-version=minimal` gives 1.2.0; `grep -rn '"core/' src include |
  grep -v version.cxx` finds nothing.
- **Phase 3**: `asdf-copy tests/python-default.asdf h.asdf` keeps
  `history:` and `extension_metadata-1.0.0`, and Python `validate
  --expect-history h.asdf` passes; `asdf-copy tests/unknown-tags.asdf u.asdf`
  keeps all foreign tags and the scalar texts unchanged; `asdf-copy
  tests/roman-like.asdf r.asdf` succeeds, keeps 1.6.0 and every foreign tag,
  `asdf-read-check r.asdf` matches the original, and Python `compare` passes;
  `asdf-ls tests/untagged-root.asdf` works; `exploded`/`stream` reference
  files fail with their contract substrings and never with `bad conversion`,
  `yaml-cpp`, or `Unknown datatype`; `asdf-copy tests/corrupt-tag.asdf c.asdf`
  fails mentioning the tag and `block`. If a real Roman WFI file is at hand:
  `asdf-ls`, `asdf-copy`, `asdf-read-check` on the copy, and Python `compare`
  original vs copy all succeed.
- **Phase 4**: `grep -c '!<tag:' demo.asdf` is 0 and
  `!core/complex-1.0.0 1+0i` is present; Python reads `deltab`.
- **Phase 5**: README/CODE.md/CLAUDE.md/tests/README.md describe the
  behaviour above; the README's "Standard conformance" section lists every
  contract-table feature as refused and string datatypes and Roman files as
  supported.
