# Standard-conformance tests.
#
# These tests need input that is deliberately not committed to this
# repository, so each family is registered only when the corresponding CMake
# cache variable is set. Without them the suite is the one described in
# CODE.md §9 and `ctest -N | grep -c 'ref-\|py-'` is 0.
#
#   ASDF_REFERENCE_FILES_DIR  the `reference_files` directory of the ASDF
#                             standard, as printed by
#                             tests/fetch-reference-files.sh
#   ASDF_PYTHON               a Python interpreter with the packages in
#                             tests/requirements.txt
#
# See docs/standard-conformance-plan.md; the tests each later phase switches
# on are marked "Phase N" below.

set(ASDF_REFERENCE_FILES_DIR "" CACHE PATH
  "The asdf-standard reference_files directory (see tests/fetch-reference-files.sh)")
set(ASDF_REFERENCE_VERSIONS "1.0.0;1.1.0;1.2.0;1.3.0;1.4.0;1.5.0;1.6.0"
  CACHE STRING "ASDF standard versions to test the reference files of")
set(ASDF_PYTHON "" CACHE FILEPATH
  "A Python interpreter with the packages in tests/requirements.txt")

set(ASDF_TESTS_DIR "${CMAKE_SOURCE_DIR}/tests")
set(ASDF_EXPECTED_DIR "${ASDF_TESTS_DIR}/expected")

# Helpers ######################################################################

# Register a tests/python_check.py invocation, but only if ASDF_PYTHON is set
function(asdf_add_python_test name)
  if(ASDF_PYTHON)
    add_test(NAME ${name}
      COMMAND ${ASDF_PYTHON} "${ASDF_TESTS_DIR}/python_check.py" ${ARGN})
  endif()
endfunction()

# Make `name` depend on `deps`, ignoring tests that were not registered
function(asdf_test_depends name)
  if(NOT TEST ${name})
    return()
  endif()
  set(deps "")
  foreach(dep ${ARGN})
    if(TEST ${dep})
      list(APPEND deps ${dep})
    endif()
  endforeach()
  if(deps)
    set_tests_properties(${name} PROPERTIES DEPENDS "${deps}")
  endif()
endfunction()

# The core tags an ASDF standard version uses; see include/asdf/version.hxx
function(asdf_standard_tags version root_var ndarray_var)
  if(version VERSION_LESS 1.2.0)
    set(${root_var} core/asdf-1.0.0 PARENT_SCOPE)
  else()
    set(${root_var} core/asdf-1.1.0 PARENT_SCOPE)
  endif()
  if(version VERSION_LESS 1.6.0)
    set(${ndarray_var} core/ndarray-1.0.0 PARENT_SCOPE)
  else()
    set(${ndarray_var} core/ndarray-1.1.0 PARENT_SCOPE)
  endif()
endfunction()

# Register a tests/check-header.sh invocation on a written file
function(asdf_add_header_test name file)
  add_test(NAME ${name}
    COMMAND "${ASDF_TESTS_DIR}/check-header.sh" ${file} ${ARGN})
endfunction()

# `check-header.sh --standard <v>` plus the root and ndarray tags of <v>
function(asdf_add_version_test name file version)
  asdf_standard_tags(${version} root_tag ndarray_tag)
  asdf_add_header_test(${name} ${file} --standard ${version}
    --root-tag ${root_tag} --ndarray-tag ${ndarray_tag} ${ARGN})
endfunction()

# Register an `asdf-read-check` test against a committed expected output
function(asdf_add_values_test name expected)
  if(NOT EXISTS "${ASDF_EXPECTED_DIR}/${expected}")
    message(WARNING "No expected output ${expected}; skipping test ${name}")
    return()
  endif()
  add_test(NAME ${name}
    COMMAND "${ASDF_TESTS_DIR}/expect-output.sh"
    "${ASDF_EXPECTED_DIR}/${expected}" ./asdf-read-check ${ARGN})
endfunction()

# The asdf-standard reference files ############################################

set(ASDF_REF_SUPPORTED
  anchor ascii basic complex compressed endian float int scalars shared
  structured unicode_bmp unicode_spp)
