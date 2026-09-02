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

# Readable today. `ascii`, `unicode_bmp`, `unicode_spp` and `structured` need
# string datatypes and move here in Phase 1b.
set(ASDF_REF_SUPPORTED
  anchor basic complex compressed endian float int scalars shared)
set(ASDF_REF_STRINGS ascii unicode_bmp unicode_spp structured)
# Recognised but deliberately refused
set(ASDF_REF_UNSUPPORTED exploded stream)
# Those of ASDF_REF_SUPPORTED whose values asdf-read-check prints; `anchor` and
# `scalars` hold no arrays
set(ASDF_REF_VALUES basic complex compressed endian float int shared)
# Of those, the ones whose values the library reads correctly today. `endian`
# needs byte-order handling and `shared` needs offset/strides (Phase 1);
# `complex`, `float` and `int` have big-endian variants (Phase 1).
set(ASDF_REF_VALUES_NOW basic compressed)

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

      list(FIND ASDF_REF_VALUES_NOW ${name} values_now)
      if(NOT values_now EQUAL -1)
        asdf_add_values_test(${prefix}-values "${name}.txt"
          "${dir}/${name}.asdf")
        asdf_add_values_test(${prefix}-values2 "${name}.txt" "${copy}")
        asdf_test_depends(${prefix}-values2 ${prefix}-copy)
      endif()
      # Phase 1 switches on -values/-values2 for the rest of ASDF_REF_VALUES.
      # Phase 2 adds ${prefix}-header (the copy's declared version and tags).
    endforeach()

    # Phase 1b moves ASDF_REF_STRINGS to ASDF_REF_SUPPORTED; until then they
    # have to fail cleanly like the genuinely unsupported files.
    foreach(name ${ASDF_REF_UNSUPPORTED} ${ASDF_REF_STRINGS})
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

# The remaining fixtures are switched on by the phase that makes them work:
#
#   Phase 1   float16.asdf, float16-inline.asdf, bigendian.asdf
#   Phase 1b  structured.asdf, strings.asdf
#   Phase 3   masked.asdf (must become an error; it is silently read today),
#             unknown-tags.asdf, untagged-root.asdf, roman-like.asdf
#
# See docs/standard-conformance-plan.md.
