#!/usr/bin/env python
"""Check ASDF files with the Python reference implementation.

Used by the ctest suite (see tests/conformance.cmake) whenever the CMake cache
variable ASDF_PYTHON points at an interpreter that has the packages in
tests/requirements.txt. Two subcommands:

    python_check.py validate <file>... [options]
    python_check.py compare <original> <copy> [--skip <path>]...

`validate` checks that Python asdf reads a file the way the file's own header
claims it should be read; `compare` checks that a copy asdf-cxx produced holds
the same data as its original. Both print one summary line per file and exit
with status 0 (all checks passed) or 1.
"""

import argparse
import re
import sys
import warnings

import numpy as np

import asdf
from asdf.exceptions import AsdfConversionWarning
from asdf.tagged import Tagged, TaggedDict, TaggedList, TaggedString
from asdf.tags.core import ExtensionMetadata
from asdf.tags.core.ndarray import NDArrayType

# Tree keys that a copy is not required to reproduce
SKIP_TOPLEVEL = ("asdf_library", "history")


class CheckFailed(Exception):
    pass


def read_head(path):
    """The bytes of the YAML head, up to and including the "..." marker."""
    with open(path, "rb") as f:
        data = f.read(1 << 20)
    end = data.find(b"\n...")
    return data if end < 0 else data[: end + 4]


def check_header(path, standard, root_tag, ndarray_tag):
    head = read_head(path)
    lines = head.split(b"\n")
    if not lines or lines[0] != b"#ASDF 1.0.0":
        raise CheckFailed(f'line 1 is {lines[0]!r}, expected b"#ASDF 1.0.0"')
    if standard is not None:
        want = b"#ASDF_STANDARD " + standard.encode()
        if len(lines) < 2 or lines[1] != want:
            raise CheckFailed(f"line 2 is {lines[1]!r}, expected {want!r}")
    if root_tag is not None:
        want = b"--- !" + root_tag.encode()
        if not any(line == want for line in lines):
            raise CheckFailed(f"no document start marker {want!r}")
    if ndarray_tag is not None:
        # Both the local (%TAG-abbreviated) and the verbatim spelling
        found = set(re.findall(rb"core/ndarray-[0-9.]+", head))
        want = {ndarray_tag.encode()}
        bad = sorted(t.decode() for t in found - want)
        if bad:
            raise CheckFailed(
                f"unexpected ndarray tags {bad}, expected {ndarray_tag}"
            )


def walk(node, path="", *, skip_toplevel=False):
    """Yield (path, value) for every leaf, descending into tagged containers."""
    if isinstance(node, (dict, TaggedDict)):
        for key in sorted(node.keys(), key=str):
            if skip_toplevel and key in SKIP_TOPLEVEL:
                continue
            if key == "$ref":  # a JSON reference, deliberately not resolved
                return
            yield from walk(node[key], f"{path}/{key}")
        return
    if isinstance(node, (list, tuple, TaggedList)):
        for i, value in enumerate(node):
            yield from walk(value, f"{path}/{i}")
        return
    yield path, node


def find_tagged(node, path=""):
    """Yield (path, value) for every tagged node that has no Python type."""
    if isinstance(node, (TaggedDict, TaggedList, TaggedString)):
        yield path, node
    if isinstance(node, dict):
        for key in sorted(node.keys(), key=str):
            yield from find_tagged(node[key], f"{path}/{key}")
    elif isinstance(node, (list, tuple)):
        for i, value in enumerate(node):
            yield from find_tagged(value, f"{path}/{i}")


def open_file(path, validate_checksums=True, allow_unknown_tags=False):
    with warnings.catch_warnings():
        # asdf reports a tree whose tag versions disagree with the file's
        # declared standard version by falling back to raw dicts and warning.
        # A file that carries foreign tags on purpose raises the same warning
        # for each of them, so the escalation is off where those are expected.
        warnings.simplefilter(
            "ignore" if allow_unknown_tags else "error", AsdfConversionWarning
        )
        af = asdf.open(
            path,
            lazy_load=False,
            memmap=False,
            validate_checksums=validate_checksums,
        )
    return af


