% PennMUSH 1.8.8 Changes
%
% Last release: ??? ?? 2018

This is the most current changes file for PennMUSH. Please look it over; each version contains new things which might significantly affect the function of your server.  Changes are reported in reverse chronological order (most recent first)

* [SW] is Shawn Wagner, a PennMUSH developer (aka Raevnos), also responsible for most unattributed changes.
* [GM] is Greg Millam, a PennMUSH developer (aka Walker)
* [MG] is Mike Griffiths, a PennMUSH developer (aka Talvo)
* [TK] is Tim Krajcar, a PennMUSH developer (aka Rince)
* [MT] is Mike Taylor, a PennMUSH developer (aka Qon or Zenithar)
* [3] refers to code by (or inspired by) TinyMUSH 3.0
* [MUX] refers to code by (or inspired by) TinyMUX 2.x
* [Rhost] refers to code by (or inspired by) RhostMUSH

Numbers next to the developer credit refer to Github issue numbers.

-------------------------------------------------------------------------------

Version 1.8.8 patchlevel 0 ??? ?? 2018
======================================

Major Changes
-------------

* Built-in HTTP server support, see "help http" [GM]

Minor Changes
-------------

* Millisecond timing in bsd.c for polling waits in prep for subsecond @waits. [GM]

Softcode
--------

* `addrlog()` for searching through list of unique IP addresses that have connected to a game. [SW]
* `connlog()` can return just a count of matching records. [SW]
* `formdecode()` for decoding HTTP paths and POST bodies. [GM]
* `@respond` for manipulating HTTP response codes and headers. [GM]
* `hmac()` for creating authentication fingerprints. [SW]

Fixes
-----

* add_function in .cnf files was not properly using the upper case'd string. [#1223, MT]
