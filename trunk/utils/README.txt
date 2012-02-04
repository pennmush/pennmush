The utils directory has assorted scripts used as part of the build
process, and source code for programs used manually at times to update
various things that don't change often. 

Detailed information about how to use the scripts can usually be found
in comments in them Here's a quick overview of what they're for:

change2html.pl: Script that HTML-ifies changelog entries for release
 announcements.

clwrapper.sh: A wrapper around the cl compiler from Microsoft.

columnize.scm: Script to format the tables of functions/commands/etc.
 in help files.

cscope.sh: Wrapper script for invoking cscope, a handy source code
 browsing/searching tool.

customize.pl: perl script used by 'make customize'

fixdepend.pl: perl script used by 'make distdepend'

fixdiff.scm:  scheme script to convert a diff with Windows-style path
 separators to Unix-style ones.
  
gentables.c:  Compiles into a program used to make src/tables.c

grep-cl.pl: Perl script to search for strings in changelogs.

ln-dir.sh:    A manual alternative to make customize. Kinda.

make_access_cnf.sh: Script used to update ancient versions of Penn
 that used two files for sitelocks. 

mkcmds.pl: Perl script used by the makefile to create hdrs/funs.h,
 hdrs/cmds.h, hdrs/patches.h, hdrs/switches.h and src/switchinc.c when
 needed.

mkvershlp.pl: perl script that turns the CHANGES.* files into
 game/txt/hlp/pennv*.hlp files.

pwutil.pl: perl script used to manipulate player passwords in an
 offline database. Run it with --help for more.

splint.sh: Wrapper for the splint code analysis tool to control
 what warnings are printed out.

typedefs.scm: Compiles into a program used to update the list of
 typedefs used by 'make indent'.

update-cnf.pl: Used by make to reconcile changes between
 game/mushcnf.dst and your local game/mush.cnf.

update.pl: Used by make to reconcile changes between options.h.dist
 and your options.h.

