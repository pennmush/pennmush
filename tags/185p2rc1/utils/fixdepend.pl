#!/usr/bin/env perl -i~
# Get rid of all global headers. We only really care about the ones
# in hdrs/

my $in_deps = 0;

while (<>) {
    $in_deps = 1 if /make depend depends on it/;
    next if $in_deps and m!:\s+/!;
    print;
}


