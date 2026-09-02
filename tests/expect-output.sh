#!/bin/sh
# Run a command and compare its standard output against a committed file.
# Synopsis: expect-output.sh <expected-file> <command> [args...]
expected=$1
shift || {
  echo "Synopsis: expect-output.sh <expected-file> <command> [args...]" >&2
  exit 2
}
if [ ! -f "$expected" ]; then
  echo "expect-output.sh: no such file: $expected" >&2
  exit 1
fi
actual="expect-output.$$.out"
"$@" >"$actual"
status=$?
if [ "$status" -ne 0 ]; then
  echo "expect-output.sh: the command exited with status $status" >&2
  rm -f "$actual"
  exit 1
fi
diff -u "$expected" "$actual"
status=$?
rm -f "$actual"
exit "$status"
