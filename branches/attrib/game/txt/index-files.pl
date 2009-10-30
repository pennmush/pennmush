#!/usr/bin/perl
#
# index-files.pl - make an & index topic for events/news/help
#
# Called by compose-tricky
#
# Take a MUSH help.txt format file on the stdin, and write a
# "& entries" entry or entries.
# Lines with entries to be indexed start with &'s.
# Write the resulting entries to the stdout, also in help.txt format,
# in columns and paged.
# Idea by Schmidt@Dune, perl version by Alan Schwartz (Javelin/Paul)
# with mods by Jeff Heinen. Modified further by Raevnos.
#
# Usage: index-files.pl [options] < news.txt > nws/index.nws
#
require 5; # Sorry, Talek.
use strict;
use Getopt::Long;
use locale;
my (@entries, @aentries);

# Have we got any options?
my($first,$longest) = ("","");
exit 1 unless GetOptions ('first' => \$first, 'longest' => \$longest);

# Collect all the topic names
my @set = ();
my @aset = ();
while (<STDIN>) {
  chomp;
  next if /^& &?Entries/o; # Skip index entries.
  if ((@set or @aset) and $_ !~ /&\s+\S/) {
    # We've got a set of entries now. Choose which to add.
    if ($first) {
       # Add the first one
       push(@entries,$set[0]) if $set[0];
       push(@aentries,$aset[0]) if $aset[0];
    }
    if ($longest) {
       # Add the longest one
       @set = sort { length($b) <=> length($a) } @set;
       @aset = sort { length($b) <=> length($a) } @aset;
       push(@entries,$set[0]) if $set[0];
       push(@aentries,$aset[0]) if $aset[0];
    }
    if (!$first and !$longest) {
       # Add all
       push(@entries,@set) if @set;
       push(@aentries,@aset) if @aset;
    }
    @set = (); @aset = ();
  }
  push @aset,$1 if /^&\s+(&.*)/o; # Catch admin-help entries
  push @set, $1 if /^&\s+([^&].*)/o; # Catch normal entries.
}
# If anything's left in @set/@aset now, someone's screwed up

# Make 'em all lower-case and sort 'em.
@entries = map { lc $_ } @entries;
@aentries = map { lc $_ } @aentries;

sub withnumchecking;
my @sorted = ($#entries > 0) ? (sort withnumchecking @entries) : @entries;
my @asorted = ($#entries > 0) ? (sort withnumchecking @aentries) : @aentries;

my $maxlines = 14;
my $maxlen = 25;
my $separator ="-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n";
my $three_items = " %-25.25s %-25.25s %-25.25s\n"; # Format for three entries
my $bigone = " %-51.51s %-25.25s\n"; # Format for two entries, long first.
my $bigtwo = " %-25.25s %-51.51s\n"; # Format for two entries, long second.
my $admin = 0;
my $index;

foreach $index (\@sorted, \@asorted) { 
  my $i = 0;
  my $title = $admin ? "&Entries" : "Entries";
  $admin++;
  my $titlecount = 1;
  my $header = 0;
  my ($entry1, $entry2, $entry3);
  print "\n& $title\n", $separator;
  while (@$index) {
    if (${$index}[0] eq "help") {
       shift @$index;
       next;
     }
    if ($header) {
      print "& $title-$titlecount\n", $separator;
      $header = 0;
    }
    $entry1 = shift @$index;
    $entry2 = shift @$index;
    $entry2 = "" if !defined $entry2;
    if (length($entry1) > $maxlen) {
      if (length($entry2) > $maxlen) {
        printf " %-76.76s\n", $entry1;
        unshift @$index, $entry2;
      } else {
        printf $bigone, $entry1, $entry2;
      }
    } else {
      if (length($entry2) > $maxlen) {
        printf $bigtwo, $entry1, $entry2;
      } else {
        $entry3 = shift @$index;
        $entry3 = "" if !defined $entry3;
        if (length($entry3) > $maxlen) {
          unshift @$index, $entry3;
          $entry3 ="";
        }
        printf $three_items, $entry1, $entry2, $entry3;
      }
    }
   if ($i++ > $maxlines)  {
     $titlecount++;
     print "\nFor more, see $title-$titlecount\n", $separator;
     $header = 1;
     $i = 0;
   }
 }
}
print $separator;

sub withnumchecking {
  my($firsta, $firstb) = ($a,$b);
  my($lasta, $lastb);
  ($firsta, $lasta) = ($1,$2) if $a =~ /(.+)p(\d+)/o;
  ($firstb, $lastb) = ($1,$2) if $b =~ /(.+)p(\d+)/o;
  my $res = $firsta cmp $firstb;
  return $res if $res;
  if (!defined($lasta)) {
    warn "Possible duplicate help topic: $a\n";
    return $res;
  }
  return $lasta <=> $lastb;
}

