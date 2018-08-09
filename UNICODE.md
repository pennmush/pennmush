PennMUSH and Unicode, ICU Edition
=================================

Yet another attempt at adding decent Unicode support to Penn, a bit
less ambitious than the last, and thus more likely to actually make
progress.

The plan
========

Work is divided into several basic phases, with some overlap:

1. Use UTF-8 instead of Latin-1 for all strings. This will be a
   gradual transition, with lots of translating between the two
   character sets to start.
    * Use UTF-8 in databases and other files.
    * Use UTF-8 in attribute bodies and other stored text.
    * Use UTF-8 in commands and functions.
    * Add a `FN_UTF8` function flag that indicates that a softcode
      function takes and returns UTF-8 strings instead of
      Latin-1. `process_expression()` converts as needed. Same for the
      command parser.
    * Add a new function class that returns an arbitrary-length
      string instead.

2. Rewrite softcode functions to be UTF-8 aware. Ranges from nothing
   needing to be done, to difficult.
    * New `ansi_string` functions that work with UTF-8 strings.
    * new `safe_XXX()` functions that don't truncate UTF-8 sequences
      at the end of a `BUFFER_LEN` long string.
    * Try to make a single extended grapheme cluster act like a single
      character in softcode.

3. Add a variable-length string data type, and start converting from
   fixed length strings to it.

The tools
=========

* PCRE. Usually comes with support for UTF-8 and Unicode character
  classes. Mandatory dependency for Penn already. Now also mandatory
  is UTF-8 and UCP support. As far as I know all OSes already do this
  for their PCRE versions.
* Sqlite3. Uses UTF-8 internally, has some functions for dealing with
  Unicode. Bundled with Penn already.
* ICU. See below.

ICU
---

Very popular Unicode library, optionally used by Sqlite3, already
tested for in configure and used when present for expanded character
conversion functions in *charconv.c*. Don't require it, but
**strongly** encourage it. Operate at reduced capability without it.

Unfortunately, most ICU functions work with UTF-16 strings, not
UTF-8. Anything fancy requires converting between the two
encodings. Possibly consider using UTF-16 internally, though that
comes with its own host of problems - wasted space on mostly-ascii
games, serialization still needs to be done in UTF-8 to stay sane,
etc.

The language
------------

C11 has some support for Unicode, including `char16_t` and `char32_t`
types for UTF-16 and UTF-32 characters, but support is spotty enough
it's not really reliable. Lots of stuff in the standard library
depends on locales too, and I'd like to be as consistent across
different hosts and environments as possible.

Penn Functions for UTF-8
========================

String Builders
---------------

### Fixed-size strings ###

Good old `BUFFER_LEN` long strings, manipulated by `safe_foo()`
functions. `safe_uchar()` appends a Unicode codepoint in UTF-8 format.

### PennStrs ###

Wrapper functions for Sqlite3 string builder functions, for making
arbitrary-length strings. Similar set of functions as the current
`safe_foo()` functions. Use in new code over `BUFFER_LEN` strings.

Character Classifications
-------------------------

* `uni_isXXX()` functions are Unicode-aware versions of the ctype
  `isXXX()` functions. Uses ICU classification routines, falling back
  on PCRE regular expressions if not present.
* `ascii_isXXX()` functions take any Unicode codepoint but only return
  true for ones in ASCII range.

Case Mapping
------------

Complicated by how in Unicode a differently cased codepoint can
actually be turned into multiple codepoints from the original.

* `uni_tolower()` and `uni_toupper()` do simple codepoint to codepoint
  case mapping. Without ICU, they only work on ASCII letters.
* `ascii_tolower()` and `ascii_toupper()` only work on ASCII,
  returnining others unchanged.
* `strupper_a()` and `strlower_a()` do full Unicode case mapping with
  ICU. Without ICU, they only transform ASCII letters.
* `uupcasestr()` and `udowncasestr()` do simple in-place mapping of
  UTF-8 strings. Without ICU, they only transform ASCII letters. If a
  mapped character takes more bytes to encode (Or fewer) than the
  original, it's skipped.

String comparison
-----------------

For case-sensitive comparision, `strcmp()` should work with UTF-8.

For case-insensitive comparision:

* `sqlite3_stricmp()`, `sqlite3_strnicmp()` only does case folding of
  ASCII letters. Use when comparing against literal ASCII-only
  strings.
* `uni_strcasecmp()` and `uni_strncasecmp()` when ICU is used, does
  full case-insensitive comparison. Falls back to the sqlite3 routines
  otherwise.
* `uni_strcoll()` does a locale-dependant comparison with ICU,
  `strcmp()` without..

Regular Expressions
-------------------

ICU has a regular expression package, but stick with PCRE as ICU isn't
a mandatory dependency, and the ICU one lacks a lot of PCRE features.

Iteration
---------

* `for_each_cp()` calls a function for each codepoint in the string.

Miscellaneous
-------------

* `first_cp()` returns the first codepoint in a string. Wrapper for
  when the `U8_NEXT()` macro is awkward to use.
* `strlen_cp()` returns the number of codepoints in a string.

UTF-8 aware versions of assorted other functions in
*strutils.h*. Usually prefixed with a `u` over the ascii versions.

Extended Grapheme Clusters
--------------------------

A single rendered character can be composed of multiple codepoints. It
would be nice to make softcode functions treat a single EGC as a
single character, instead of one codepoint as one character, for most
cases.


UTF-8 Files
===========

The following files are encoded in UTF-8:

* help files
* names.cnf
* log files

The following will be:

* Database files
* mush.cnf
* access.cnf
* Everything in game/txt

The config files should be ASCII-only so no conversion needs to be
done.

Note: Many of these shouldn't require anything special to be done
mush-side.

All filenames should be ASCII on Windows.

Converting files to UTF-8
-------------------------

Anything stock in the MUSH source tree can be changed; the issue is
user-added or changed files. Database files will have a new flag added
to indicate UTF-8. Other stuff... tell people with non-ascii stuff to
convert by hand with iconv:

    $ iconv -f iso-8859-1 -t utf-8 foo.txt

should do the trick.
