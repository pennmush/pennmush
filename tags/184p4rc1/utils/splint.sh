#!/bin/sh

# Wrapper script for checking Penn source with splint (http://www.splint.org)
# Run from within the source directory.

SFLAGS="-I.. -I../hdrs +posixlib -weak"

# Disable assorted spurious warnings
SFLAGS="$SFLAGS -nestcomment -fixedformalarray"
SFLAGS="$SFLAGS -predbool -retvalother -unrecog"

# Work around C99/GCC keywords splint doesn't understand
SFLAGS="$SFLAGS -D__restrict= -Drestrict="

echo "Using options: $SFLAGS"

exec splint $SFLAGS $* 