# Recognised but deliberately refused
set(ASDF_REF_UNSUPPORTED exploded stream)
# Those of ASDF_REF_SUPPORTED whose values asdf-read-check prints against a
# committed expected output. `anchor` and `scalars` hold no arrays, and
# `complex.asdf` holds 400 complex numbers whose expected output would be
# 9 kB -- larger than the whole rest of tests/expected/ -- so it is covered
# by `-py-compare` and by the complex arrays in `asdf-demo-strided` instead.
set(ASDF_REF_VALUES
  ascii basic compressed endian float int shared structured unicode_bmp
  unicode_spp)

if(ASDF_REFERENCE_FILES_DIR)
  foreach(version ${ASDF_REFERENCE_VERSIONS})
    set(dir "${ASDF_REFERENCE_FILES_DIR}/${version}")
    if(NOT IS_DIRECTORY "${dir}")
      message(WARNING
        "No reference files for ASDF standard ${version} in ${dir}; skipping")
      continue()
    endif()

    foreach(name ${ASDF_REF_SUPPORTED})
      set(prefix "ref-${version}-${name}")
      set(copy "${prefix}.asdf")
      add_test(NAME ${prefix}-ls COMMAND ./asdf-ls "${dir}/${name}.asdf")
      add_test(NAME ${prefix}-copy
        COMMAND ./asdf-copy "${dir}/${name}.asdf" "${copy}")
      add_test(NAME ${prefix}-ls2 COMMAND ./asdf-ls "${copy}")
      asdf_test_depends(${prefix}-ls2 ${prefix}-copy)
      # Python must read the copy back and find the same data. The reference
      # `compressed.asdf` files predate the current convention of checksumming
      # the compressed rather than the uncompressed bytes, so Python itself
      # refuses to verify their checksums.
      set(compare_options "")
      if(name STREQUAL "compressed")
        set(compare_options --no-validate-checksums)
      endif()
      asdf_add_python_test(${prefix}-py-compare
        compare ${compare_options} "${dir}/${name}.asdf" "${copy}")
      asdf_test_depends(${prefix}-py-compare ${prefix}-copy)

      list(FIND ASDF_REF_VALUES ${name} has_values)
      if(NOT has_values EQUAL -1)
        asdf_add_values_test(${prefix}-values "${name}.txt"
          "${dir}/${name}.asdf")
        asdf_add_values_test(${prefix}-values2 "${name}.txt" "${copy}")
        asdf_test_depends(${prefix}-values2 ${prefix}-copy)
      endif()
      # asdf-copy preserves the input file's declared standard version, so
      # the copy has to carry the tags that belong to that version
      asdf_add_version_test(${prefix}-header "${copy}" ${version}
        --absent "offset: 0")
      asdf_test_depends(${prefix}-header ${prefix}-copy)
    endforeach()

    # `shared.asdf` holds a second view of one block: `subset` is
    # `base[1::2]`, which needs both an offset and explicit strides
    if(version STREQUAL "1.6.0")
      asdf_add_header_test(ref-${version}-shared-strides
        "ref-${version}-shared.asdf"
        --present "offset: 8" --present "strides: [16]")
      asdf_test_depends(ref-${version}-shared-strides ref-${version}-shared-copy)
    endif()

    foreach(name ${ASDF_REF_UNSUPPORTED})
      # Phase 3 adds the error-message contract substrings (-m ...) here.
      add_test(NAME ref-${version}-${name}-unsupported
        COMMAND "${ASDF_TESTS_DIR}/expect-error.sh"
        ./asdf-ls "${dir}/${name}.asdf")
    endforeach()
  endforeach()
endif()

# Python checks of the files the demos and the fixtures produce ################

# A file asdf-cxx writes from scratch declares the lowest standard version
# that fits its content, which for all the demos is 1.2.0
set(ASDF_WRITTEN_TAGS --standard 1.2.0 --root-tag core/asdf-1.1.0
  --ndarray-tag core/ndarray-1.0.0)
# tests/python-default.asdf and the other Python-written fixtures declare
# 1.6.0, and asdf-copy preserves that
set(ASDF_WRITTEN_TAGS_1_6_0 --standard 1.6.0 --root-tag core/asdf-1.1.0
  --ndarray-tag core/ndarray-1.1.0)

