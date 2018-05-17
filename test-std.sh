#!/bin/bash

# Test standard conformance

STDDIR="$HOME/src/asdf/asdf-standard/reference_files/1.0.0"
FILES='
basic.asdf
complex.asdf
compressed.asdf
float.asdf
int.asdf
shared.asdf
'
BROKEN='
ascii.asdf
exploded.asdf
stream.asdf
unicode_bmp.asdf
unicode_spp.asdf
'

for file in $FILES; do
    echo "Testing file $file..."
    ./asdf-copy "$STDDIR/$file" "$file"
done
