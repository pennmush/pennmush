PennMUSH and Unicode, ICU Edition
=================================

Yet another attempt at adding decent Unicode support to Penn, a bit
less ambitious than the last, and thus more likely to actually make
progress.

The plan
--------

Work is divided into several basic phases, with some overlap:

1. Use UTF-8 instead of Latin-1 for all strings. This will be a
   gradual transition, with lots of translating between the two
   character sets to start.
    * Use UTF-8 in databases.
    * Use UTF-8 in attribute bodies.
    * Add a `FN_UTF8` function flag that indicates that a softcode
      function takes and returns UTF-8 strings instead of
      Latin-1. `process_expression()` converts as needed. Same for the
      command parser.

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
---------

* PCRE. Usually comes with support for UTF-8 and Unicode character
  classes. Mandatory dependency for Penn already.
* Sqlite3. Uses UTF-8 internally, has some functions for dealing with
  Unicode. Bundled with Penn already.
* ICU. See below.

### ICU ###

Very popular Unicode library, optionally used by Sqlite3, already
tested for in configure and used when present for expanded character
conversion functions in *charconv.c*. Don't require it, but
**strongly** encourage it. Operate at reduced capability without it.

ICU has a very permissive license in recent versions. Take the UTF-8
handling macros and functions from it and include them in Penn even
when the library isn't installed on a server. But not the whole thing,
because it's huge.

Unfortunately, most ICU functions work with UTF-16 strings, not
UTF-8. Anything fancy requires converting between the two
encodings. Possibly consider using UTF-16 internally, though that
comes with its own host of problems - wasted space on mostly-ascii
games, serialization still needs to be done in UTF-8 to stay sane,
etc.

### The language ###

C11 has some support for Unicode, including `char16_t` and `char32_t`
types for UTF-16 and UTF-32 characters, but support is spotty enough
it's not really reliable. Lots of stuff in the standard library
depends on locales too, and I'd like to be as consistent across
different hosts and environments as possible.