asdf_add_python_test(py-validate-demo validate ${ASDF_WRITTEN_TAGS} demo.asdf)
asdf_test_depends(py-validate-demo demo)
asdf_add_python_test(py-validate-demo2 validate ${ASDF_WRITTEN_TAGS} demo2.asdf)
asdf_test_depends(py-validate-demo2 copy)
asdf_add_python_test(py-compare-demo compare demo.asdf demo2.asdf)
asdf_test_depends(py-compare-demo demo copy)

asdf_add_python_test(py-validate-external validate ${ASDF_WRITTEN_TAGS}
  external.asdf metadata.asdf)
asdf_test_depends(py-validate-external external)

asdf_add_python_test(py-compare-padded
  compare "${ASDF_TESTS_DIR}/padded.asdf" padded2.asdf)
asdf_test_depends(py-compare-padded padded-copy)
asdf_add_python_test(py-compare-python-default
  compare "${ASDF_TESTS_DIR}/python-default.asdf" python-default2.asdf)
asdf_test_depends(py-compare-python-default python-default-copy)
# Phase 3 preserves `history`, so this gains --expect-history
asdf_add_python_test(py-validate-python-default2
  validate ${ASDF_WRITTEN_TAGS_1_6_0} python-default2.asdf)
asdf_test_depends(py-validate-python-default2 python-default-copy)

if(LIBLZ4_FOUND)
  asdf_add_python_test(py-compare-lz4
    compare "${ASDF_TESTS_DIR}/lz4.asdf" lz4-2.asdf)
  asdf_test_depends(py-compare-lz4 lz4-copy)
endif()

# compression.asdf holds one block per available compressor, among them the
# `lz4f` frame encoding that Python cannot read. Re-compressing the whole file
# with a codec Python knows makes it checkable. These copies exist only to feed
# the Python checks, so they are registered together with them.
if(ASDF_PYTHON)
  if(LIBLZ4_FOUND)
    add_test(NAME copy-compression-lz4
      COMMAND ./asdf-copy --compression=lz4 compression.asdf
      compression-lz4.asdf)
    set_tests_properties(copy-compression-lz4 PROPERTIES DEPENDS compression)
    asdf_add_python_test(py-validate-compression-lz4
      validate ${ASDF_WRITTEN_TAGS} compression-lz4.asdf)
    asdf_test_depends(py-validate-compression-lz4 copy-compression-lz4)
  endif()
  add_test(NAME copy-compression-none
    COMMAND ./asdf-copy --compression=none compression.asdf
    compression-none.asdf)
  set_tests_properties(copy-compression-none PROPERTIES DEPENDS compression)
  asdf_add_python_test(py-validate-compression-none
    validate ${ASDF_WRITTEN_TAGS} compression-none.asdf)
  asdf_test_depends(py-validate-compression-none copy-compression-none)
  if(LIBLZ4_FOUND)
    asdf_add_python_test(py-compare-compression
      compare compression-lz4.asdf compression-none.asdf)
    asdf_test_depends(py-compare-compression
      copy-compression-lz4 copy-compression-none)
  endif()
endif()

# Values of the files the demos and the fixtures produce #######################

asdf_add_values_test(values-demo demo.txt demo.asdf)
asdf_test_depends(values-demo demo)
asdf_add_values_test(values-demo2 demo.txt demo2.asdf)
asdf_test_depends(values-demo2 copy)
asdf_add_values_test(values-padded fixture-abc.txt
  "${ASDF_TESTS_DIR}/padded.asdf")
asdf_add_values_test(values-python-default fixture-abc.txt
  "${ASDF_TESTS_DIR}/python-default.asdf")
if(LIBLZ4_FOUND)
  asdf_add_values_test(values-lz4 fixture-abc.txt "${ASDF_TESTS_DIR}/lz4.asdf")
endif()

# Fixtures written by tests/make_fixtures.py ###################################

