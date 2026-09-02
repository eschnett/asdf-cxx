#!/bin/sh
# Run a command that is expected to fail cleanly: exit status 1 and a message
# containing "error:" on stderr. A crash (signal) or a silent failure does not
# count as passing.
# Synopsis: expect-error.sh <command> [args...]
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
cat "$stderr" >&2
rm -f "$stderr"
exit "$result"
