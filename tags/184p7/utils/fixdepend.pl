#!/usr/bin/env perl -pi~
# Get rid of all global headers. We only really care about the ones
# in hdrs/
$_ = "" if m#:\s+/#;