# YAML anchors and aliases: yaml-cpp resolves them on load, and the copy
# expands them into equal (but no longer shared) content
add_test(NAME fixture-alias-ls COMMAND ./asdf-ls "${ASDF_TESTS_DIR}/alias.asdf")
add_test(NAME fixture-alias-copy
  COMMAND ./asdf-copy "${ASDF_TESTS_DIR}/alias.asdf" alias2.asdf)
add_test(NAME fixture-alias-ls2 COMMAND ./asdf-ls alias2.asdf)
asdf_test_depends(fixture-alias-ls2 fixture-alias-copy)
asdf_add_python_test(fixture-alias-py-compare
  compare "${ASDF_TESTS_DIR}/alias.asdf" alias2.asdf)
asdf_test_depends(fixture-alias-py-compare fixture-alias-copy)

# bigendian.asdf: one block shared by two views, negative and non-contiguous
# strides, big-endian data, Fortran order, and bool8. The copy and the inline
# copy must hold the same values as the original.
add_test(NAME bigendian-ls
  COMMAND ./asdf-ls "${ASDF_TESTS_DIR}/bigendian.asdf")
add_test(NAME bigendian-copy
  COMMAND ./asdf-copy "${ASDF_TESTS_DIR}/bigendian.asdf" bigendian2.asdf)
add_test(NAME bigendian-ls2 COMMAND ./asdf-ls bigendian2.asdf)
asdf_test_depends(bigendian-ls2 bigendian-copy)
add_test(NAME bigendian-inline-copy
  COMMAND ./asdf-copy --array=inline "${ASDF_TESTS_DIR}/bigendian.asdf"
  bigendian-inline.asdf)
asdf_add_values_test(values-bigendian bigendian.txt
  "${ASDF_TESTS_DIR}/bigendian.asdf")
asdf_add_values_test(values-bigendian2 bigendian.txt bigendian2.asdf)
asdf_test_depends(values-bigendian2 bigendian-copy)
asdf_add_values_test(values-bigendian-inline bigendian.txt
  bigendian-inline.asdf)
asdf_test_depends(values-bigendian-inline bigendian-inline-copy)
asdf_add_python_test(py-compare-bigendian
  compare "${ASDF_TESTS_DIR}/bigendian.asdf" bigendian2.asdf)
asdf_test_depends(py-compare-bigendian bigendian-copy)
asdf_add_python_test(py-compare-bigendian-inline
  compare "${ASDF_TESTS_DIR}/bigendian.asdf" bigendian-inline.asdf)
asdf_test_depends(py-compare-bigendian-inline bigendian-inline-copy)

# float16.asdf: Roman WFI level-2 products store float16 arrays. Reading,
# copying and extracting the values of a float16 block must work on every
# build, including one without `_Float16`.
add_test(NAME float16-ls COMMAND ./asdf-ls "${ASDF_TESTS_DIR}/float16.asdf")
add_test(NAME float16-copy
  COMMAND ./asdf-copy "${ASDF_TESTS_DIR}/float16.asdf" float16-2.asdf)
add_test(NAME float16-ls2 COMMAND ./asdf-ls float16-2.asdf)
asdf_test_depends(float16-ls2 float16-copy)
asdf_add_values_test(values-float16 float16.txt
  "${ASDF_TESTS_DIR}/float16.asdf")
asdf_add_values_test(values-float16-2 float16.txt float16-2.asdf)
asdf_test_depends(values-float16-2 float16-copy)
# The copy preserves the input's 1.6.0, so its float16 arrays are tagged
# core/ndarray-1.1.0 and Python validates them against the right schema
asdf_add_python_test(py-compare-float16
  compare "${ASDF_TESTS_DIR}/float16.asdf" float16-2.asdf)
asdf_test_depends(py-compare-float16 float16-copy)

# float16-inline.asdf holds the same array inline, which needs the C++ type
# to parse the values
if(ASDF_HAVE_FLOAT16)
  add_test(NAME float16-inline-ls
    COMMAND ./asdf-ls "${ASDF_TESTS_DIR}/float16-inline.asdf")
  asdf_add_values_test(values-float16-inline float16.txt
    "${ASDF_TESTS_DIR}/float16-inline.asdf")
