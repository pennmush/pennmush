#!/usr/bin/perl

# Utility to convert a changelog to a html fragment, for pasting into
# c.p.o release announcements, etc.

# Usage:
# utils/change2html.pl [-r VERSION] [-f] CHANGES.184 ... > output.html

# If a specific version isn't given, converts all entries in all files
# given on command line. Versions look like 1.8.4p6

# -f produces a complete html document.

use warnings;
use strict;
use Getopt::Std;
use Sort::Versions;
use CGI ':html';
use subs qw/parse_version reset_list print_line/;

our ($opt_r, $opt_f);

$opt_r = undef;

die "Usage: $0 [-r VERSION] [-f] FILE ...\n" unless getopts("fr:");

my $display = defined $opt_r ? 0 : 1;

my $skipping_noise = 1;
my $in_list = 0;
my $in_elem = 0;
my $line = "";

if ($opt_f) {
    print start_html("PennMUSH Changes");
    print h1({-style => "text-align:center"},
	     $opt_r ? "Changes in this version" : "PennMUSH Changes"), "\n";
}

while (<>) {
    chomp;

    if ($skipping_noise) {
	$skipping_noise = 0 if /^=+$/;
	next;
    }

    if (/^Version/) {
	my $this_vers = parse_version $_;	
	
	reset_list;

	if (versioncmp($this_vers, $opt_r) == 0) {
	    $display = 1;
	} elsif (defined $opt_r) {
	    last if $display;
	}
	
	if ($display) {
	    $_ =~ m/^(Version \d+\.\d+\.\d+ patchlevel \d+)\s+(\S\S\S\s+\S\S,\s+\S\S\S\S)/;
	    reset_list;
	    print hr(), "\n";
	    print h2("$1 ($2)"), "\n\n";
	}

    } elsif (/^([\w\s]+):/) {
	if ($display) {
	    reset_list;
	    print h3($1), "\n\n <ul>\n";
	    $in_list = 1;
	}
    } elsif (/^\s+\* (.*)/) {
	if ($display) {
	    print_line if $line;
	    $line = $1;
	    $in_elem = 1;
	}
    } elsif (/^\s+(\S.*)/) {
	if ($display) {
	    $line .= " ";
	    $line .= $1;
	}
    }
}

print end_html(), "\n" if $opt_f;

sub parse_version {
    my $vers = shift;

    if ($vers =~ m/^Version (\d+\.\d+\.\d+) patchlevel (\d+)/) {
	return "$1p$2";
    } else {
	return "Unknown";
    }
}

sub reset_list {
    print_line if $in_elem;
    print " </ul>\n\n" if $in_list;
    $in_elem = $in_list = 0;
}

sub print_line { 
    
    $line = escapeHTML $line;

    ## Pretty printing
    # Things that look like commands and functions
    $line =~ s#(\@[\w/]+)#<code>$1</code>#g;
    $line =~ s#(\w+\(\))#<code>$1</code>#g;
    # %-subs
    $line =~ s#(%[0-9\#!@?^cu=])#<var>$1</var>#g;
    # Dev initials
    $line =~ s#(\[[A-Z]+\])#<b>$1</b>#g;
    # And credits
    $line =~ s#(Suggested|Reported) by ([^.,]+)#$1 by <em>$2</em>#;

    print "  ", li($line), "\n";
    $line = "";
    $in_elem = 0;
}






