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
      # Phase 2 adds ${prefix}-header (the copy's declared version and tags).
    endforeach()

    foreach(name ${ASDF_REF_UNSUPPORTED})
      # Phase 3 adds the error-message contract substrings (-m ...) here.
      add_test(NAME ref-${version}-${name}-unsupported
        COMMAND "${ASDF_TESTS_DIR}/expect-error.sh"
        ./asdf-ls "${dir}/${name}.asdf")
    endforeach()
  endforeach()
endif()

# Python checks of the files the demos and the fixtures produce ################

# asdf-cxx writes ASDF 1.2.0 with core/ndarray-1.0.0 tags. Phase 2 adds
# `--root-tag core/asdf-1.1.0`, which needs the root tag on the `---` line.
set(ASDF_WRITTEN_TAGS --standard 1.2.0 --ndarray-tag core/ndarray-1.0.0)

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
  validate ${ASDF_WRITTEN_TAGS} python-default2.asdf)
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
# py-compare-float16 waits for Phase 2: the copy declares standard 1.2.0 but
# needs core/ndarray-1.1.0 tags for its float16 datatype, so Python rejects
# it against the 1.0.0 ndarray schema.

# float16-inline.asdf holds the same array inline, which needs the C++ type
# to parse the values
if(ASDF_HAVE_FLOAT16)
  add_test(NAME float16-inline-ls
    COMMAND ./asdf-ls "${ASDF_TESTS_DIR}/float16-inline.asdf")
  asdf_add_values_test(values-float16-inline float16.txt
    "${ASDF_TESTS_DIR}/float16-inline.asdf")
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

# bad-field-shape.asdf: a structured field whose sub-array shape multiplies to
# 2**64. Unchecked, that gives an element size of zero, which passes the array
# bounds check and only trips up much later.
add_test(NAME error-bad-field-shape
  COMMAND "${ASDF_TESTS_DIR}/expect-error.sh" -m shape
  ./asdf-ls "${ASDF_TESTS_DIR}/bad-field-shape.asdf")

# The remaining fixtures are switched on by the phase that makes them work:
#
#   Phase 2   py-compare-float16
#   Phase 3   masked.asdf (must become an error; it is silently read today),
#             unknown-tags.asdf, untagged-root.asdf, roman-like.asdf
#
# See docs/standard-conformance-plan.md.
