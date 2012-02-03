#!/usr/bin/env perl
#
# Generate game/txt/hlp/ files from the CHANGES file(s).
# Should be run by Makefile from top-level directory
# 
# Requires the extra Sort::Versions module. Install through CPAN.
#
# Usage: mkvershlp game/txt/hlp CHANGES.176 CHANGES.OLD ...
#
# Each file CHANGES.<blah> generates file pennv<blah>.hlp in the
# specified directory.
#
use strict;
use Sort::Versions;
use Text::Wrap;

BEGIN {
    print "Rebuilding HELP CHANGES entries...\n";
}

END {
    print "Done.\n";
}

my $targetdir = shift;
my @sources = @ARGV;
my $verspat = qr'^Version (\S+) patchlevel (\S+)';
my %patchlevels;

@sources = sort byrevision @sources;

my $really_started = 0;
foreach my $file (@sources) {
  next if $file =~ /~$/o;
  warn "Can't open $file!\n", next unless open IN, "<", $file;
  my $target = $file;
  $target =~ s/.*\.(.*)/pennv$1.hlp/;
  open(OUT,">","$targetdir/$target") or die "Unable to open $targetdir/$target\n";
  my $started = 0;
  while (<IN>) {
    if (/$verspat/o) {
      print OUT "& $1p$2\n";
      push @{$patchlevels{$1}}, $2;
      unless ($started) {
        # This is the first one 
        unless ($really_started) {
          print OUT <<'EOP';
& changes
This is a list of changes in this patchlevel which are probably of
interest to players. More information about new commands and functions
can probably be gotten via 'help <name of whatever>'. 'help credits'
lists the [initials] of developers and porters that are used in the list 
of changes.

Information about changes in prior releases can be found under
help topics named for each release (e.g. 'help 1.7.2p30').
A list of the patchlevels associated with each release can
be read in 'help patchlevels'.

EOP
          $really_started = 1;
        }
        $started = 1;
      }
      print OUT;
    } elsif ($started) {
      print OUT;
    } 
  }
  close IN;
}

# Now spew the patchlevels list. Special case for 1.50
$patchlevels{'1.5.0'} = $patchlevels{'1.50'};
delete($patchlevels{'1.50'});
my @versions = reverse sort versions keys %patchlevels;
print OUT <<EOP;
& patchlevels
For information on a specific patchlevel of one of the versions listed,
type 'help <version>p<patchlevel>'. For example, 'help 1.7.2p3'

EOP
foreach (@versions) {
  my @pls = sort {$a <=> $b} @{$patchlevels{$_}};
  my $line;
  if ($_ eq "1.5.0") {
    $line = "1.50: ". join(", ",@pls). "\n";
  } else {
    $line = "$_: ". join(", ",@pls). "\n";
  }
  print OUT wrap("","       ",$line);
}

close OUT;


# A sort subroutine to order CHANGES.<blah> in reverse chronological
# order
sub byrevision {
  return $b cmp $a if ($a =~ /\d/ and $b =~ /\d/);
  return $a cmp $b;
}