else()
  add_test(NAME error-float16-inline
    COMMAND "${ASDF_TESTS_DIR}/expect-error.sh" -m float16 -m "this build"
    ./asdf-ls "${ASDF_TESTS_DIR}/float16-inline.asdf")
endif()

# Python must be able to read what asdf-demo-strided writes: arrays with an
# offset, with negative strides, in Fortran order and in the other byte order
asdf_add_python_test(py-validate-strided validate ${ASDF_WRITTEN_TAGS}
  strided.asdf)
asdf_test_depends(py-validate-strided demo-strided)
# (the test names avoid a leading "copy-", whose "py-" would show up in the
# `ctest -N | grep -c 'ref-\|py-'` check for a plain configure)
add_test(NAME strided-copy COMMAND ./asdf-copy strided.asdf strided2.asdf)
set_tests_properties(strided-copy PROPERTIES DEPENDS demo-strided)
add_test(NAME strided-ls2 COMMAND ./asdf-ls strided2.asdf)
asdf_test_depends(strided-ls2 strided-copy)
asdf_add_python_test(py-compare-strided
  compare strided.asdf strided2.asdf)
asdf_test_depends(py-compare-strided strided-copy)

# strings.asdf: `ascii` and `ucs4` arrays as blocks (little-endian, and
# big-endian so that the 4-byte code units have to be swapped) and inline,
# including a non-BMP code point. The inline copy must hold the same values.
add_test(NAME strings-ls COMMAND ./asdf-ls "${ASDF_TESTS_DIR}/strings.asdf")
add_test(NAME strings-copy
  COMMAND ./asdf-copy "${ASDF_TESTS_DIR}/strings.asdf" strings2.asdf)
add_test(NAME strings-ls2 COMMAND ./asdf-ls strings2.asdf)
asdf_test_depends(strings-ls2 strings-copy)
add_test(NAME strings-inline-copy
  COMMAND ./asdf-copy --array=inline "${ASDF_TESTS_DIR}/strings.asdf"
  strings-inline.asdf)
asdf_add_values_test(values-strings fixture-strings.txt
  "${ASDF_TESTS_DIR}/strings.asdf")
asdf_add_values_test(values-strings2 fixture-strings.txt strings2.asdf)
asdf_test_depends(values-strings2 strings-copy)
asdf_add_values_test(values-strings-inline fixture-strings.txt
  strings-inline.asdf)
asdf_test_depends(values-strings-inline strings-inline-copy)
asdf_add_python_test(py-compare-strings
  compare "${ASDF_TESTS_DIR}/strings.asdf" strings2.asdf)
asdf_test_depends(py-compare-strings strings-copy)
asdf_add_python_test(py-compare-strings-inline
  compare "${ASDF_TESTS_DIR}/strings.asdf" strings-inline.asdf)
asdf_test_depends(py-compare-strings-inline strings-inline-copy)

# rank0.asdf: rank-0 arrays (`shape: []`) as blocks, which the core/ndarray
# schema allows and Python asdf writes. Reading, copying and reading the values
# back must work; only the inline form is unrepresentable, because `data` has
# to be a list, so `--array=inline` is refused as nonstandard content.
add_test(NAME rank0-ls COMMAND ./asdf-ls "${ASDF_TESTS_DIR}/rank0.asdf")
add_test(NAME rank0-copy
  COMMAND ./asdf-copy "${ASDF_TESTS_DIR}/rank0.asdf" rank0-2.asdf)
add_test(NAME rank0-ls2 COMMAND ./asdf-ls rank0-2.asdf)
asdf_test_depends(rank0-ls2 rank0-copy)
asdf_add_values_test(values-rank0 rank0.txt "${ASDF_TESTS_DIR}/rank0.asdf")
asdf_add_values_test(values-rank0-2 rank0.txt rank0-2.asdf)
asdf_test_depends(values-rank0-2 rank0-copy)
asdf_add_version_test(header-rank0 rank0-2.asdf 1.6.0)
asdf_test_depends(header-rank0 rank0-copy)
asdf_add_python_test(py-validate-rank0
  validate ${ASDF_WRITTEN_TAGS_1_6_0} rank0-2.asdf)
