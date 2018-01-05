% User's Guide to PennMUSH 1.8.x

PennMUSH is a server used to create text-based multiplayer games.

Installation information can be found in the files
[INSTALL.md](INSTALL.html) or [UPGRADING.md](UPGRADING.html),
depending on whether it's a new install or an upgrade.  The file
[I18N](I18N.html) discusses internationalization.

You may also want to take a look at Javelin's Guide for PennMUSH Gods,
at <http://download.pennmush.org/Guide/>

Introduction and history
========================

PennMUSH uses a version-numbering system that includes version numbers
(like 1.7.2) and patchlevels (like p32), usually written together
(1.7.2p32).

PennMUSH is a TinyMUD derivative, and one of the branches along the
MUSH line. "Vanilla" TinyMUSH, which added the "v" registers and
functions to the basic TinyMUD building commands, was written by Larry
Foard. The code was later expanded by Jin, of MicroMUSH. In January of
1991, MicroMUSH changed its name to MicroMUSE, and the code there
continued to develop under the MUSE name. At that same point in time,
Moonchilde took the last public release of that code and began a
series of improvements and extensions.

That code was released as PernMUSH, named for the MUSH that Moonchilde
was running. The last released version of that code was version 1.15,
at the end of November 1991. PernMUSH itself had switched over to
TinyMUSH 2.0, which Moonchilde had co-written with Glenn Crocker
(Wizard of TinyCWRU); there was no longer a reason for Moonchilde to
maintain this code.

In January of 1992, Amberyl began working on the PernMUSH 1.15 code
release, for TinyKrynn. She took over the code, which no one was
supporting, and worked on extending this code, as well as improving
its compatibility with TinyMUSH 2.0.  She changed the name to PennMUSH
(named for her school, the University of Pennsylvania), to avoid the
confusion that resulted from PernMUSH actually running TinyMUSH 2.0.

In January of 1995, Amberyl passed on her mantle to Javelin (aka
Paul@Dune, Alan Schwartz), who continuted as the maintainer of the
primary public distribution in development. He released two
patchlevels numbered "dune-1" and "dune-2" before releasing PennMUSH
1.50 pl11 and later distributions. The numbering scheme changed again
with PennMUSH 1.6.0 (see CHANGES.OLD).

Gradually during the early part of 1995, Alan formed the PennMUSH
development team with T. Alexander Popiel (Talek) and Ralph Melton.
The development process became more formalized, with official patches,
a dedicated bug reporting email address, and better tracking of
outstanding issues and history.

In August of 1997, Ralph Melton left the PennMUSH development team,
and Thorvald Natvig joined as a new member.  Many thanks go to Ralph
who contributed much time, code, and good cheer to PennMUSH.  Since
that time, the development team has gained and lost members.  The
current membership is usually listed at the top of the latest
CHANGES.<version> file.

In November 2002, with the release of PennMUSH 1.7.6, PennMUSH began
using the Artistic License (see the COPYRITE file), an open
source/free software license. This license was simultaneously adopted
by TinyMUSH (2.2.5, 3.x) and TinyMUX to facilitate code sharing and
widen use.

In July 2006, Javelin retired from the role of Maintainer, passing the
mantle of lead developer to Raevnos. Many thanks go to Javelin whose
contributions and guidance of PennMUSH have shaped it into the
codebase that it is today.

If you are planning on modifying the source code to PennMUSH, you'll
probably want Javelin's Guide for PennMUSH Gods, which should be
available where you got this code, or, in hypertext, as
<http://download.pennmush.org/Guide/guide-single.html>. More recent
versions may be available at <http://community.pennmush.org>.

Enjoy!

Getting Help, Reporting Bugs
============================

Here are some guidelines about where and how to report bugs or
problems or generally look for help.

There are three places one could get help with a problem:

*  The PennMUSH bug-tracking site is
   <https://github.com/pennmush/pennmush/issues> To file a new report,
   click on the 'New issue' link. If you want to get emails about
   updates to the bug report, put your email address in the
   appropriate field. For 'type', please select the most appropriate
   category: Bug, suggested feature, documentation issue,
   build/compilation problems, etc. Be sure to include what version of
   PennMUSH you found the problem on.

   If the problem resulted in a crash and a core dump, a stack trace
   of the core dump should also be included.

   If we need additional stuff (like a log of the configure or make),
   we'll ask for it, but if you know that it's relevant, you can send
   it along, too.

   You can also search to see if anyone else has already reported the
   issue, see what issues have been fixed for upcoming releases, and
   much more at the site.

* The PennMUSH community portal is at <http://community.pennmush.org>

  There is documentation there about many aspects of working with
  mushes, and after you create an account, you can make blog posts
  asking for help.

* `M*U*S*H`, at mush.pennmush.org 4201 is where the devs and many other
   talented folk hang out.

Miscellaneous
=============

Announcing when a mush is down
------------------------------

If your mush is no longer running on a given port or server and you
want to and are able to leave a program running listening on that
port, you can tell people about a new location or other news.

Run:

    % make portmsg

Then start the port announcer with:

    % ./src/portmsg message.txt port#

Any connections to the given port will see the contents of the message
file and then be disconnected after a few seconds.

Running under gdb
-----------------

If you start the game through gdb (As opposed to attaching to a
running process) pass the `--no-session` argument to netmush/netmud to
avoid detaching from the controlling terminal (Done via `fork()` and
`setsid()` as part of the normal mush startup). If you don't know what
gdb is, don't worry about this.

