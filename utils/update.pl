#!/usr/local/bin/perl
#
# You make have to change the path to perl above, unless you call this
# script with 'make update'
#
# update.pl - integrate previous options.h settings with new options.h.dist
#             results appear in options.h
#
# Usage: update.pl old-file new-file
#  e.g.: update.pl options.h options.h.dist
#
# 'make update' calls this.
#
# Here's how it works.
# 1. We make a backup of your old-file to old-file.bak
# 2. We read all the #def's in the old-file, and their
#  associated comments. Associated comments means comments
#  on the same line, after the define, or comments on lines
#  preceding the define. We store the names of all the defines,
#  their comments, and whether they're defined or not.
# 3. We check to see if there's are enviroment variables named DEFINE
#  or UNDEFINE. If so we parse them. DEFINE may contain NAMEs or
#  NAME=value pairs. UNDEFINE should contain only NAMEs.
#  We consider these as if they were present in old-file. 
#  These override old-file.
# 4. We read in the new-file. If we find a define
#  that wasn't in the old-file, we show the user the comment
#  and ask them how they want it set. Every time we write out
#  a define, we delete it from the list of defines from old-file
# 5. Finally, if there's anything left from old-file that's not in
#  new-file, we ask if the user would like to retain each one.
#  Presumably users want to retain their custom defines, but don't
#  want to retain obsoleted defines. Retained defines appear at
#  the end of the file.

die "Usage: update.pl old-file new-file\n" unless scalar @ARGV == 2;

use File::Copy;
use subs qw/ask_value ask_simple def/;

my ($old, $new) = @ARGV;

$bak = $old . ".bak";

# Part 1 - back up the old file
if (-r $old) {
    print "*** Backing up $old to $bak...\n";
    copy $old, $bak or die "Unable to copy $old to $bak: $!\n";
}

# Part 2 - read the settings from the old file and store them
if (-r $old) {
    print "*** Reading your settings from $old...\n";
    open OLD, "<", $old or die "update.pl: Unable to open $old: $!\n"; 
    while (<OLD>) {
	# There are a few possibilities for what we could have:
	# an #ifdef, #ifndef, #else, #endif, #define, #undef,
	# commented #define, comment text, etc. We only care
	# about the settings of define/undefs
	s#/\*\s*\*/##;
	s#[ \t]+([\r\n]*)$#$1#;
	if (/^#define\s+([A-Z0-9_-]+).*\\$/) {
	    # A define with a continuation, we need the next line
	    chop($next = <OLD>);
	    $defs{$1} = $next;
	    $comment{$1} = $comment;
	} elsif (m!^#define\s+([A-Z0-9_-]+)\s+(.+)\s+(/\*.*\*/)!) {
	    # A define with a value and a comment
	    $name = $1;
	    $comment{$name} = $3;
	    $defs{$name} = $2;
	    undef $comment;
        } elsif (m!^#define\s+([A-Z0-9_-]+)\s+(.+)!) {
	    # A define with a value
	    $defs{$1} = $2;
	    $comment{$1} = $comment;
        } elsif (/^#undef\s+([A-Z0-9_-]+)/) {
	    # an undef
	    $defs{$1} = 'undef';
	    $comment{$1} = $comment;
        } elsif (m!^/\*\s*#define\s+([A-Z0-9_-]+)\s+(.+)\s+(/\*.*\*/)!) {
	    # A commented define with a value and a comment
	    $name = $1;
	    $comment{$name} = $3;
	    $cvaldef{$name}++;
	    $defs{$name} = $2;
	    undef $comment;
        } elsif (/^(\/\*)*\s*#define\s+([A-Z0-9-][A-Z0-9_-]+)/) {
	    # a define or commented define
	    $defs{$2} = ($1 eq "/*") ? 'undef' : 'define';
	    $comment{$2} = $comment;
        } else {
	    if (m#^\s*/\*#) {
		# Start of a comment
		$incomment = 1;
		undef $comment;
	    }
	    if ($incomment) {
		$comment = $comment . $_;
		if (m#\*/\s+$#) {
		    # End of a comment
		    $incomment = 0;
		}
	    }
        }
    }
    close(OLD);
}
undef $comment; $incomment = 0;


# Part 3 - Check to see if we have environment variable SETTINGS and
#          use those settings as if they were in the old file.
if ($settings = $ENV{'DEFINE'}) {
    print "\n*** Found a DEFINE environment variable - applying settings...\n";
    my @pairs = split ' ', $settings;
    foreach (@pairs) {
	if (/(.+)=(.+)/) {
	    $defs{$1} = $2;
	} else {
	    $defs{$_} = 'define';
	}
    }
}
if ($settings = $ENV{'UNDEFINE'}) {
    print "\n*** Found an UNDEFINE environment variable - applying settings...\n";
    my @pairs = split ' ', $settings;
    foreach (@pairs) {
	$defs{$_} = 'undef';
    }
}

