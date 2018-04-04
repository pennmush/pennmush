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

Minor Changes
-------------

* Message translation support now defaults to off. Run configure with `--enable-nls` to turn it on if needed. [SW]
* Shrink the size of several structs, for memory savings, especially in softcode that queues lots of commands. [SW]

Softcode
--------

* Support all of Rhost's colors() key arguments (Except n). [SW, 1112]
* Functions that work on integers (Like div() or band()) now use 64-bit values instead of 32-bit. [SW]

Fixes
-----

* A bunch of color names weren't mapping correctly to Xterm color codes. [SW]
* `@grep/iprint` hilites the matching text in the same case it appears in the attribute body. [SW, 1120]
* `@mail` wasn't updating a player's MAILCURF attribute correctly. [CLDawes, 1131]
* Connecting with a web browser to a mush without a mud_url config option set caused an infinite refresh loop. Reported by grapenut. [1149]
* Make sure sigrecv_ack() won't hang the mush if it somehow gets called at the wrong time. Also fix a file descriptor leak in the signal handling code. [SW]
* Pass pe_info into IDLE and HAVEN attributes from the page command. [MG]
* The x and X options to align() now always truncate to the column width, rather than incorrectly truncating at a space. Reported by Qon. [MG, 1178]
* json_query() didn't understand an action of 'type' as documented. [SW]

Documentation
-------------

* Changelogs and other documentation use markup. [SW, 1140]
* Start trying to clean up and revise ancient documentation. [1095]

OS Specific
-----------

### BSDs in general ###

* info_slave and ssl_slave use kqueue() to efficiently be notified of parent mush crashes.

### OpenBSD ###

* netmush and slave processes use pledge(2) to limit their privileges. [SW]

### Windows ###

* Use Windows crypto library functions for base64 conversion and digest hashing instead of OpenSSL. [SW]

