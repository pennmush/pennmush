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
  classes. Mandatory dependency for Penn already.
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

Penn Functions
==============

String Builders
---------------

### Fixed-size strings ###

Good old `BUFFER_LEN` long strings, manipulated by `safe_foo()`
functions. `safe_chr()`, `safe_str()` and `safe_strl()` should only be
used with ASCII strings. TODO: New `safe_uchar()` and `safe_ustr()`
functions for adding Unicode characters and UTF-8 strings
respectively. Used in old code.

### PennStrs ###

TODO:

Wrapper functions for Sqlite3 string builder functions, for making
arbitrary-length strings. Similar set of functions as the current
`safe_foo()` functions. Used in new code.

Character Classifications
-------------------------

* Ascii-only `isalpha()` etc.

* TODO: `uni_isalpha()` etc that are Unicode aware. Implementation
wise, prefer ICU versions if available, if not fall back on PCRE
regular expressions.

Regular Expressions
-------------------

ICU has a regular expression package, but stick with PCRE as ICU isn't
a mandatory dependency, and the ICU one lacks a lot of PCRE features.

UTF-8 Files
===========

The following files are treated like they're encoded in UTF-8:

* help files
* names.cnf

The following will be:

* Database files
* mush.cnf
* access.cnf
* Everything in game/txt

The config files should be ASCII-only so no conversion needs to be
done.

Note: Many of these shouldn't require anything special to be done
mush-side.

Converting files to UTF-8
-------------------------

Anything stock in the MUSH source tree can be changed; the issue is
user-added or changed files. Database files will have a new flag added
to indicate UTF-8. Other stuff... tell people with non-ascii stuff to
convert by hand with iconv:

    $ iconv -f iso-8859-1 -t utf-8 foo.txt

should do the trick.
