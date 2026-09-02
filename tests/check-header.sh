#!/bin/sh
# Check properties of an ASDF file's YAML head (everything up to the "..."
# document end marker).
#
# Synopsis: check-header.sh <file> [--standard X.Y.Z] [--root-tag T]
#                                  [--ndarray-tag T]
#                                  [--present <str>]... [--absent <str>]...
#
# Always checked:
#   - line 1 is "#ASDF 1.0.0"
#   - line 2 is "#ASDF_STANDARD <version>" (the version only with --standard)
# With --root-tag: there is exactly one line matching "^--- !core/asdf-", it
#   carries the given tag, and there is no bare "---" line (the root tag has to
#   sit on the same line as the document start marker).
# With --ndarray-tag: every "!core/ndarray-X.Y.Z" occurrence uses that tag.
# --present/--absent: fixed strings that must / must not occur in the head.
file=$1
shift || {
  echo "Synopsis: check-header.sh <file> [options]" >&2
  exit 2
}
standard=
root_tag=
ndarray_tag=
present_file="check-header.$$.present"
absent_file="check-header.$$.absent"
: >"$present_file"
: >"$absent_file"
cleanup() { rm -f "$present_file" "$absent_file" "$head_file"; }
head_file="check-header.$$.head"

while [ $# -gt 0 ]; do
  case "$1" in
    --standard) standard=$2; shift 2 ;;
    --root-tag) root_tag=$2; shift 2 ;;
    --ndarray-tag) ndarray_tag=$2; shift 2 ;;
    --present) printf '%s\n' "$2" >>"$present_file"; shift 2 ;;
    --absent) printf '%s\n' "$2" >>"$absent_file"; shift 2 ;;
    *) echo "check-header.sh: unknown option \"$1\"" >&2; cleanup; exit 2 ;;
  esac
done

if [ ! -f "$file" ]; then
  echo "check-header.sh: no such file: $file" >&2
  cleanup
  exit 1
fi

# The YAML head, up to and including the document end marker
sed -n '1,/^\.\.\.$/p' "$file" >"$head_file"

result=0
fail() { echo "$file: $1" >&2; result=1; }

line1=$(sed -n '1p' "$head_file")
[ "$line1" = "#ASDF 1.0.0" ] ||
  fail "line 1 is \"$line1\", expected \"#ASDF 1.0.0\""

line2=$(sed -n '2p' "$head_file")
case "$line2" in
  "#ASDF_STANDARD "*) ;;
  *) fail "line 2 is \"$line2\", expected an \"#ASDF_STANDARD\" line" ;;
esac
if [ -n "$standard" ] && [ "$line2" != "#ASDF_STANDARD $standard" ]; then
  fail "line 2 is \"$line2\", expected \"#ASDF_STANDARD $standard\""
fi

if [ -n "$root_tag" ]; then
  n=$(grep -c '^--- !core/asdf-' "$head_file" || true)
  [ "$n" -eq 1 ] ||
    fail "found $n lines matching \"^--- !core/asdf-\", expected exactly 1"
  grep -q "^--- !$root_tag\$" "$head_file" ||
    fail "the document start marker does not carry the tag \"!$root_tag\""
  if grep -q '^---[[:space:]]*$' "$head_file"; then
    fail "found a bare \"---\" line; the root tag must be on the same line"
  fi
fi

if [ -n "$ndarray_tag" ]; then
  bad=$(grep -o '!core/ndarray-[0-9.]*' "$head_file" | sort -u |
        grep -v "^!$ndarray_tag\$" || true)
  if [ -n "$bad" ]; then
    fail "found ndarray tags other than \"!$ndarray_tag\": $(echo $bad)"
  fi
fi

while IFS= read -r pattern; do
  [ -n "$pattern" ] || continue
  grep -q -F -e "$pattern" "$head_file" ||
    fail "expected \"$pattern\" in the YAML head"
done <"$present_file"

while IFS= read -r pattern; do
  [ -n "$pattern" ] || continue
  if grep -q -F -e "$pattern" "$head_file"; then
    fail "did not expect \"$pattern\" in the YAML head"
  fi
done <"$absent_file"

if [ "$result" -ne 0 ]; then
  echo "--- YAML head of $file ---" >&2
  cat "$head_file" >&2
fi
cleanup
exit "$result"
