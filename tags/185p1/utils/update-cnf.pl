#!/usr/local/bin/perl -w
#
# You make have to change the path to perl above, unless you call this
# script with 'make update'
#
# update-cnf.pl - integrate previous mush.cnf settings with new 
#                  mush.cnf.dist. Results appear in mush.cnf
#
# Usage: update-cnf.pl old-file new-file
#  e.g.: update-cnf.pl game/mush.cnf game/mush.cnf.dist
#
# 'make update' calls this program as in the example above.
#
# Here's how it works.
# First, we make a backup of your old-file to old-file.bak
# If old-file doesn't exist, just copy new-file to it.
# Otherwise... 
# * Read both files, storing all their directives.
# * Then, for each directive in old-file, if one of the same name also
#   exists in new-file, write out old-file's version.
# * If it's a multiple option, like function_alias, add ones present
#   in new-file that aren't in old-file. If old-file has entries that
#   aren't in new-file, also write them out.
# * If it's a single option and is missing from new-file, prompt to
#   keep it or not.
# * Then add directives that are in new-file but not old-file.

use File::Copy;
use strict;
use subs qw/read_cnf_file prompt_to_save dump_directive/;

die "Usage: $0 old-file new-file\n" unless scalar @ARGV == 2;

my ($old, $new) = @ARGV;

my $bak = $old . ".bak";

# Part 1 - back up the old file
if (-r $old) {
    print "*** Backing up $old to $bak...\n";
    copy $old, $bak or die "Unable to back up $old: $!\n";
} else {
    # Heck, let's just copy the new file to the old one and quit!
    print "*** Creating $old from $new...\n";
    copy $new, $old or die "Unable to copy $old to $new: $!\n";
    exit 0;
}

# Part 2 - read the settings from the current and the distributed config files and store them.

my $working_cnf = read_cnf_file $old;
my $dist_cnf = read_cnf_file $new;

# Part 3: Merge changes to the distributed conf file with the working one.

my $tmp = $old . ".tmp";
print "*** Updating $old from $new...\n";

my @comment = ();
my @newoptions = ();
my %seen;
my %opt_seen;
my $expectCommentedDirective = 0;

open OUT, ">", $tmp or die "update-cnf.pl: Unable to open $old: $!\n"; 
open IN, "<", $old or die "update-cnf.pl: Unable to open $new: $!\n"; 
while (<IN>) {
    if (/^#\s*OPTIONAL/) {
        # The next line is a commented-out directive that is optional.
        $expectCommentedDirective = 1;
        push @comment, $_;
    } elsif (/^#/) {
        if ($expectCommentedDirective && /^#\s*(\S+)/) {
            # An optional directive
            my $key = $1; 
            $opt_seen{$key} = 1;
        }
        push @comment, $_;
    } elsif (/^\s+$/) {
        # Blank line. Dump preceding comments (if any) or ignore.
        $expectCommentedDirective = 0;
        if (scalar @comment) {
            print OUT "\n", @comment;
            undef @comment;
        }
    } elsif (/^(\S+)/) {
        # Directive
        $expectCommentedDirective = 0;
        my $key = $1;    
        
        undef @comment;
        
        next if $seen{$key};
        
        if (!defined $dist_cnf->{$key}) {
            # Present in working copy, not in dist copy.
            dump_directive \*OUT, $working_cnf->{$key} if prompt_to_save $key, $working_cnf->{$key};
            $seen{$key} = 1;
        } else {
            # Present in working copy and dist copy.
            my $nwork = scalar @{$working_cnf->{$key}};
            my $ndist = scalar @{$dist_cnf->{$key}};
            if ($nwork == $ndist) {
                # Both copies have the same number of repeats. Use working version.
                dump_directive \*OUT, $working_cnf->{$key};
                $seen{$key} = 1;
            } elsif ($nwork > $ndist) {
                # There are more records in the working cnf
                # file. Assume they're supposed to be there
                # (Game-specific function aliases, etc.)
                # TODO: Consider prompting for each one?
                dump_directive \*OUT, $working_cnf->{$key};
                $seen{$key} = 1;
            } else {
                # More records in the dist file. Try to merge.
                my %records;
        
                # Copy everything in the conf file.
                dump_directive \*OUT, $working_cnf->{$key};
        
                # And add anything new from the dist file. TODO: Prompting?
                foreach my $dir (@{$dist_cnf->{$key}}) {
                    $records{$dir->{"value"}} = $dir->{"comment"};
                }
                foreach my $dir (@{$working_cnf->{$key}}) {
                    delete $records{$dir->{"value"}};
                }
                while (my ($val, $c) = each %records) {
                    if ($val =~ /^(\S+)\s/) {
                    push @newoptions, "$key $1";
                    } else {
                    push @newoptions, $key;
                    }
                    print OUT $c, $key, " ", $val, "\n";
                }
                $seen{$key} = 1;
            }
        }
    }
}
close IN;

# Now we check for things in dist that haven't been seen in the working file.
# Sorted using position
my @keys = keys %{$dist_cnf};

