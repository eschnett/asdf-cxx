#!/bin/sh
# Fetch the ASDF standard's reference files at the commit pinned in
# tests/asdf-standard.pin. They are deliberately not committed to this
# repository; see tests/README.md.
#
# Synopsis: fetch-reference-files.sh [dest]
#
# Clones only the `reference_files` directory (sparse checkout, depth 1) into
# `dest` (default ./asdf-standard) and prints `dest/reference_files` on stdout,
# so that it can be used directly as
#
#     cmake ... -DASDF_REFERENCE_FILES_DIR=$(tests/fetch-reference-files.sh)
#
# Re-running is a no-op when `dest` is already at the pinned commit.

set -e

repo="https://github.com/asdf-format/asdf-standard.git"
here=$(cd "$(dirname "$0")" && pwd)
pin=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$here/asdf-standard.pin" | head -1 |
      tr -d '[:space:]')
if [ -z "$pin" ]; then
  echo "$0: no commit found in $here/asdf-standard.pin" >&2
  exit 1
fi

dest=${1:-./asdf-standard}

# Already at the pin?
if [ -d "$dest/.git" ]; then
  have=$(git -C "$dest" rev-parse HEAD 2>/dev/null || true)
  if [ "$have" = "$pin" ] && [ -d "$dest/reference_files" ]; then
    echo "$dest/reference_files"
    exit 0
  fi
fi

if [ ! -d "$dest/.git" ]; then
  mkdir -p "$dest"
  git -C "$dest" init --quiet
  git -C "$dest" remote add origin "$repo"
fi
git -C "$dest" config core.sparseCheckout true
git -C "$dest" sparse-checkout set --no-cone reference_files >/dev/null 2>&1 ||
  echo 'reference_files/*' >"$dest/.git/info/sparse-checkout"
git -C "$dest" fetch --quiet --depth 1 origin "$pin"
git -C "$dest" checkout --quiet --force FETCH_HEAD

if [ ! -d "$dest/reference_files" ]; then
  echo "$0: $dest/reference_files is missing after the fetch" >&2
  exit 1
fi

echo "$dest/reference_files"