# Part 4 - read in the new file, modifying its definition lines to
#          match the old file. If we come across a definition that
#          isn't in the old file, ask the user about it. 
print "*** Updating $old from $new...\n";
open OLD, ">", $old or die "update.pl: Unable to open $old: $!\n"; 
open NEW, "<", $new or die "update.pl: Unable to open $new: $!\n"; 
$_ = <NEW>;
while ($next = <NEW>) {
    # Just like before, but we need to keep track of
    # comments in the file so that we can describe options
    s#[ \t]+([\r\n]*)$#$1#;
    if (/^#define\s+([A-Z0-9_-]+).*\\$/) {
	# A define with a continuation, we need the next line
	print OLD "#define $1 \\\n";
	ask_value($1,$next) if (!defined($defs{$1}));
	print OLD $defs{$1};
	delete $defs{$1};
	$next = <NEW>;
    } elsif (/^#define\s+([A-Z0-9-][A-Z0-9_-]+)\s+\/\*\s*\*\//) {
	# a define followed by /* */
	print OLD defined($defs{$1}) ? def($1) 
	    : def(ask_simple($1,'define'));
    } elsif (m!^(/\*\s*)?#define\s+([A-Z0-9_-]+)\s+(.+)\s+(/\*.*\*/)!) {
	# A define with a value and a comment
        $maybeundef = $1;
	$maybecomment = $4; $name = $2;
	$olddef = $def = $3;
	$comment = $maybecomment if ($maybecomment =~ /\w/);
	$def = "undef" if $maybeundef =~ /./;
	print OLD defined($defs{$name}) 
	    ? def($name,$comment) : def(ask_value($name,$def),$maybecomment,$def eq "undef" ? $olddef : $def);
    } elsif (m!^#define\s+([A-Z0-9_-]+)\s+(.+)!) {
	# A define with a value
	print OLD defined($defs{$1}) ? def($1) : def(ask_value($1,$2));
    } elsif (/^#undef\s+([A-Z0-9_-]+)/) {
	print OLD defined($defs{$1}) ? def($1) 
	    : def(ask_simple($1,'undef'));
    } elsif (/^(\/\*)*\s*#define\s+([A-Z0-9-][A-Z0-9_-]+)/) {
	# a define or commented define
	print OLD defined($defs{$2}) ?
	    def($2)
	    : def(ask_simple($2,($1 eq "/*" ? 'undef': 'define')));
    } else {
	if (m#^\s*/\*#) {
	    # Start of a comment
	    $incomment = 1;
	    undef $comment;
	}
	if ($incomment) {
	    $comment .= $_;
	    if (m#\*/\s+$#) {
		# End of a comment
		$incomment = 0;
	    }
	}
	print OLD;
    }
    $_ = $next;
}
# At the end of that loop, $_ contains the last line of the
# file, which should be the #endif.
$final = $_;
close NEW;

# Part 5 - if there are any definitions left from the old file,
#          offer to delete them (or not)
print "\n*** Checking for leftover defines from $old...\n";
while (my ($d, $val) = each %defs) {
    print "\nI found: $d\n";
    if ($val eq 'undef') {
	print "Currently undefined\n";
    } elsif ($val eq 'define') {
	print "Currently defined\n";
    } else {
	print "Definition: $val\n";
    }
    print $comment{$d}, "\n";
    print "If this is a define that you hacked in, you probably should retain it.\n";
    print "If not, it's probably an obsolete define from an earlier patchlevel,\n";
    print "and you need not retain it.\n";
    print "Do you want to retain this in your $old file? [y] ";
    $yn = <STDIN>;
    if ($yn !~ /^n/i) {
	print "Retaining definition. It will appear at the end of $old.\n";
        push @retained, $d;
	print OLD $comment{$d};
	print OLD def($d);
	print OLD "\n";
    } else {
	print "Deleting definition.\n";
	push @deleted, $d;
    }
}

print OLD $final;

close OLD;

print "\nSummary of changes:\n";
print "New options from $new: ", join(", ",@newoptions), "\n";
print "Old options retained: ", join(", ",@retained), "\n";
print "Old options deleted: ", join(", ",@deleted), "\n";
print "If this is wrong, you can recover $old from $bak.\n";
print "Done!\n";


#
# &def - Given a define name, return the appopriate C code
# to define/undefine it. And delete it.
# May also be given a comment as a second arg.
#
sub def {
    my ($d,$c,$oldval) = @_;
    my $df = $defs{$d};
    delete $defs{$d};
    $d =~ s/^\s+//;
    $d =~ s/\s+$//;
    $c =~ s/^\s+//;
    $c =~ s/\s+$//;
    $df =~ s/^\s+//;
    $df =~ s/\s+$//;
    if ($df eq 'undef') {
	if (defined($oldval) and $oldval) {
	    $oldval =~ s/^\s+//;
	    $oldval =~ s/\s+$//;
	    return "/* #define $d\t$oldval /* */\n";
	} else {
	    return "/* #define $d /* */\n";
	}
    }
    return "#define $d /* */\n" if ($df eq 'define');
    return "/* #define $d\t$df\t$c\n" if ($cvaldef{$d} and $c);
    return "#define $d\t$df\t$c\n" if ($c);
    return "#define $d\t$df\n";
}

#
# &ask_simple - Given a define name and default setting,
# show the comment in $comment,
# and ask the user if they want to define it or not
# Set $defs{$d} and return the name given
#
sub ask_simple {
    my ($d,$s) = @_;
    my $yn;
    print "\nNew option: $d\n";
    print $comment;
    $s = ($s eq 'define') ? 'y' : 'n';
    while (1) {
	print "Define this option? [$s] ";
	$yn = <STDIN>;
	$yn = $s if $yn =~ /^$/;
	last if $yn =~ /^[yn]/i;
    }
    $defs{$d} = ($yn =~ /^y/i) ? 'define' : 'undef';
    push @newoptions, $d;
    return $d;
}


#
# &ask_value - Just like ask_simple, but instead of a yes/no,
# we're going to get a value
#
sub ask_value {
    my ($d,$s) = @_;
    my $val;
    print "\nNew option: $d\n";
    print "$comment\n" unless ($comment =~ /^\s*\/\*\s*\*\/\s*$/);
    print "Default value: $s\n";
    print "Value for this option? (undef to undefine) [$s] ";
    $val = <STDIN>;
    $val = $s if $val =~ /^$/;
    $defs{$d} = $val;
    push @newoptions, $d;
    return $d;
}

