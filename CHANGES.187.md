% PennMUSH 1.8.7 Changes
%
% Last release: ??? ?? 20??

This is the most current changes file for PennMUSH. Please look it over; each version contains new things which might significantly affect the function of your server.  Changes are reported in reverse chronological order (most recent first)

* [SW] is Shawn Wagner, a PennMUSH developer (aka Raevnos), also responsible for most unattributed changes.
* [GM] is Greg Millam, a PennMUSH developer (aka Walker)
* [MG] is Mike Griffiths, a PennMUSH developer (aka Talvo)
* [TK] is Tim Krajcar, a PennMUSH developer (aka Rince)
* [3] refers to code by (or inspired by) TinyMUSH 3.0
* [MUX] refers to code by (or inspired by) TinyMUX 2.x
* [Rhost] refers to code by (or inspired by) RhostMUSH

Numbers next to the developer credit refer to Github issue numbers.

-------------------------------------------------------------------------------

Version 1.8.7 patchlevel 0 ??? ?? 20??
======================================

Major Changes
-------------

* Support websocket connections. See <https://github.com/grapenut/websockclient> for a
  sample in-browser client. [Grapenut, 1007]

Softcode
--------

* Support all of Rhost's colors() key arguments (Except n). [SW, 1112]

Fixes
-----

* A bunch of color names weren't mapping correctly to Xterm color codes. [SW]
* `@grep/iprint` hilites the matching text in the same case it appears in the attribute body. [SW, 1120]
* `@mail` wasn't updating a player's MAILCURF attribute correctly. [CLDawes, 1131]
* Connecting with a web browser to a mush without a mud_url config
  option set caused an infinite refresh loop. Reported by
  grapenut. [1149]
* Make sure sigrecv_ack() won't hang the mush if it somehow gets
  called at the wrong time. Also fix a file descriptor leak in the
  signal handling code. [SW]

Documentation
-------------

* Changelogs and other documentation use markup. [SW, 1140]
* Start trying to clean up and revise ancient documentation. [1095]

OS Specific
-----------

### OpenBSD ###

* netmush and slave processes use pledge(2) to limit their privileges. [SW]

### Windows ###

* Use Windows crypto library functions for base64 conversion and digest hashing instead of OpenSSL. [SW]

