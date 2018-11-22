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

WARNING! With the removal of the object queue, please be careful when upgrading that you do not have any infinitely looping triggers without an @wait.

As an example, this used to be a common way to ensure something was executed once per second:

  &gt; &amp;everysecond object=do some ; updates ; @trigger me/everysecond

This will now happen up to several thousand times per second! Add in an @wait 1, and it'll work as expected!

Major Changes
-------------

* Built-in HTTP server support, see "help http" [GM]
* A single command queue for players and objects. No more @trigger waits. [GM]
* A restructuring of bsd.c, to make it easier to reason about Penn's queue cycle. [GM]
* Millisecond timing in bsd.c for polling waits in prep for subsecond @waits. [GM]

Minor Changes
-------------

* Sockets commands now inline $-commands, so, e.g: $,* *: chat aliases don't hit queue. [GM]
* Millisecond timing in bsd.c for polling waits in prep for subsecond @waits. [GM]
* Sqlite3's `REGEXP` operator is always available and uses pcre regular expressions (previously it depended on libicu and used java style REs). [SW]
* Update `local.dst` to include example of millisecond callback loop. [MT]
* Updated to use PCRE2 10.31 for regular expressions. [SW]
* Wildcard patterns are sometimes converted to regular expressions when matched against many strings. [SW]
* Add '--disable-socket-quota' option for our test suite. [GM]
* The list of color definitions used with `ansi()`, `colors()`, etc. is now kept in game/txt/colors.json. [SW]
* Sqlite3 updated to 3.25.3. Biggest user-visible change is support for window functions. [SW]

Softcode
--------

* `addrlog()` for searching through list of unique IP addresses that have connected to a game. [SW]
* `connlog()` can return just a count of matching records. [SW]
* `formdecode()` for decoding HTTP paths and POST bodies. [GM]
* `@respond` for manipulating HTTP response codes and headers. [GM]
* `hmac()` for creating authentication fingerprints. [SW]
* `@chatformat` and channel mogrifiers are told if `@cemit/silent` is being used. [1267, SW]

Fixes
-----

* `add_function` in .cnf files was not properly using the upper case'd string. [#1223, MT]
* Various PCRE calls in the softcode have had CPU time limit watchdogs added. Discovered by Ashen-Shugar. [GM]
* Fix a file descriptor leak caused by recent OpenSSL versions. [SW]
* Added GAGGED restrictions that were missing from a few commands, including `@message` and the MUXcomm aliases. [MG]
* Minor help updates, including clarification of what GAGGED blocks, suggested by Merit. [#1262, MG, MT]