asdf_test_depends(py-validate-rank0 rank0-copy)
asdf_add_python_test(py-compare-rank0
  compare "${ASDF_TESTS_DIR}/rank0.asdf" rank0-2.asdf)
asdf_test_depends(py-compare-rank0 rank0-copy)
add_test(NAME error-rank0-inline
  COMMAND "${ASDF_TESTS_DIR}/expect-error.sh" -m nonstandard
  -m "inline rank-0 array"
  ./asdf-copy --array=inline "${ASDF_TESTS_DIR}/rank0.asdf"
  rank0-inline-refused.asdf)
add_test(NAME rank0-inline-allowed-copy
  COMMAND ./asdf-copy --array=inline --allow-nonstandard
  "${ASDF_TESTS_DIR}/rank0.asdf" rank0-inline.asdf)
asdf_add_values_test(values-rank0-inline rank0.txt rank0-inline.asdf)
asdf_test_depends(values-rank0-inline rank0-inline-allowed-copy)

# structured.asdf: a record array with per-field byte order, a sub-array field
# and a [ucs4, 16] field, as in Roman skycell reference files. Python asdf 5.3
# with numpy 2 cannot read inline structured arrays, so there is no inline copy.
add_test(NAME structured-ls
  COMMAND ./asdf-ls "${ASDF_TESTS_DIR}/structured.asdf")
add_test(NAME structured-copy
  COMMAND ./asdf-copy "${ASDF_TESTS_DIR}/structured.asdf" structured2.asdf)
add_test(NAME structured-ls2 COMMAND ./asdf-ls structured2.asdf)
asdf_test_depends(structured-ls2 structured-copy)
asdf_add_values_test(values-structured fixture-structured.txt
  "${ASDF_TESTS_DIR}/structured.asdf")
asdf_add_values_test(values-structured2 fixture-structured.txt structured2.asdf)
asdf_test_depends(values-structured2 structured-copy)
asdf_add_python_test(py-compare-structured
  compare "${ASDF_TESTS_DIR}/structured.asdf" structured2.asdf)
asdf_test_depends(py-compare-structured structured-copy)

# Datatypes that a file may claim but that no writer produces. Each has to be
# refused with an ASDF::error naming the problem, not with a yaml-cpp message
# or a bounds-checked container throwing `vector`.
#
#   bad-field-shape    a structured field whose sub-array shape multiplies to
#                      2**64; unchecked, that gives an element size of zero
#   zero-size-datatype `[ascii, 0]`, the other route to a zero element size
#   bad-string-length  `[ucs4, abc]`, a length that is not a number
add_test(NAME error-bad-field-shape
  COMMAND "${ASDF_TESTS_DIR}/expect-error.sh" -m shape
  ./asdf-ls "${ASDF_TESTS_DIR}/bad-field-shape.asdf")
add_test(NAME error-zero-size-datatype
  COMMAND "${ASDF_TESTS_DIR}/expect-error.sh" -m "zero size"
  ./asdf-ls "${ASDF_TESTS_DIR}/zero-size-datatype.asdf")
add_test(NAME error-bad-string-length
  COMMAND "${ASDF_TESTS_DIR}/expect-error.sh" -m "ucs4 datatype must be"
  ./asdf-ls "${ASDF_TESTS_DIR}/bad-string-length.asdf")

# Standard version selection and the write options ############################
#
# `asdf-copy` preserves the input file's declared standard version; a file
# written from scratch gets the lowest version that fits its content. The
# tests below pin down both, plus the flags that override them.
#
# The copies here are named `<what>-copy` rather than `copy-<what>`: a test
# name containing "py-" would show up in the `ctest -N | grep -c 'ref-\|py-'`
# count that must be zero for a plain configure, and "copy-" contains "py-".

asdf_add_header_test(header-demo demo.asdf --standard 1.2.0
  --root-tag core/asdf-1.1.0 --ndarray-tag core/ndarray-1.0.0
  --absent "offset: 0" --absent "strides:")
asdf_test_depends(header-demo demo)
# demo.asdf declares 1.2.0, so the default (preserving) copy keeps it
asdf_add_header_test(header-demo2 demo2.asdf --standard 1.2.0
  --root-tag core/asdf-1.1.0 --ndarray-tag core/ndarray-1.0.0
  --absent "offset: 0" --absent "strides:")
