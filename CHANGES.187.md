% PennMUSH 1.8.7 Changes
%
% Last release: ??? ?? 20??

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

Version 1.8.7 patchlevel 0 ??? ?? 20??
======================================

Major Changes
-------------

* Support websocket connections. See <https://github.com/grapenut/websockclient> for a sample in-browser client. [Grapenut, 1007]
* Change attributes from being stored in sorted linked lists to sorted arrays; results in faster lookups and less memory usage. [SW]
* Penn now comes with the Sqlite3 database engine bundled with it, and uses it internally in a few ways:
    * 3 different tables for looking up color names are combined into a single table.
    * Per-object auxilliary data keys (objdata) are handled in sql.
    * Player names and aliases are handled in sql, making some operations on them simpler.
    * Suggests alternatives for unknown function names, flags, powers and help entries, and a softcode interface to the suggestion engine.
    * @entrances and entrances() no longer scan the entire database.
    * Help files are stored in a database, with an expanded help/search that supports full text search. See `HELP SEARCHING` for details.
    * Optional enhanced connection logging. See the file *game/CONNLOG.md* for details.
    * A number of new softcode functions and expanded functions, listed below.

Minor Changes
-------------

* Message translation support now defaults to off. Run configure with `--enable-nls` to turn it on if needed. [SW]
* Shrink the `NEW_PE_INFO` struct, for signficant memory savings in softcode that queues lots of commands. [SW]
* Add more test cases to the softcode test suite. [SW]
* log_forces in mushcnf.dst now defaults to no. You probably only want this on if you're debugging. [MG]

Softcode
--------

* Support all of Rhost's `colors()` key arguments (Except n). [SW, 1112]
* Functions that work on integers (Like `div()` or `band()`) now use 64-bit values instead of 32-bit. [SW]
* Added `isjson()`
* `json_query()` get and exists can follow paths into objects instead of taking a single key/index. Suggested by qa'toq. [SW]
* `json_query()` can apply merge patches to json objects per <https://tools.ietf.org/html/rfc7396>
* `json_query(str, unescape)` handles unicode escape sequences.
* `json(string, foo)` escapes non-ascii characters.
* `clone()` now takes an optional fourth argument to act like `@clone/preserve` [797]
* New 'me' and 'inventory' flags for `scan()` give finer control of what to scan. [MG]
* `orflags()`, `orlflags()`, `andflags()`, `andlflags()`, and the power versions no longer return errors on unknown flags/powers. They instead treat the unknown one as if it wasn't set. Suggested by Qon. [1180].
* `sqlescape()` when using a sqlite3 connection no longer also requires MySQL.
* `timecalc()` and `secscalc()` for adding/subtracting intervals from times.
* `@suggest` and `suggest()` for user-defined word suggestion dictionaries.
* `connlog()` and `connrecord()` for interfacing with enhanced connection logs.
* `soundex()` and `soundslike()` now support a second phonetic hash besides soundex.

Fixes
-----

* A bunch of color names weren't mapping correctly to Xterm color codes. [SW]
* `@grep/iprint` hilites the matching text in the same case it appears in the attribute body. [SW, 1120]
* `@mail` wasn't updating a player's MAILCURF attribute correctly. [CLDawes, 1131]
* Connecting with a web browser to a mush without a `mud_url` config option set caused an infinite refresh loop. Reported by grapenut. [1149]
* Make sure `sigrecv_ack()` won't hang the mush if it somehow gets called at the wrong time. Also fix a file descriptor leak in the signal handling code. [SW]
* Pass `pe_info` into IDLE and HAVEN attributes from the page command. [MG]
* The x and X options to `align()` now always truncate to the column width, rather than incorrectly truncating at a space. Reported by Qon. [MG, 1178]
* `json_query()` didn't understand an action of 'type' as documented. [SW]
* Assorted help file fixes. [SW]
* `@clone` without /preserve wasn't stripping privileged flags and such. [1190,SW]
* `@chown/preserve` was resetting wiz-bit despite it's help file indicating otherwise. [1187] PR by Qon.
* `scan()` now determines if objects will be included based on whether the caller can examine them, rather than if `scan()`'s `<looker>` can examine them. [MG]
* Fixed some bugs regarding when `setq()` will and won't let you set the values of named registers when you've hit the limit. [MG, 1179]

Documentation
-------------

* Changelogs and other documentation use markup. [SW, 1140]
* Start trying to clean up and revise ancient documentation. [1095]
* Minor help fixes. [MG]
* More minor help fixes. [MT]

OS Specific
-----------

### BSDs in general ###

* `info_slave` and `ssl_slave` use `kqueue()` to efficiently be notified of parent mush crashes.

### OpenBSD ###

* netmush and slave processes use `pledge(2)` to limit their privileges. [SW]

### Windows ###

* Use Windows crypto library functions for base64 conversion and digest hashing instead of OpenSSL. [SW]

