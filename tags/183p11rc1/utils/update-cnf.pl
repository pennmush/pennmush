#!/usr/local/bin/perl
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
# Then we read all the directives in the old-file, and their
#  associated comments. Associated comments are those which
#  appear on lines preceding the directive.
#  We store the names of all the directives, 
#  their comments, and how they're defined.
# Then we do the same for the new-file. If we find a directive
#  that wasn't in the old-file, we show the user the comment
#  and ask them how they want it set. Every time we write out
#  a directive, we delete it from the list of directives from old-file
# Finally, if there's anything left from old-file that's not in
#  new-file, we ask if the user would like to retain each one.
#  Presumably users want to retain their custom directives, but don't
#  want to retain obsoleted directives. Retained directives appear at
#  the end of the file.

die "Usage: update-cnf.pl old-file new-file\n" unless $#ARGV == 1;

$old = $ARGV[0];
$bak = $old . ".bak";
$new = $ARGV[1];


# Part 1 - back up the old file (inefficient but reliable method)
if (-r $old) {
    print "*** Backing up $old to $bak...\n";
    die "update-cnf.pl: Unable to open $old\n" unless open(OLD,"$old"); 
    die "update-cnf.pl: Unable to open $bak\n" unless open(BAK,">$bak");
    print BAK <OLD>;
    close(BAK);
    close(OLD);
} else {
    # Heck, let's just copy the new file to the old one and quit!
    print "*** Creating $old from $new...\n";
    die "update-cnf.pl: Unable to open $old\n" unless open(OLD,">$old"); 
    die "update-cnf.pl: Unable to open $new\n" unless open(NEW,"$new"); 
    print OLD <NEW>;
    close(OLD);
    close(NEW);
    exit 0;
}
  

# Part 2 - read the settings from the old file and store them
if (-r $old) {
    print "*** Reading your settings from $old...\n";
    die "update-cnf.pl: Unable to open $old\n" unless open(OLD,"$old"); 
    while (<OLD>) {
	# We can have comments, which start with #,
        # or directives, which start with anything else.
	if (/^#/) {
	  # A comment
 	  push(@comment,$_);
	} elsif (/^(\S+)\s+(.+)$/) {
	  # A directive
	  $key = $1; $val = $2;
	  chop;
	  if (defined($directive{$key})) {
	      # This is a repeatable directive! 
	      $num{$key}++;
	      $directive{"$key/$num{$key}"} = $val;
	      $comment{"$key/$num{$key}"} = join("",@comment);
	  } else {
	      $directive{$key} = $val; 
	      $comment{$key} = join("",@comment);
	  }
	  undef @comment;
        } elsif (/^(\S+)/) {
          # A directive that's defined as blank
          $key = $1; $val = "";
          $directive{$key} = $val;
	  $comment{$key} = join("",@comment);
          undef @comment;
    	} elsif (/^$/) {
	  # A blank line. Ignore comments so far
	  undef @comment;
	}
    }
    close(OLD);
}

# Part 3 - read in the new file, modifying its definition lines to
#          match the old file. If we come across a definition that
#          isn't in the old file, ask the user about it. 
print "*** Updating $old from $new...\n";
die "update-cnf.pl: Unable to open $old\n" unless open(OLD,">$old"); 
die "update-cnf.pl: Unable to open $new\n" unless open(NEW,"$new"); 
while (<NEW>) {
    # We can have comments, which start with #,
    # or directives, which start with anything else.
    if (/^#/) {
	# A comment
	push(@comment,$_);
    } elsif (/^(\S+)/) {
	# Not a comment
	$key = $1;
	chop;
	if ($num{$key}) {
	    if ($num{$key} > 0) {
		# A repeatable directive!
		# Spew them all here. It's probably the best we can do.
		print OLD @comment;
		print OLD "$key\t$directive{$key}\n\n";
		delete $comment{$key};
		for ($i = 1; $i <= $num{$key}; $i++) {
		    print OLD $comment{"$key/$i"};
		    print OLD "$key\t", $directive{"$key/$i"},"\n\n";
		    delete $comment{"$key/$i"};
		    delete $directive{"$key/$i"};
		} 
		$num{$key} = -1; # Don't spew again
	    }
	} else {
	    print OLD @comment;
	    if (!defined($directive{$key}) || $key eq "include") {
		# It's new, just add it
		print OLD $_,"\n";
		push(@newoptions,$key);
              delete $comment{$key} if $key eq "include";
	    } else {
		# It's old. Put it in, with a value if one's set.
		print OLD $key;
		print OLD "\t$directive{$key}" if (defined($directive{$key}));
		print OLD "\n";
		# Remove its comment.
		delete $comment{$key};
	    }
	}
	undef @comment;
    } elsif (/^$/) {
	print OLD @comment;
	undef @comment;
	print OLD;
    } else {
	print OLD;
    }
}
close(NEW);

# Part 4 - if there are any definitions left from the old file,
#          offer to delete them (or not)
print "\n*** Checking for leftover defines from $old...\n";
foreach $d (sort { $directive{$a} cmp $directive{$b} } keys %comment) {
    $newd = $d;
    $newd =~ s!/.*!!;
    print "\nI found:\n";
    print $comment{$d};
    print "$newd\t$directive{$d}\n";
    print "\n";
    print "If this is a directive that you hacked in, you probably should retain it.\n";
    print "If not, it's probably an obsolete directive from an earlier release,\n";
    print "and you need not retain it.\n";
    print "Do you want to retain this in your $old file? [y] ";
    $yn = <STDIN>;
    if ($yn !~ /^[Nn]/) {
	print "Retaining directive. It will appear at the end of $old.\n";
        @retained = (@retained, $newd);
	print OLD $comment{$d};
	print OLD "$newd\t$directive{$d}\n";
	print OLD "\n";
    } else {
	print "Deleting definition.\n";
	@deleted = (@deleted, $d);
    }
}
close(OLD);

print "\nSummary of changes:\n";
print "New options from $new: ",join(" ",@newoptions),"\n";
print "Old options retained: ",join(" ",@retained),"\n";
print "Old options deleted: ",join(" ",@deleted),"\n";
print "If this is wrong, you can recover $old from $bak.\n";
print "Done!\n";
exit 0;
