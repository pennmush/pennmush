#!/usr/local/bin/perl
#
# If this script doesn't work for you, try changing the first line
# to point to the location of Perl on your system. That shouldn't
# be necessary if you're running it via 'make customize'
#
# customize.pl - Alan Schwartz <dunemush@pennmush.org>
#
# This script asks the user for a mush name and makes a copy of the
# game/ directory called <mush>/. It also rewrites the restart script
# to <mush>.restart and mush.cnf (which it calls <mush>.cnf) to
# make it easier to keep track of multiple MUSHes.
# 
# $Id: customize.pl 1.2 Tue, 09 Jan 2001 17:56:37 -0600 dunemush $
#

$tar1="(cd game; tar cf - .) | (cd ";
$tar2="; tar xfBp -)";

@dontwrite = ("src","hdrs","hints","game");


# Interact with the user
print <<END;
Welcome. This script creates a game directory for a MUSH and 
customizes the files in order to make running multiple MUSHes
easier. This is still experimental, but should not affect
any of your files in standard MUSH directories (game, src, hdrs, etc.)

When choosing the name for your directory, use a short version of
the MUSH name. For example, if you MUSH was called Fallen Angels MUSH,
you might choose 'fallen' or 'fa'. Use only upper- or lower-case
letters and numbers in directory names.

END

print "Please enter the name for your directory: ";
chop($targetdir = <STDIN>);

# Verify that the target directory isn't an unwritable one, or
# a tricky one.

$targetdir =~ s/ +//g;
die "Invalid directory: contains a bad character.\n"
	 if ($targetdir =~ /[^A-Za-z0-9_]/);
foreach (@dontwrite) {
  die "Invalid directory: already in use\n" if ($targetdir eq $_);
}

# Does the directory already exist?
if (-d $targetdir) {
  print "That directory already exists. Overwrite it? [n] ";
  $yn = <STDIN>;
  unless ($yn =~ /^[yY]/) {
	exit 0;
  }
}

# Ok, go ahead and create it. We probably should trap signals so
# we can clean up, too.

print "Making $targetdir...";
mkdir($targetdir,0755) unless (-d $targetdir);
die "Failed!\n" unless (-d $targetdir);
print "done.\n";

print "Using tar to copy from game/ to $targetdir/...";
$tar = $tar1 . $targetdir . $tar2;
if (system($tar)) {
	die "Failed!\n";
}
print "done.\n";

print "Replacing standard files in $targetdir/txt/hlp with\nlinks to files in game/txt/hlp...";
chop($curdir = `pwd`);
foreach $file (<$targetdir/txt/hlp/penn*.hlp>) {
  unlink($file) || die "Failed!\n";
  ($newfile) = $file =~ /(penn.*\.hlp)/;
  symlink("$curdir/game/txt/hlp/$newfile","$targetdir/txt/hlp/$newfile") || die "Failed!\n";
}
print "done.\n";

# Enter the directory, and, produce some files
chdir($targetdir);
chop($fullpath = `pwd`);

print "Modifying mush.cnf (to $targetdir.cnf)...";
unless (open(IN,"mush.cnf") && open(OUT,">$targetdir.cnf")) {
	die "Failed!\n";
}

while (<IN>) {
  if (/^mud_name/) {
	print OUT "mud_name $targetdir\n";
  } elsif (/^input_database/) {
	print OUT "input_database data/indb.$targetdir\n";
  } elsif (/^output_database/) {
	print OUT "output_database data/outdb.$targetdir\n";
  } elsif (/^mail_database/) {
	print OUT "mail_database data/maildb.$targetdir\n";
  } elsif (/^chat_database/) {
	print OUT "chat_database data/chatdb.$targetdir\n";
  } else {
	print OUT $_;
  }
}
close(IN); close(OUT);
print "done\n";


print "Modifying restart (to $targetdir.restart)...";
unless (open(IN,"restart") && open(OUT,">$targetdir.restart")) {
	die "Failed!\n";
}
while (<IN>) {
  if (/^GAMEDIR/) {
   	print OUT "GAMEDIR=$fullpath\n";
  } elsif (/^CONF_FILE/) {
	print OUT "CONF_FILE=$targetdir.cnf\n";
  } elsif (/^LOG/) {
	print OUT "LOG=log/$targetdir.log\n";
  } else {
	print OUT $_;
  }
}
close(IN); close(OUT);
chmod(0744,"$targetdir.restart");
print "done\n";

print "Renaming data/minimal.db.Z to data/indb.$targetdir.Z...";
rename("data/minimal.db.Z","data/indb.$targetdir.Z");
print "done.\n";

print "Removing old mush.cnf and restart script...";
unlink("mush.cnf");
unlink("restart");
print "done\n";

print "Fixing links...";
symlink("../src/netmud","netmush");
symlink("../src/info_slave","info_slave");
print "done\n";

chdir("..");

print "Customization complete for $targetdir/\n";

exit 0;
