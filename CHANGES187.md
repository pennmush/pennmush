PennMUSH 1.8.7 Changes
======================

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

Version 1.8.7p0, ??? ?? 20??
----------------------------

### Documentation ###

* Changelog's using Markdown. Other docs and help files might follow.
* Start trying to clean up and revise ancient documentation.

### OS Specific ###

#### OpenBSD ####

* info_slave uses pledge(2) to limit its privileges.

#### Windows ####

* Use Windows crypto library functions for base64 conversion and digest hashing instead of OpenSSL.