asdf_test_depends(header-demo2 copy)

# nonstandard.asdf holds float16 and complex32 where the build has them,
# which needs standard 1.6.0; otherwise its int128 arrays and rank-0 array
# fit into the default version
if(ASDF_HAVE_FLOAT16)
  asdf_add_version_test(header-nonstandard nonstandard.asdf 1.6.0)
else()
  asdf_add_version_test(header-nonstandard nonstandard.asdf 1.2.0)
endif()
asdf_test_depends(header-nonstandard demo-nonstandard)

# --standard-version=X.Y.Z writes that version, with its tags
add_test(NAME demo-1.6.0-copy
  COMMAND ./asdf-copy --standard-version=1.6.0 demo.asdf demo-1.6.0.asdf)
set_tests_properties(demo-1.6.0-copy PROPERTIES DEPENDS demo)
asdf_add_version_test(header-demo-1.6.0 demo-1.6.0.asdf 1.6.0)
asdf_test_depends(header-demo-1.6.0 demo-1.6.0-copy)
asdf_add_python_test(py-validate-demo-1.6.0
  validate ${ASDF_WRITTEN_TAGS_1_6_0} demo-1.6.0.asdf)
asdf_test_depends(py-validate-demo-1.6.0 demo-1.6.0-copy)
asdf_add_python_test(py-compare-demo-1.6.0 compare demo.asdf demo-1.6.0.asdf)
asdf_test_depends(py-compare-demo-1.6.0 demo demo-1.6.0-copy)

# 1.0.0 uses the older root tag
add_test(NAME demo-1.0.0-copy
  COMMAND ./asdf-copy --standard-version=1.0.0 demo.asdf demo-1.0.0.asdf)
set_tests_properties(demo-1.0.0-copy PROPERTIES DEPENDS demo)
asdf_add_version_test(header-demo-1.0.0 demo-1.0.0.asdf 1.0.0)
asdf_test_depends(header-demo-1.0.0 demo-1.0.0-copy)
asdf_add_python_test(py-validate-demo-1.0.0 validate --standard 1.0.0
  --root-tag core/asdf-1.0.0 --ndarray-tag core/ndarray-1.0.0 demo-1.0.0.asdf)
asdf_test_depends(py-validate-demo-1.0.0 demo-1.0.0-copy)

add_test(NAME demo-latest-copy
  COMMAND ./asdf-copy --standard-version=latest demo.asdf demo-latest.asdf)
set_tests_properties(demo-latest-copy PROPERTIES DEPENDS demo)
asdf_add_version_test(header-demo-latest demo-latest.asdf 1.6.0)
asdf_test_depends(header-demo-latest demo-latest-copy)

# tests/python-default.asdf declares 1.6.0: the default copy keeps it, and
# --standard-version=minimal falls back to the default 1.2.0
asdf_add_version_test(header-python-default2 python-default2.asdf 1.6.0)
asdf_test_depends(header-python-default2 python-default-copy)
add_test(NAME python-default-minimal-copy
  COMMAND ./asdf-copy --standard-version=minimal
  "${ASDF_TESTS_DIR}/python-default.asdf" python-default-minimal.asdf)
asdf_add_version_test(header-python-default-minimal
  python-default-minimal.asdf 1.2.0)
asdf_test_depends(header-python-default-minimal python-default-minimal-copy)
asdf_add_python_test(py-validate-python-default-minimal
  validate ${ASDF_WRITTEN_TAGS} python-default-minimal.asdf)
asdf_test_depends(py-validate-python-default-minimal
  python-default-minimal-copy)

# header-comment.asdf declares 1.6.0 on line 2 and carries a YAML comment
# inside the tree that looks like a `#ASDF_STANDARD 1.0.0` header line. Only
# the first two lines count, so the preserving copy stays 1.6.0 rather than
# silently downgrading itself and its tags.
add_test(NAME header-comment-ls
  COMMAND ./asdf-ls "${ASDF_TESTS_DIR}/header-comment.asdf")