# Unique 'em, I'm seeing duplicates.
my %useen = ();
my @ukeys = grep { ! $useen{$_} ++ } @keys;

my @sorted = sort { $dist_cnf->{$a}->[0]->{"position"} <=> $dist_cnf->{$b}->[0]->{"position"} } @ukeys;
for my $key (@sorted) {
    my $dir = $dist_cnf->{$key};
    next if $seen{$key} || $opt_seen{$key};
    dump_directive \*OUT, $dir;
    push @newoptions, $key;   
}

close OUT;
move $tmp, $old or die "Unable to create $old: $!\n";

my (@deleted, @retained);

print "\nSummary of changes:\n";
print "New options from $new: ", join(", ",@newoptions), "\n";
print "Old options retained: ", join(", ",@retained), "\n";
print "Old options deleted: ", join(", ",@deleted), "\n";
print "If this is wrong, you can recover $old from $bak.\n";
print "Done!\n";

sub read_cnf_file {
    my (%directive, @comment);
    my $file = shift;
    my $position = 0;
    my $expectCommentedDirective = 0;
    
    print "*** Reading settings from $file...\n";
    open FILE, "<", $file or die "update-cnf.pl: Unable to open $file: $!\n"; 
    while (<FILE>) {
        # We can have comments, which start with #,
        # or directives, which start with anything else.
        chomp;
        if (/^#\s*OPTIONAL/) {
            # The next line is a commented-out directive that is optional.
            $expectCommentedDirective = 1;
            push @comment, $_;
        } elsif (/^#/) {
            if ($expectCommentedDirective && /^#\s*(\S+)\s+(.+)$/) {
                # A directive, but an optional one.
                my ($key, $val) = ($1, $2);
                my $c = join "\n", @comment;
                $c .= "\n" if length $c;
                if (defined $directive{$key}) {
                    # This is a repeated directive! 
                    push @{$directive{$key}}, {"key" => $key, "value" => $val, "comment" => $c, "optional" => 1 };
                } else {
                    $directive{$key} = [{"position" => $position++, "key" => $key, "value" => $val, "comment" => $c, "optional" => 1}]; 
                }
                undef @comment;
            } elsif ($expectCommentedDirective && /^#\s*(\S+)\s*$/) {
                # An optional directive that's defined as blank
                my ($key, $val) = ($1, "");
                my $c = join "\n", @comment;
                $c .= "\n" if length $c;
                if (defined $directive{$key}) {
                    # These probably won't repeat, but... 
                    push @{$directive{$key}}, {"key" => $key, "value" => "", "comment" => $c, "optional" => 1 };
                } else {
                    $directive{$key} = [{"position" => $position++, "key" => $key, "value" => "", "comment" => $c, "optional" => 1 }];
                }
                undef @comment;
            } else {
                push @comment, $_;
            }
        } elsif (/^(\S+)\s+(.+)$/) {
            $expectCommentedDirective = 0;
            # A directive
            my ($key, $val) = ($1, $2);
            my $c = join "\n", @comment;
            $c .= "\n" if length $c;
            if (defined $directive{$key}) {
                # This is a repeated directive! 
                push @{$directive{$key}}, {"key" => $key, "value" => $val, "comment" => $c };
            } else {
                $directive{$key} = [{"position" => $position++, "key" => $key, "value" => $val, "comment" => $c}]; 
            }
            undef @comment;
        } elsif (/^(\S+)/) {
            # A directive that's defined as blank
            my ($key, $val) = ($1, "");
            my $c = join "\n", @comment;
            $c .= "\n" if length $c;
            if (defined $directive{$key}) {
            # These probably won't repeat, but... 
            push @{$directive{$key}}, {"key" => $key, "value" => "", "comment" => $c };
            } else {
            $directive{$key} = [{"position" => $position++, "key" => $key, "value" => "", "comment" => $c }];
            }
            undef @comment;
        } elsif (/^$/) {
            # A blank line. Ignore comments so far
            $expectCommentedDirective = 0;
            undef @comment;
        }
    }
    close FILE;
    return \%directive;
}

sub prompt_to_save {
    my $name = shift;
    my $directive = shift;

    print "\nI found $name:\n";
    dump_directive \*STDOUT, $directive;
    print "\n";
    print "If this is a directive that you hacked in, you probably should retain it.\n";
    print "If not, it's probably an obsolete directive from an earlier release,\n";
    print "and you need not retain it.\n";
    print "Do you want to retain this in your $old file? [y] ";
    my $yn = <STDIN>;
    if ($yn !~ /^n/i) {
        print "Retaining directive.\n";
        push @retained, $name;
        return 1;
    } else {
        push @deleted, $name;
        return 0;
    }
}

sub dump_directive {
    my $out = shift;
    my $directive = shift;
    foreach my $d (@{$directive}) {
        print $out "\n", $d->{"comment"} if length $d->{"comment"};
        if ($d->{"optional"}) {
            print $out "# ", $d->{key}, " ", $d->{"value"}, "\n";
        } else {
            print $out $d->{key}, " ", $d->{"value"}, "\n";
        }
    }
}
