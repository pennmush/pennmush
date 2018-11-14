% PennMUSH 1.8.7 Changes
%
% Last release: Aug 10 2018

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

Version 1.8.7 patchlevel 1 Nov 20 2018
======================================

Minor Changes
-------------

* `help/search` takes a `/brief` switch to display just the names of
  matching entries. [SW]
* help full text search includes @ and + in tokens, so things like
  `help/search "@force"` work as expected. Also enabled porter
  stemming of search phrases (Searching for *wizard* matches
  *wizards*). Noticed by [MG]. [SW]
* Add `help/find` which mimics the pre-1.8.7 behavior of
  `help/search`. Suggested by [MG]. [SW]

Fixes
-----

* `connrecord()` returns an error if extended connection logging is disabled. [SW]
* `connlog()` didn't handle future dates very well. [SW]
* dbtools programs couldn't handle attributes with quote marks in the name. Reported by [MG]. [SW,1228]
* `@http` requests in-progress during a `@shutdown/reboot` would leak sockets. [SW,1246]
* `lattrp()` and similar functions didn't behave the same with attribute trees as 1.8.6. Reported by Mercutio. [SW,1233]
* Recursive calls to `json_map()` were broken. Reported by Mercutio [SW,1252]
* Fixed a potential overflow bug in `@search`. Reported by eery. [MG]

Version 1.8.7 patchlevel 0 Aug 10 2018
======================================

Major Changes
-------------

* Support websocket connections. See <https://github.com/grapenut/websockclient> for a sample in-browser client. [Grapenut, 1007]
* Change attributes from being stored in sorted linked lists to sorted arrays; results in faster lookups and less memory usage. [SW]
* Penn now comes with the Sqlite3 database engine bundled with it, and uses it internally in a few ways:
    * 3 different tables for looking up color names are combined into a single table.
    * Per-object auxilliary data keys (objdata) are handled in sql.
    * Player names and alias lists are handled in sql, making some operations on them simpler.
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
* The connect screen now respects SOCKSET options. [MG]
* @chan/what now displays channel locks. [MT, 1208]

Softcode
--------

* Support all of Rhost's `colors()` key arguments (Except n). [SW, 1112]
* Functions that work on integers (Like `div()` or `band()`) now use 64-bit values instead of 32-bit. [SW]
* Added `isjson()`
* `json_query()` get and exists can follow paths into objects instead of taking a single key/index. Suggested by qa'toq. [SW]
* `json_mod()` for modifying complex JSON objects. [SW]
* `json_query(str, unescape)` handles unicode escape sequences.
* `json(string, foo)` escapes non-ascii characters.
* `clone()` now takes an optional fourth argument to act like `@clone/preserve` [797]
* New 'me' and 'inventory' flags for `scan()` give finer control of what to scan. [MG]
* `orflags()`, `orlflags()`, `andflags()`, `andlflags()`, and the power versions no longer return errors on unknown flags/powers. They instead treat the unknown one as if it wasn't set. Suggested by Qon. [1180].
* `timecalc()` and `secscalc()` for adding/subtracting intervals from times.
* `@suggest` and `suggest()` for user-defined spellchecking. Loads */usr/share/dict/words* or another configurable wordlist by default.
* `connlog()` and `connrecord()` for interfacing with enhanced connection logs.
* `soundex()` and `soundslike()` now support a second phonetic hash algorithm besides soundex.
* Side-effect version of `link()` now returns 1 on success, 0 or #-1 on failure. [MT]
* `owner()` now accepts two optional arguments, allowing ownership to be changed as in `@chown` and `@atrchown`. [MT]
* If compiled with libcurl support, adds `@http` for interacting with RESTFul web APIs. [SW]
* `stripaccents()` supports a second, smarter, transliteration algorithm.
* If compiled with ICU support, adds `lcstr2()` and `ucstr2()` with proper support for characters like the German eszett (ß) that map to a different number of characters in different cases.
* `@chatformat` now receives a new arg, `%6`, which defaults to "says" but may be replaced by the speechtext mogrifier. Inspired by Bodin. [MG]
* `etimefmt()` supports `$w` and `$y` formats for weeks and years. [SW, 804]

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
* `@clone` without /preserve wasn't stripping privileged flags and such. [1190,SW]
* `@chown/preserve` was resetting wiz-bit despite it's help file indicating otherwise. [1187] PR by Qon.
* `scan()` now determines if objects will be included based on whether the caller can examine them, rather than if `scan()`'s `<looker>` can examine them. [MG]
* Fixed some bugs regarding when `setq()` will and won't let you set the values of named registers when you've hit the limit. [MG, 1179]
* `sqlescape()` when using a sqlite3 connection no longer also requires MySQL.
* A number of issues in the handling UTF-8 text sent by clients have been fixed, as well as improvements in UTF-8 handling in general. [SW]
* Fix an off-by-one error in command switch initialization code. [SW]
* `@mail` without a message list respects the current folder instead of using folder 0. [77]
* `ufun()`, `ulocal()`, etc. could get confused by ansi (markup) in the attribute name. Strip markup first. [MT]
* Fix a long-standing bug where input sent right after a SSL connection could get lost. [SW]

Documentation
-------------

* Changelogs and other documentation use markup. [SW, 1140]
* Start trying to clean up and revise ancient documentation. [1095]
* Help fixes and improvements. [MG, SW, MT]
* Help files are now in UTF-8.

OS Specific
-----------

### BSDs in general ###

* `info_slave` and `ssl_slave` use `kqueue()` to efficiently be notified of parent mush crashes.

### OpenBSD ###

* netmush and slave processes use `pledge(2)` to limit their privileges. [SW]

### Windows ###

* Use Windows crypto library functions for base64 conversion and digest hashing instead of OpenSSL. [SW]
