#!/usr/bin/env perl
#
# Generate game/txt/hlp/ files from the CHANGES file(s).
# Should be run by Makefile from top-level directory
#
# Requires pandoc
#
# Usage: mkvershlp game/txt/hlp CHANGES.176 CHANGES.OLD ...
#
# Each file CHANGES.<blah> generates file pennv<blah>.hlp in the
# specified directory.
#
use strict;
use autodie;
use version 0.77;  # Cause the docs said so.
use Text::Wrap;
use IPC::Open2;
use IO::Handle;
use File::Copy;
use File::Temp qw/tempfile/;

print "Rebuilding HELP CHANGES entries...\n";

my $targetdir = shift;
my @sources = @ARGV;
my $verspat = qr'^Version (\S+) patchlevel (\S+)';
my %patchlevels;

@sources = sort byrevision map { s/\.md$//; $_; } @sources;

foreach my $file (@sources) {
    next if $file =~ /~$/o;

    my $markdown = 0;

    if (-f "$file.md") {
        $markdown = 1;
        $file .= ".md";
    }

    open my $IN, "<", $file;

    my $outfile = make_hlpname($file);

    open my $OUT, ">", $outfile;

    if ($markdown) {
        process_markdown($IN, $OUT);
    } else {
        process_text($IN, $OUT);
    }

    close $IN;
    close $OUT;
}

add_header(make_hlpname($sources[0]));
versions_index(make_hlpname($sources[-1]));

print "Done.\n";

sub make_hlpname {
    my $target = shift;
    $target =~ s/.*\.(\d+|OLD)(?:\.md)?$/pennv$1.hlp/;
    return "$targetdir/$target";
}

# Turn a plain text changelog into hlp
sub process_text {
    my ($IN, $OUT) = @_;
    my $started = 0;

    while (<$IN>) {
        if (/$verspat/) {
            print $OUT "& $1p$2\n$_";
            push @{$patchlevels{$1}}, $2;
            $started = 1;
        } elsif ($started) {
            print $OUT $_;
        }
    }
}

# Turn a markdown changelog into hlp
sub process_markdown {
    no autodie qw/sysread/;

    my ($IN, $OUT) = @_;
    my ($MDIN, $MDOUT);

    my $pid = open2 $MDOUT, $MDIN, "pandoc", "-f", "markdown-smart", "-t", "utils/change2hlp.lua";

    my $started = 0;
    my $n = 0;
    $MDOUT->blocking(0);
    my $text;

    while (<$IN>) {
        if (/$verspat/) {
            push @{$patchlevels{$1}}, $2;
            $started = 1;
        }
        if ($started) {
            print $MDIN $_;
            if (($n++ % 20) == 0) {
                while (sysread($MDOUT, $text, 4096) > 0) {
                    print $OUT $text;
                }
            }
        }
    }
    close $MDIN;
    # Read remaining pandoc output
    $MDOUT->blocking(1);
    while (sysread($MDOUT, $text, 4096) > 0) {
        print $OUT $text;
    }
    close $MDOUT;
    waitpid $pid, 0;
}

sub versions_index {
    my $file = shift;

    open my $OUT, ">>", $file;

    # Now spew the patchlevels list. Special case for 1.50
    $patchlevels{'1.5.0'} = $patchlevels{'1.50'};
    delete $patchlevels{'1.50'};
    my @versions = sort {$b cmp $a} map {version->parse($_)} keys %patchlevels;
    print $OUT <<'EOP';
& patchlevels
For information on a specific patchlevel of one of the versions listed,
type 'help <version>p<patchlevel>'. For example, 'help 1.7.2p3'

EOP

    foreach (@versions) {
        my @pls = sort {$a <=> $b} @{$patchlevels{$_}};
        my $line;
        if ($_ eq "1.5.0") {
            $line = sprintf "1.50: %s\n", join(", ", @pls);
        } else {
            $line = sprintf "$_: %s\n", join(", ", @pls);
        }
        print $OUT wrap("", " " x 8, $line);
    }

    close $OUT;
}

# A sort subroutine to order CHANGES.<blah> in reverse chronological
# order
sub byrevision {
  return $b cmp $a if ($a =~ /\d/ and $b =~ /\d/);
  return $a cmp $b;
}

sub add_header {
    my $file = shift;

    open my $IN, "<", $file;
    my ($OUT, $tmpfile) = tempfile();

    print $OUT "& changes\n";

    my $line = <$IN>;
    print $OUT $line;
    $line = <$IN>;
    print $OUT $line, "\n";

    print $OUT <<'EOP';
This is a list of changes in this patchlevel which are probably of
interest to players. More information about new commands and functions
can probably be gotten via 'help <name of whatever>'. 'help credits'
lists the [initials] of developers and porters that are used in the list 
of changes.

Information about changes in prior releases can be found under
help topics named for each release (e.g. 'help 1.8.7p0').
A list of the patchlevels associated with each release can
be read in 'help patchlevels'.

EOP

    while ($line = <$IN>) {
        print $OUT $line;
    }

    close $IN;
    close $OUT;
    move $tmpfile, $file;
}
