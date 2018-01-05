% Frequently asked questions about the PennMUSH Server, 1.8.7

What's the release history since 1.50pl10?
==========================================

PennMUSH has been around for a long time. The above-mentioned 1.50p10
(1.5.0p10) release was made in 1995, and Penn wasn't new then.

PennMUSH 1.50pl10 was the last patchlevel of PennMUSH developed by Amberyl.
Amberyl handed over the maintenance, development, and support of
PennMUSH to Javelin/Paul (Alan Schwartz) after 1.50pl10.

The first two post-pl10 releases were termed the "dune-1" and "dune-2"
releases (in honor of DuneMUSH, where Alan did most of his development
work). Amberyl and Javelin agreed that it was silly to start a whole
new numbering scheme, so the next patchlevel released was pl11.

Javelin, along with the other two PennMUSH developers, T. Alexander
Popiel and Ralph Melton, made so many internal changes that it was
time for a new numbering scheme, and PennMUSH was advanced to 1.6.x.

Ralph Melton has since retired, and Thorvald Natvig took his place on
the devteam. He rewrote the command parser, and PennMUSH was advanced
to 1.7.0. Talek and Thorvald have both since retired, and many other
people have submitted code, both as official developers or community
contributors over the years.

In July 2006, Raevnos took over from Javelin as maintainer. Currently,
the active development team is Raevnos, Walker, Mike, Intrevis, and Rince.
The current version is 1.8.7.

How do I ask for help with a problem?
=====================================

There are several options:

 * Ask on `M*U*S*H`, a game where the PennMUSH developers and many other
   talented folk hang out. mush.pennmush.org 4201
 * File an issue with the devs (See the link below for the github bug tracker)

How do I report a bug?
======================

Visit the [issue tracker](https://github.com/pennmush/pennmush/issues).

Include specific information: PennMUSH version, OS, how to reproduce
the problem, what local changes you've made to the source. If you know
what's causing the bug, or how to fix it, or if you have a patch for
the bug, send it along. If you don't, and the bug caused a crash with
a core dump, you can send along a stack trace.

How do I request a new feature?
===============================

Visit the [issue tracker](https://github.com/pennmush/pennmush/issues).

No promises, but we try to get back to you about the feasibility of
suggestions quickly, and implement them as we can. Features that come
with a patch implementing them tend to get accepted faster than those
that don't unless it's a trivial addition.

Where can I get more information about admin'ing and hacking MUSH?
==================================================================

Read
[Javelin's God for PennMUSH Gods](http://download.pennmush.org/Guide/),
loads of info about setting up a MUSH, hacking source code, daily
maintenance, and many tips from other Gods! It's a bit dated in some
respects but still very useful.

The [community portal](http://community.pennmush.org) also has guides
and reference material for working with mush hardcode that are
slightly less out of date.

The source code has
[doxygen documentation](http://doxygen.pennmush.org) that can be
browsed.

Where can I hear about new releases?
====================================

New releases of the PennMUSH code are announced on `M*U*S*H`
(mush.pennmush.org 4201).

Why doesn't %t or space() work right for Pueblo clients?
========================================================

Actually, it does. Pueblo is built around an HTML browser. In HTML,
multiple whitespace is ignored and treated as a single space.  This is
correct behavior. In HTML, if you really want spaces to count as
spaces, you must put your text in `<PRE>..</PRE>` blocks,
e.g. `tagwrap(PRE,this %t has %t tabs %t and %b%b spaces`).

What signals does PennMUSH understand?
======================================

PennMUSH understands the following signals, and performs the listed action:

Name  Number Description
----  ------ -----------
HUP        1 Performs a silent `@readcache`
USR1      16 Performs an `@shutdown/reboot`
USR2      17 Performs an `@dump`
INT        2 Performs an `@shutdown`
TERM      15 Performs an `@shutdown/panic`

