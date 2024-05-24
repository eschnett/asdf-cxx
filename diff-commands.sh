#!/bin/sh

if [ $# != 2 ]; then
    echo "Synopsis: $0 <command 1> <command 2>" 1>&2
    exit 1
fi

# dir=`mktemp -d`
dir="/tmp/$USER-$$-`date -u +%Y%m%dT%H%M%S`"
mkdir "$dir"
$1 | grep -v compress | grep -v checksum >"$dir/output1"
$2 | grep -v compress | grep -v checksum >"$dir/output2"
diff -u "$dir/output1" "$dir/output2"
retval=$?
rm -rf "$dir"
(exit "$retval")
