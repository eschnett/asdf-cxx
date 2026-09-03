#!/bin/sh
# Run a command that is expected to fail cleanly: exit status 1 and a message
# containing "error:" on stderr. A crash (signal) or a silent failure does not
# count as passing.
#
# Synopsis: expect-error.sh [-m <substring>]... <command> [args...]
#
# Every -m substring must occur in the command's stderr, compared
# case-insensitively as a fixed string. Without -m only the "error:" marker is
# required.
matches_file="expect-error.$$.matches"
: >"$matches_file"
while [ $# -gt 0 ]; do
  case "$1" in
    -m)
      if [ $# -lt 2 ]; then
        echo "expect-error.sh: -m needs an argument" >&2
        rm -f "$matches_file"
        exit 2
      fi
      printf '%s\n' "$2" >>"$matches_file"
      shift 2
      ;;
    --) shift; break ;;
    *) break ;;
  esac
done
if [ $# -eq 0 ]; then
  echo "Synopsis: expect-error.sh [-m <substring>]... <command> [args...]" >&2
  rm -f "$matches_file"
  exit 2
fi

stderr="expect-error.$$.stderr"
"$@" 2>"$stderr"
status=$?
result=0
if [ "$status" -ne 1 ]; then
  echo "expected exit status 1, got $status" >&2
  result=1
fi
if ! grep -q "error:" "$stderr"; then
  echo "expected an \"error:\" message on stderr" >&2
  result=1
fi
while IFS= read -r pattern; do
  [ -n "$pattern" ] || continue
  if ! grep -i -q -F -e "$pattern" "$stderr"; then
    echo "expected \"$pattern\" on stderr" >&2
    result=1
  fi
done <"$matches_file"
cat "$stderr" >&2
rm -f "$stderr" "$matches_file"
exit "$result"