add_test(NAME header-comment-copy
  COMMAND ./asdf-copy "${ASDF_TESTS_DIR}/header-comment.asdf"
  header-comment2.asdf)
asdf_add_version_test(header-header-comment header-comment2.asdf 1.6.0)
asdf_test_depends(header-header-comment header-comment-copy)
asdf_add_python_test(py-compare-header-comment
  compare "${ASDF_TESTS_DIR}/header-comment.asdf" header-comment2.asdf)
asdf_test_depends(py-compare-header-comment header-comment-copy)

# tests/padded.asdf declares 1.0.0, and so does its copy
asdf_add_version_test(header-padded2 padded2.asdf 1.0.0)
asdf_test_depends(header-padded2 padded-copy)

# Content no version of the standard describes is refused unless it is
# explicitly allowed
add_test(NAME error-nonstandard-copy
  COMMAND "${ASDF_TESTS_DIR}/expect-error.sh" -m nonstandard
  ./asdf-copy nonstandard.asdf nonstandard-refused.asdf)
set_tests_properties(error-nonstandard-copy PROPERTIES DEPENDS demo-nonstandard)
add_test(NAME nonstandard-allowed-copy
  COMMAND ./asdf-copy --allow-nonstandard nonstandard.asdf nonstandard2.asdf)
set_tests_properties(nonstandard-allowed-copy
  PROPERTIES DEPENDS demo-nonstandard)
add_test(NAME ls-nonstandard2 COMMAND ./asdf-ls nonstandard2.asdf)
asdf_test_depends(ls-nonstandard2 nonstandard-allowed-copy)

# Bad options
add_test(NAME error-standard-version-unknown
  COMMAND "${ASDF_TESTS_DIR}/expect-error.sh" -m "standard version"
  ./asdf-copy --standard-version=2.0.0 demo.asdf unknown-version.asdf)
set_tests_properties(error-standard-version-unknown PROPERTIES DEPENDS demo)
add_test(NAME error-compression-level
  COMMAND "${ASDF_TESTS_DIR}/expect-error.sh" -m "compression level"
  ./asdf-copy --compression-level=10 demo.asdf bad-level.asdf)
set_tests_properties(error-compression-level PROPERTIES DEPENDS demo)

# float16 is a legitimate feature of standard 1.6.0, never "nonstandard": a
# copy of a float16 file stays 1.6.0, even at --standard-version=minimal, and
# an explicitly requested older version is an error rather than a silent
# downgrade
asdf_add_version_test(header-float16-copy float16-2.asdf 1.6.0)
asdf_test_depends(header-float16-copy float16-copy)
asdf_add_python_test(py-validate-float16
  validate ${ASDF_WRITTEN_TAGS_1_6_0} float16-2.asdf)
asdf_test_depends(py-validate-float16 float16-copy)
add_test(NAME float16-minimal-copy
  COMMAND ./asdf-copy --standard-version=minimal
  "${ASDF_TESTS_DIR}/float16.asdf" float16-minimal.asdf)
asdf_add_version_test(header-float16-minimal float16-minimal.asdf 1.6.0)
asdf_test_depends(header-float16-minimal float16-minimal-copy)
add_test(NAME error-float16-1.0.0
  COMMAND "${ASDF_TESTS_DIR}/expect-error.sh" -m requires -m 1.6.0
  ./asdf-copy --standard-version=1.0.0 "${ASDF_TESTS_DIR}/float16.asdf"
  float16-refused.asdf)
add_test(NAME float16-1.0.0-allowed-copy
  COMMAND ./asdf-copy --standard-version=1.0.0 --allow-nonstandard
  "${ASDF_TESTS_DIR}/float16.asdf" float16-1.0.0.asdf)
asdf_add_version_test(header-float16-1.0.0 float16-1.0.0.asdf 1.0.0)
asdf_test_depends(header-float16-1.0.0 float16-1.0.0-allowed-copy)

# The remaining fixtures are switched on by the phase that makes them work:
#
#   Phase 3   masked.asdf (must become an error; it is silently read today),
#             unknown-tags.asdf, untagged-root.asdf, roman-like.asdf
#
# See docs/standard-conformance-plan.md.
