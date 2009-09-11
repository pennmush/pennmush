#!/usr/bin/perl -w

# Tool to grep changelogs and show what versions mention a given string.

use strict;
use Getopt::Std;

sub HELP_MESSAGE {
    print STDERR <<EOH;
Usage: $0 [OPTIONS] REGEXP

Searches Penn help files for occurrences of a regular expression and
display the results in a smart fashion. Run from the base PennMUSH
directory.

Options:

    -i: Ignore case.
    -b: Anchor the search regexp with word-boundries to avoid partial matches.
    -p: Print each matching line instead of a summary.
--help: This message.

EOH
exit 0;
}

our ($opt_i, $opt_b, $opt_p);

die "Unknown option given.\n" unless getopts("ibp");

my $string = "@ARGV";
my @files = glob "CHANGES.*";
my $matches = 0;
my $pattern = "";

# Massage the search string per options. This approach will quickly
# grow unmanageable with many more.
if ($opt_i) {
    if ($opt_b) {
	$pattern = qr/\b$string\b/i;
    } else {
	$pattern = qr/$string/i;
    }
} else {
    if ($opt_b) {
	$pattern = qr/\b$string\b/;
    } else {
	$pattern = qr/$string/;
    }
}

foreach my $file (@files) {
    open CHANGELOG, "<", $file 
	or (warn "Unable to open $file: $!\n" && next);
    my $version = "Unknown";
    my $vmatches = 0;
    while (<CHANGELOG>) {
	if (/^Version (\d[\d.]+ patchlevel \d+)/) {
	    print "Found $vmatches occurrences in $version\n" if $vmatches > 0 && !$opt_p;
	    $version = $1;
	    $vmatches = 0;
	    next;
	}
	if (/$pattern/) {
	    $matches++;
	    $vmatches++;
	    if ($opt_p) {
		print "In version $version:\n" if $vmatches == 1;
		print;
	    }
	}	
    }
    print "Found $vmatches occurrences in $version\n" if $vmatches > 0 && !$opt_p;
    close CHANGELOG;
}


print "No occurrences of '$string' found.\n" if $matches == 0;