def do_validate(args):
    failures = 0
    for path in args.files:
        try:
            check_header(path, args.standard, args.root_tag, args.ndarray_tag)
            allow_unknown = args.allow_unknown_tags or args.expect_unknown_tags
            with open_file(
                path, not args.no_validate_checksums, allow_unknown
            ) as af:
                narrays = 0
                for node_path, value in walk(af.tree):
                    if isinstance(value, NDArrayType):
                        raise CheckFailed(
                            f"{node_path}: an array was not converted to a "
                            f"numpy.ndarray"
                        )
                    if isinstance(value, np.ndarray):
                        narrays += 1
                tagged = [p for p, _ in find_tagged(af.tree)]
                if tagged and not allow_unknown:
                    raise CheckFailed(
                        f"undeserialised tagged nodes: {tagged[:5]}"
                    )
                if args.expect_unknown_tags and not tagged:
                    raise CheckFailed(
                        "no undeserialised tagged nodes; the file's foreign "
                        "tags did not survive"
                    )
                if args.expect_history:
                    history = af.tree.get("history")
                    if not isinstance(history, dict):
                        raise CheckFailed("history is missing or not a map")
                    extensions = history.get("extensions")
                    if not isinstance(extensions, list) or not extensions:
                        raise CheckFailed("history.extensions is missing or empty")
                    for entry in extensions:
                        if not isinstance(entry, ExtensionMetadata):
                            raise CheckFailed(
                                f"history.extensions holds a "
                                f"{type(entry).__name__}, expected "
                                f"ExtensionMetadata"
                            )
                print(f"validate {path}: ok ({narrays} arrays)")
        except Exception as exc:  # noqa: BLE001 - report, do not traceback
            print(f"validate {path}: FAILED: {type(exc).__name__}: {exc}")
            failures += 1
    return failures


def descr(dtype):
    return str(np.dtype(dtype).newbyteorder("=").descr)


def compare_values(path, a, b):
    if isinstance(a, np.ndarray) or isinstance(b, np.ndarray):
        if not (isinstance(a, np.ndarray) and isinstance(b, np.ndarray)):
            raise CheckFailed(f"{path}: array vs {type(b).__name__}")
        if a.shape != b.shape:
            raise CheckFailed(f"{path}: shape {a.shape} vs {b.shape}")
        if descr(a.dtype) != descr(b.dtype):
            raise CheckFailed(
                f"{path}: dtype {descr(a.dtype)} vs {descr(b.dtype)}"
            )
        np.testing.assert_array_equal(a, b, err_msg=f"{path}: values differ")
        return
    if isinstance(a, float) and isinstance(b, float):
        if not (a == b or (np.isnan(a) and np.isnan(b))):
            raise CheckFailed(f"{path}: {a!r} vs {b!r}")
        return
    if a != b:
        # A tag Python asdf deserialises into a plain object without an
        # `__eq__` (asdf.tags.core.Constant, for one) would otherwise compare
        # by identity, which two separately read files never share
        if (
            type(a) is type(b)
            and getattr(a, "__dict__", None) is not None
            and vars(a) == vars(b)
        ):
            return
        raise CheckFailed(f"{path}: {a!r} vs {b!r}")


def do_compare(args):
    try:
        checksums = not args.no_validate_checksums
        unknown = args.allow_unknown_tags
        with open_file(args.original, checksums, unknown) as af1, \
             open_file(args.copy, checksums, unknown) as af2:
            skip = set(args.skip)
            values1 = {
                p: v
                for p, v in walk(af1.tree, skip_toplevel=True)
                if p not in skip
            }
            values2 = {
                p: v
                for p, v in walk(af2.tree, skip_toplevel=True)
                if p not in skip
            }
            only1 = sorted(set(values1) - set(values2))
            only2 = sorted(set(values2) - set(values1))
            if only1 or only2:
                raise CheckFailed(
                    f"paths differ: only in the original {only1[:5]}, "
                    f"only in the copy {only2[:5]}"
                )
            for p in sorted(values1):
                compare_values(p, values1[p], values2[p])
            print(
                f"compare {args.original} {args.copy}: ok "
                f"({len(values1)} values)"
            )
    except Exception as exc:  # noqa: BLE001
        print(
            f"compare {args.original} {args.copy}: FAILED: "
            f"{type(exc).__name__}: {exc}"
        )
        return 1
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("validate", help="open a file with Python asdf")
    p.add_argument("files", nargs="+")
    p.add_argument("--standard", help="expected #ASDF_STANDARD version")
    p.add_argument("--root-tag", help="expected tag on the '---' line")
    p.add_argument("--ndarray-tag", help="the only allowed core/ndarray tag")
    p.add_argument("--allow-unknown-tags", action="store_true")
    p.add_argument(
        "--expect-unknown-tags",
        action="store_true",
        help="implies --allow-unknown-tags, and additionally requires that "
        "at least one node kept a tag Python asdf does not know",
    )
    p.add_argument("--expect-history", action="store_true")
    p.add_argument(
        "--no-validate-checksums",
        action="store_true",
        help="skip block checksums (for files written with the pre-2019 "
        "convention of checksumming the uncompressed data)",
    )
    p.set_defaults(func=do_validate)

    p = sub.add_parser("compare", help="compare a copy against its original")
    p.add_argument("original")
    p.add_argument("copy")
    p.add_argument("--skip", action="append", default=[])
    p.add_argument("--allow-unknown-tags", action="store_true")
    p.add_argument("--no-validate-checksums", action="store_true")
    p.set_defaults(func=do_compare)

    args = parser.parse_args(argv)
    return 1 if args.func(args) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
