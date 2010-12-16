/*************************************************
*      Perl-Compatible Regular Expressions       *
*************************************************/


/* PCRE is a library of functions to support regular expressions whose syntax
and semantics are as close as possible to those of the Perl 5 language.

                       Written by Philip Hazel
           Copyright (c) 1997-2005 University of Cambridge

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/

/* Modified by Shawn Wagner for PennMUSH to fit in one file and remove
   things we don't use, like a bunch of API functions and utf-8
   support. If you want the full thing, see http://www.pcre.org. */
/* Modified by Alan Schwartz for PennMUSH to change the use of
 * 'isblank' as a variable (reported to Philip Hazel for pcre 4.5) */
/* Modified to disable warnings I don't feel like fixing */

#include <string.h>
#include "config.h"
#include "mypcre.h"

/* Only use if a system libpcre isn't present. */
#ifndef HAVE_PCRE

#ifndef WIN32
#pragma GCC diagnostic warning "-Wclobbered"
#endif

#include <ctype.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "confmagic.h"
#undef min
#undef max

/* Bits of PCRE's conf.h */
#define NEWLINE '\n'
#define LINK_SIZE   2
#define MATCH_LIMIT 100000
#define NO_RECURSE
#define EBCDIC 0

/* Bits of pcre_internal.h */



#define PCRE_DEFINITION         /* Win32 __declspec(export) trigger for .dll */

typedef unsigned char uschar;

/* We need to have types that specify unsigned 16-bit and 32-bit integers. We
cannot determine these outside the compilation (e.g. by running a program as
part of "configure") because PCRE is often cross-compiled for use on other
systems. Instead we make use of the maximum sizes that are available at
preprocessor time in standard C environments. */

#if USHRT_MAX == 65535
typedef unsigned short pcre_uint16;
#elif UINT_MAX == 65535
typedef unsigned int pcre_uint16;
#else
#error Cannot determine a type for 16-bit unsigned integers
#endif

#if UINT_MAX == 4294967295
typedef unsigned int pcre_uint32;
#elif ULONG_MAX == 4294967295
typedef unsigned long int pcre_uint32;
#else
#error Cannot determine a type for 32-bit unsigned integers
#endif


/* PCRE keeps offsets in its compiled code as 2-byte quantities (always stored
in big-endian order) by default. These are used, for example, to link from the
start of a subpattern to its alternatives and its end. The use of 2 bytes per
offset limits the size of the compiled regex to around 64K, which is big enough
for almost everybody. However, I received a request for an even bigger limit.
For this reason, and also to make the code easier to maintain, the storing and
loading of offsets from the byte string is now handled by the macros that are
defined here.

The macros are controlled by the value of LINK_SIZE. This defaults to 2 in
the config.h file, but can be overridden by using -D on the command line. This
is automated on Unix systems via the "configure" command. */

#define LINK_SIZE 2

#define PUT(a,n,d)   \
  (a[n] = (d) >> 8), \
  (a[(n)+1] = (d) & 255)

#define GET(a,n) \
  (((a)[n] << 8) | (a)[(n)+1])

#define MAX_PATTERN_SIZE (1 << 16)


/* Convenience macro defined in terms of the others */

#define PUTINC(a,n,d)   PUT(a,n,d), a += LINK_SIZE


/* PCRE uses some other 2-byte quantities that do not change when the size of
offsets changes. There are used for repeat counts and for other things such as
capturing parenthesis numbers in back references. */

#define PUT2(a,n,d)   \
  a[n] = (d) >> 8; \
  a[(n)+1] = (d) & 255

#define GET2(a,n) \
  (((a)[n] << 8) | (a)[(n)+1])

#define PUT2INC(a,n,d)  PUT2(a,n,d), a += 2

#define GETCHAR(c, eptr) c = *eptr;
#define GETCHARTEST(c, eptr) c = *eptr;
#define GETCHARINC(c, eptr) c = *eptr++;
#define GETCHARINCTEST(c, eptr) c = *eptr++;
#define GETCHARLEN(c, eptr, len) c = *eptr;
#define BACKCHAR(eptr)

/* These are the public options that can change during matching. */

#define PCRE_IMS (PCRE_CASELESS|PCRE_MULTILINE|PCRE_DOTALL)

/* Private options flags start at the most significant end of the four bytes,
but skip the top bit so we can use ints for convenience without getting tangled
with negative values. The public options defined in pcre.h start at the least
significant end. Make sure they don't overlap! */

#define PCRE_FIRSTSET      0x40000000   /* first_byte is set */
#define PCRE_REQCHSET      0x20000000   /* req_byte is set */
#define PCRE_STARTLINE     0x10000000   /* start after \n for multiline */
#define PCRE_ICHANGED      0x08000000   /* i option changes within regex */
#define PCRE_NOPARTIAL     0x04000000   /* can't use partial with this regex */

/* Options for the "extra" block produced by pcre_study(). */

#define PCRE_STUDY_MAPPED   0x01        /* a map of starting chars exists */

/* Masks for identifying the public options that are permitted at compile
time, run time, or study time, respectively. */

#define PUBLIC_OPTIONS \
  (PCRE_CASELESS|PCRE_EXTENDED|PCRE_ANCHORED|PCRE_MULTILINE| \
   PCRE_DOTALL|PCRE_DOLLAR_ENDONLY|PCRE_EXTRA|PCRE_UNGREEDY|PCRE_UTF8| \
   PCRE_NO_AUTO_CAPTURE|PCRE_NO_UTF8_CHECK|PCRE_AUTO_CALLOUT|PCRE_FIRSTLINE)

#define PUBLIC_EXEC_OPTIONS \
  (PCRE_ANCHORED|PCRE_NOTBOL|PCRE_NOTEOL|PCRE_NOTEMPTY|PCRE_NO_UTF8_CHECK| \
   PCRE_PARTIAL)

#define PUBLIC_DFA_EXEC_OPTIONS \
  (PCRE_ANCHORED|PCRE_NOTBOL|PCRE_NOTEOL|PCRE_NOTEMPTY|PCRE_NO_UTF8_CHECK| \
   PCRE_PARTIAL|PCRE_DFA_SHORTEST|PCRE_DFA_RESTART)

#define PUBLIC_STUDY_OPTIONS 0  /* None defined */

/* Magic number to provide a small check against being handed junk. Also used
to detect whether a pattern was compiled on a host of different endianness. */

#define MAGIC_NUMBER  0x50435245UL      /* 'PCRE' */

/* Negative values for the firstchar and reqchar variables */

#define REQ_UNSET (-2)
#define REQ_NONE  (-1)

/* The maximum remaining length of subject we are prepared to search for a
req_byte match. */

#define REQ_BYTE_MAX 1000

/* Flags added to firstbyte or reqbyte; a "non-literal" item is either a
variable-length repeat, or a anything other than literal characters. */

#define REQ_CASELESS 0x0100     /* indicates caselessness */
#define REQ_VARY     0x0200     /* reqbyte followed non-literal item */

/* Miscellaneous definitions */

typedef bool BOOL;

#define FALSE   false
#define TRUE    true

/* Escape items that are just an encoding of a particular data value. Note that
ESC_n is defined as yet another macro, which is set in config.h to either \n
(the default) or \r (which some people want). */

#ifndef ESC_e
#define ESC_e 27
#endif

#ifndef ESC_f
#define ESC_f '\f'
#endif

#ifndef ESC_n
#define ESC_n NEWLINE
#endif

#ifndef ESC_r
#define ESC_r '\r'
#endif

/* We can't officially use ESC_t because it is a POSIX reserved identifier
(presumably because of all the others like size_t). */

#ifndef ESC_tee
#define ESC_tee '\t'
#endif

/* These are escaped items that aren't just an encoding of a particular data
value such as \n. They must have non-zero values, as check_escape() returns
their negation. Also, they must appear in the same order as in the opcode
definitions below, up to ESC_z. There's a dummy for OP_ANY because it
corresponds to "." rather than an escape sequence. The final one must be
ESC_REF as subsequent values are used for \1, \2, \3, etc. There is are two
tests in the code for an escape greater than ESC_b and less than ESC_Z to
detect the types that may be repeated. These are the types that consume
characters. If any new escapes are put in between that don't consume a
character, that code will have to change. */

enum { ESC_A = 1, ESC_G, ESC_B, ESC_b, ESC_D, ESC_d, ESC_S, ESC_s, ESC_W,
  ESC_w, ESC_dum1, ESC_C, ESC_P, ESC_p, ESC_X, ESC_Z, ESC_z, ESC_E,
  ESC_Q, ESC_REF
};

/* Flag bits and data types for the extended class (OP_XCLASS) for classes that
contain UTF-8 characters with values greater than 255. */

#define XCL_NOT    0x01         /* Flag: this is a negative class */
#define XCL_MAP    0x02         /* Flag: a 32-byte map is present */

#define XCL_END       0         /* Marks end of individual items */
#define XCL_SINGLE    1         /* Single item (one multibyte char) follows */
#define XCL_RANGE     2         /* A range (two multibyte chars) follows */
#define XCL_PROP      3         /* Unicode property (one property code) follows */
#define XCL_NOTPROP   4         /* Unicode inverted property (ditto) */


/* Opcode table: OP_BRA must be last, as all values >= it are used for brackets
that extract substrings. Starting from 1 (i.e. after OP_END), the values up to
OP_EOD must correspond in order to the list of escapes immediately above.
Note that whenever this list is updated, the two macro definitions that follow
must also be updated to match. */

enum {
  OP_END,                       /* 0 End of pattern */

  /* Values corresponding to backslashed metacharacters */

  OP_SOD,                       /* 1 Start of data: \A */
  OP_SOM,                       /* 2 Start of match (subject + offset): \G */
  OP_NOT_WORD_BOUNDARY,         /*  3 \B */
  OP_WORD_BOUNDARY,             /*  4 \b */
  OP_NOT_DIGIT,                 /*  5 \D */
  OP_DIGIT,                     /*  6 \d */
  OP_NOT_WHITESPACE,            /*  7 \S */
  OP_WHITESPACE,                /*  8 \s */
  OP_NOT_WORDCHAR,              /*  9 \W */
  OP_WORDCHAR,                  /* 10 \w */
  OP_ANY,                       /* 11 Match any character */
  OP_ANYBYTE,                   /* 12 Match any byte (\C); different to OP_ANY for UTF-8 */
  OP_NOTPROP,                   /* 13 \P (not Unicode property) */
  OP_PROP,                      /* 14 \p (Unicode property) */
  OP_EXTUNI,                    /* 15 \X (extended Unicode sequence */
  OP_EODN,                      /* 16 End of data or \n at end of data: \Z. */
  OP_EOD,                       /* 17 End of data: \z */

  OP_OPT,                       /* 18 Set runtime options */
  OP_CIRC,                      /* 19 Start of line - varies with multiline switch */
  OP_DOLL,                      /* 20 End of line - varies with multiline switch */
  OP_CHAR,                      /* 21 Match one character, casefully */
  OP_CHARNC,                    /* 22 Match one character, caselessly */
  OP_NOT,                       /* 23 Match anything but the following char */

  OP_STAR,                      /* 24 The maximizing and minimizing versions of */
  OP_MINSTAR,                   /* 25 all these opcodes must come in pairs, with */
  OP_PLUS,                      /* 26 the minimizing one second. */
  OP_MINPLUS,                   /* 27 This first set applies to single characters */
  OP_QUERY,                     /* 28 */
  OP_MINQUERY,                  /* 29 */
  OP_UPTO,                      /* 30 From 0 to n matches */
  OP_MINUPTO,                   /* 31 */
  OP_EXACT,                     /* 32 Exactly n matches */

  OP_NOTSTAR,                   /* 33 The maximizing and minimizing versions of */
  OP_NOTMINSTAR,                /* 34 all these opcodes must come in pairs, with */
  OP_NOTPLUS,                   /* 35 the minimizing one second. */
  OP_NOTMINPLUS,                /* 36 This set applies to "not" single characters */
  OP_NOTQUERY,                  /* 37 */
  OP_NOTMINQUERY,               /* 38 */
  OP_NOTUPTO,                   /* 39 From 0 to n matches */
  OP_NOTMINUPTO,                /* 40 */
  OP_NOTEXACT,                  /* 41 Exactly n matches */

  OP_TYPESTAR,                  /* 42 The maximizing and minimizing versions of */
  OP_TYPEMINSTAR,               /* 43 all these opcodes must come in pairs, with */
  OP_TYPEPLUS,                  /* 44 the minimizing one second. These codes must */
  OP_TYPEMINPLUS,               /* 45 be in exactly the same order as those above. */
  OP_TYPEQUERY,                 /* 46 This set applies to character types such as \d */
  OP_TYPEMINQUERY,              /* 47 */
  OP_TYPEUPTO,                  /* 48 From 0 to n matches */
  OP_TYPEMINUPTO,               /* 49 */
  OP_TYPEEXACT,                 /* 50 Exactly n matches */

  OP_CRSTAR,                    /* 51 The maximizing and minimizing versions of */
  OP_CRMINSTAR,                 /* 52 all these opcodes must come in pairs, with */
  OP_CRPLUS,                    /* 53 the minimizing one second. These codes must */
  OP_CRMINPLUS,                 /* 54 be in exactly the same order as those above. */
  OP_CRQUERY,                   /* 55 These are for character classes and back refs */
  OP_CRMINQUERY,                /* 56 */
  OP_CRRANGE,                   /* 57 These are different to the three sets above. */
  OP_CRMINRANGE,                /* 58 */

  OP_CLASS,                     /* 59 Match a character class, chars < 256 only */
  OP_NCLASS,                    /* 60 Same, but the bitmap was created from a negative
                                   class - the difference is relevant only when a UTF-8
                                   character > 255 is encountered. */

  OP_XCLASS,                    /* 61 Extended class for handling UTF-8 chars within the
                                   class. This does both positive and negative. */

  OP_REF,                       /* 62 Match a back reference */
  OP_RECURSE,                   /* 63 Match a numbered subpattern (possibly recursive) */
  OP_CALLOUT,                   /* 64 Call out to external function if provided */

  OP_ALT,                       /* 65 Start of alternation */
  OP_KET,                       /* 66 End of group that doesn't have an unbounded repeat */
  OP_KETRMAX,                   /* 67 These two must remain together and in this */
  OP_KETRMIN,                   /* 68 order. They are for groups the repeat for ever. */

  /* The assertions must come before ONCE and COND */

  OP_ASSERT,                    /* 69 Positive lookahead */
  OP_ASSERT_NOT,                /* 70 Negative lookahead */
  OP_ASSERTBACK,                /* 71 Positive lookbehind */
  OP_ASSERTBACK_NOT,            /* 72 Negative lookbehind */
  OP_REVERSE,                   /* 73 Move pointer back - used in lookbehind assertions */

  /* ONCE and COND must come after the assertions, with ONCE first, as there's
     a test for >= ONCE for a subpattern that isn't an assertion. */

  OP_ONCE,                      /* 74 Once matched, don't back up into the subpattern */
  OP_COND,                      /* 75 Conditional group */
  OP_CREF,                      /* 76 Used to hold an extraction string number (cond ref) */

  OP_BRAZERO,                   /* 77 These two must remain together and in this */
  OP_BRAMINZERO,                /* 78 order. */

  OP_BRANUMBER,                 /* 79 Used for extracting brackets whose number is greater
                                   than can fit into an opcode. */

  OP_BRA                        /* 80 This and greater values are used for brackets that
                                   extract substrings up to EXTRACT_BASIC_MAX. After
                                   that, use is made of OP_BRANUMBER. */
};

/* WARNING WARNING WARNING: There is an implicit assumption in pcre.c and
study.c that all opcodes are less than 128 in value. This makes handling UTF-8
character sequences easier. */

/* The highest extraction number before we have to start using additional
bytes. (Originally PCRE didn't have support for extraction counts highter than
this number.) The value is limited by the number of opcodes left after OP_BRA,
i.e. 255 - OP_BRA. We actually set it a bit lower to leave room for additional
opcodes. */

#define EXTRACT_BASIC_MAX  100


/* This macro defines textual names for all the opcodes. These are used only
for debugging. The macro is referenced only in pcre_printint.c. */

#define OP_NAME_LIST \
  "End", "\\A", "\\G", "\\B", "\\b", "\\D", "\\d",                \
  "\\S", "\\s", "\\W", "\\w", "Any", "Anybyte",                   \
  "notprop", "prop", "extuni",                                    \
  "\\Z", "\\z",                                                   \
  "Opt", "^", "$", "char", "charnc", "not",                       \
  "*", "*?", "+", "+?", "?", "??", "{", "{", "{",                 \
  "*", "*?", "+", "+?", "?", "??", "{", "{", "{",                 \
  "*", "*?", "+", "+?", "?", "??", "{", "{", "{",                 \
  "*", "*?", "+", "+?", "?", "??", "{", "{",                      \
  "class", "nclass", "xclass", "Ref", "Recurse", "Callout",       \
  "Alt", "Ket", "KetRmax", "KetRmin", "Assert", "Assert not",     \
  "AssertB", "AssertB not", "Reverse", "Once", "Cond", "Cond ref",\
  "Brazero", "Braminzero", "Branumber", "Bra"


/* This macro defines the length of fixed length operations in the compiled
regex. The lengths are used when searching for specific things, and also in the
debugging printing of a compiled regex. We use a macro so that it can be
defined close to the definitions of the opcodes themselves.

As things have been extended, some of these are no longer fixed lenths, but are
minima instead. For example, the length of a single-character repeat may vary
in UTF-8 mode. The code that uses this table must know about such things. */

#define OP_LENGTHS \
  1,                             /* End                                    */ \
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* \A, \G, \B, \B, \D, \d, \S, \s, \W, \w */ \
  1, 1,                          /* Any, Anybyte                           */ \
  2, 2, 1,                       /* NOTPROP, PROP, EXTUNI                  */ \
  1, 1, 2, 1, 1,                 /* \Z, \z, Opt, ^, $                      */ \
  2,                             /* Char  - the minimum length             */ \
  2,                             /* Charnc  - the minimum length           */ \
  2,                             /* not                                    */ \
  /* Positive single-char repeats                            ** These are  */ \
  2, 2, 2, 2, 2, 2,              /* *, *?, +, +?, ?, ??      ** minima in  */ \
  4, 4, 4,                       /* upto, minupto, exact     ** UTF-8 mode */ \
  /* Negative single-char repeats - only for chars < 256                   */ \
  2, 2, 2, 2, 2, 2,              /* NOT *, *?, +, +?, ?, ??                */ \
  4, 4, 4,                       /* NOT upto, minupto, exact               */ \
  /* Positive type repeats                                                 */ \
  2, 2, 2, 2, 2, 2,              /* Type *, *?, +, +?, ?, ??               */ \
  4, 4, 4,                       /* Type upto, minupto, exact              */ \
  /* Character class & ref repeats                                         */ \
  1, 1, 1, 1, 1, 1,              /* *, *?, +, +?, ?, ??                    */ \
  5, 5,                          /* CRRANGE, CRMINRANGE                    */ \
 33,                             /* CLASS                                  */ \
 33,                             /* NCLASS                                 */ \
  0,                             /* XCLASS - variable length               */ \
  3,                             /* REF                                    */ \
  1+LINK_SIZE,                   /* RECURSE                                */ \
  2+2*LINK_SIZE,                 /* CALLOUT                                */ \
  1+LINK_SIZE,                   /* Alt                                    */ \
  1+LINK_SIZE,                   /* Ket                                    */ \
  1+LINK_SIZE,                   /* KetRmax                                */ \
  1+LINK_SIZE,                   /* KetRmin                                */ \
  1+LINK_SIZE,                   /* Assert                                 */ \
  1+LINK_SIZE,                   /* Assert not                             */ \
  1+LINK_SIZE,                   /* Assert behind                          */ \
  1+LINK_SIZE,                   /* Assert behind not                      */ \
  1+LINK_SIZE,                   /* Reverse                                */ \
  1+LINK_SIZE,                   /* Once                                   */ \
  1+LINK_SIZE,                   /* COND                                   */ \
  3,                             /* CREF                                   */ \
  1, 1,                          /* BRAZERO, BRAMINZERO                    */ \
  3,                             /* BRANUMBER                              */ \
  1+LINK_SIZE                    /* BRA                                    */ \


/* A magic value for OP_CREF to indicate the "in recursion" condition. */

#define CREF_RECURSE  0xffff

/* Error code numbers. They are given names so that they can more easily be
tracked. */

enum { ERR0, ERR1, ERR2, ERR3, ERR4, ERR5, ERR6, ERR7, ERR8, ERR9,
  ERR10, ERR11, ERR12, ERR13, ERR14, ERR15, ERR16, ERR17, ERR18, ERR19,
  ERR20, ERR21, ERR22, ERR23, ERR24, ERR25, ERR26, ERR27, ERR28, ERR29,
  ERR30, ERR31, ERR32, ERR33, ERR34, ERR35, ERR36, ERR37, ERR38, ERR39,
  ERR40, ERR41, ERR42, ERR43, ERR44, ERR45, ERR46, ERR47
};

/* The real format of the start of the pcre block; the index of names and the
code vector run on as long as necessary after the end. We store an explicit
offset to the name table so that if a regex is compiled on one host, saved, and
then run on another where the size of pointers is different, all might still
be well. For the case of compiled-on-4 and run-on-8, we include an extra
pointer that is always NULL. For future-proofing, a few dummy fields were
originally included - even though you can never get this planning right - but
there is only one left now.

NOTE NOTE NOTE:
Because people can now save and re-use compiled patterns, any additions to this
structure should be made at the end, and something earlier (e.g. a new
flag in the options or one of the dummy fields) should indicate that the new
fields are present. Currently PCRE always sets the dummy fields to zero.
NOTE NOTE NOTE:
*/

typedef struct real_pcre {
  pcre_uint32 magic_number;
  pcre_uint32 size;             /* Total that was malloced */
  pcre_uint32 options;
  pcre_uint32 dummy1;           /* For future use, maybe */

  pcre_uint16 top_bracket;
  pcre_uint16 top_backref;
  pcre_uint16 first_byte;
  pcre_uint16 req_byte;
  pcre_uint16 name_table_offset;        /* Offset to name table that follows */
  pcre_uint16 name_entry_size;  /* Size of any name items */
  pcre_uint16 name_count;       /* Number of name items */
  pcre_uint16 ref_count;        /* Reference count */

  const unsigned char *tables;  /* Pointer to tables or NULL for std */
  const unsigned char *nullpad; /* NULL padding */
} real_pcre;

/* The format of the block used to store data from pcre_study(). The same
remark (see NOTE above) about extending this structure applies. */

typedef struct pcre_study_data {
  pcre_uint32 size;             /* Total that was malloced */
  pcre_uint32 options;
  uschar start_bits[32];
} pcre_study_data;

/* Structure for passing "static" information around between the functions
doing the compiling, so that they are thread-safe. */

typedef struct compile_data {
  const uschar *lcc;            /* Points to lower casing table */
  const uschar *fcc;            /* Points to case-flipping table */
  const uschar *cbits;          /* Points to character type table */
  const uschar *ctypes;         /* Points to table of type maps */
  const uschar *start_code;     /* The start of the compiled code */
  const uschar *start_pattern;  /* The start of the pattern */
  uschar *name_table;           /* The name/number table */
  int names_found;              /* Number of entries so far */
  int name_entry_size;          /* Size of each entry */
  int top_backref;              /* Maximum back reference */
  unsigned int backref_map;     /* Bitmap of low back refs */
  int req_varyopt;              /* "After variable item" flag for reqbyte */
  BOOL nopartial;               /* Set TRUE if partial won't work */
} compile_data;

/* Structure for maintaining a chain of pointers to the currently incomplete
branches, for testing for left recursion. */

typedef struct branch_chain {
  struct branch_chain *outer;
  uschar *current;
} branch_chain;

/* Structure for items in a linked list that represents an explicit recursive
call within the pattern. */

typedef struct recursion_info {
  struct recursion_info *prevrec;       /* Previous recursion record (or NULL) */
  int group_num;                /* Number of group that was called */
  const uschar *after_call;     /* "Return value": points after the call in the expr */
  const uschar *save_start;     /* Old value of md->start_match */
  int *offset_save;             /* Pointer to start of saved offsets */
  int saved_max;                /* Number of saved offsets */
} recursion_info;

/* When compiling in a mode that doesn't use recursive calls to match(),
a structure is used to remember local variables on the heap. It is defined in
pcre.c, close to the match() function, so that it is easy to keep it in step
with any changes of local variable. However, the pointer to the current frame
must be saved in some "static" place over a longjmp(). We declare the
structure here so that we can put a pointer in the match_data structure.
NOTE: This isn't used for a "normal" compilation of pcre. */

struct heapframe;

/* Structure for passing "static" information around between the functions
doing traditional NFA matching, so that they are thread-safe. */

typedef struct match_data {
  unsigned long int match_call_count;   /* As it says */
  unsigned long int match_limit;        /* As it says */
  int *offset_vector;           /* Offset vector */
  int offset_end;               /* One past the end */
  int offset_max;               /* The maximum usable for return data */
  const uschar *lcc;            /* Points to lower casing table */
  const uschar *ctypes;         /* Points to table of type maps */
  BOOL offset_overflow;         /* Set if too many extractions */
  BOOL notbol;                  /* NOTBOL flag */
  BOOL noteol;                  /* NOTEOL flag */
  BOOL utf8;                    /* UTF8 flag */
  BOOL endonly;                 /* Dollar not before final \n */
  BOOL notempty;                /* Empty string match not wanted */
  BOOL partial;                 /* PARTIAL flag */
  BOOL hitend;                  /* Hit the end of the subject at some point */
  const uschar *start_code;     /* For use when recursing */
  const uschar *start_subject;  /* Start of the subject string */
  const uschar *end_subject;    /* End of the subject string */
  const uschar *start_match;    /* Start of this match attempt */
  const uschar *end_match_ptr;  /* Subject position at end match */
  int end_offset_top;           /* Highwater mark at end of match */
  int capture_last;             /* Most recent capture number */
  int start_offset;             /* The start offset value */
  recursion_info *recursive;    /* Linked list of recursion data */
  void *callout_data;           /* To pass back to callouts */
  struct heapframe *thisframe;  /* Used only when compiling for no recursion */
} match_data;

/* A similar structure is used for the same purpose by the DFA matching
functions. */

typedef struct dfa_match_data {
  const uschar *start_code;     /* Start of the compiled pattern */
  const uschar *start_subject;  /* Start of the subject string */
  const uschar *end_subject;    /* End of subject string */
  const uschar *tables;         /* Character tables */
  int moptions;                 /* Match options */
  int poptions;                 /* Pattern options */
  void *callout_data;           /* To pass back to callouts */
} dfa_match_data;

/* Bit definitions for entries in the pcre_ctypes table. */

#define ctype_space   0x01
#define ctype_letter  0x02
#define ctype_digit   0x04
#define ctype_xdigit  0x08
#define ctype_word    0x10      /* alphameric or '_' */
#define ctype_meta    0x80      /* regexp meta char or zero (end pattern) */

/* Offsets for the bitmap tables in pcre_cbits. Each table contains a set
of bits for a class map. Some classes are built by combining these tables. */

#define cbit_space     0        /* [:space:] or \s */
#define cbit_xdigit   32        /* [:xdigit:] */
#define cbit_digit    64        /* [:digit:] or \d */
#define cbit_upper    96        /* [:upper:] */
#define cbit_lower   128        /* [:lower:] */
#define cbit_word    160        /* [:word:] or \w */
#define cbit_graph   192        /* [:graph:] */
#define cbit_print   224        /* [:print:] */
#define cbit_punct   256        /* [:punct:] */
#define cbit_cntrl   288        /* [:cntrl:] */
#define cbit_length  320        /* Length of the cbits table */

/* Offsets of the various tables from the base tables pointer, and
total length. */

#define lcc_offset      0
#define fcc_offset    256
#define cbits_offset  512
#define ctypes_offset (cbits_offset + cbit_length)
#define tables_length (ctypes_offset + 256)

/* Layout of the UCP type table that translates property names into codes for
pcre_ucp_findchar(). */

typedef struct {
  const char *name;
  int value;
} ucp_type_table;


/* Bits of pcre_globals.c */
int (*pcre_callout) (pcre_callout_block *) = NULL;

/* pcre_chartables.c */
/*************************************************
*      Perl-Compatible Regular Expressions       *
*************************************************/

/* This file is automatically written by the dftables auxiliary 
program. If you edit it by hand, you might like to edit the Makefile to 
prevent its ever being regenerated.

This file contains the default tables for characters with codes less than
128 (ASCII characters). These tables are used when no external tables are
passed to PCRE. */

const unsigned char _pcre_default_tables[] = {

/* This table is a lower casing table. */

  0, 1, 2, 3, 4, 5, 6, 7,
  8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55,
  56, 57, 58, 59, 60, 61, 62, 63,
  64, 97, 98, 99, 100, 101, 102, 103,
  104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119,
  120, 121, 122, 91, 92, 93, 94, 95,
  96, 97, 98, 99, 100, 101, 102, 103,
  104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119,
  120, 121, 122, 123, 124, 125, 126, 127,
  128, 129, 130, 131, 132, 133, 134, 135,
  136, 137, 138, 139, 140, 141, 142, 143,
  144, 145, 146, 147, 148, 149, 150, 151,
  152, 153, 154, 155, 156, 157, 158, 159,
  160, 161, 162, 163, 164, 165, 166, 167,
  168, 169, 170, 171, 172, 173, 174, 175,
  176, 177, 178, 179, 180, 181, 182, 183,
  184, 185, 186, 187, 188, 189, 190, 191,
  192, 193, 194, 195, 196, 197, 198, 199,
  200, 201, 202, 203, 204, 205, 206, 207,
  208, 209, 210, 211, 212, 213, 214, 215,
  216, 217, 218, 219, 220, 221, 222, 223,
  224, 225, 226, 227, 228, 229, 230, 231,
  232, 233, 234, 235, 236, 237, 238, 239,
  240, 241, 242, 243, 244, 245, 246, 247,
  248, 249, 250, 251, 252, 253, 254, 255,

/* This table is a case flipping table. */

  0, 1, 2, 3, 4, 5, 6, 7,
  8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55,
  56, 57, 58, 59, 60, 61, 62, 63,
  64, 97, 98, 99, 100, 101, 102, 103,
  104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119,
  120, 121, 122, 91, 92, 93, 94, 95,
  96, 65, 66, 67, 68, 69, 70, 71,
  72, 73, 74, 75, 76, 77, 78, 79,
  80, 81, 82, 83, 84, 85, 86, 87,
  88, 89, 90, 123, 124, 125, 126, 127,
  128, 129, 130, 131, 132, 133, 134, 135,
  136, 137, 138, 139, 140, 141, 142, 143,
  144, 145, 146, 147, 148, 149, 150, 151,
  152, 153, 154, 155, 156, 157, 158, 159,
  160, 161, 162, 163, 164, 165, 166, 167,
  168, 169, 170, 171, 172, 173, 174, 175,
  176, 177, 178, 179, 180, 181, 182, 183,
  184, 185, 186, 187, 188, 189, 190, 191,
  192, 193, 194, 195, 196, 197, 198, 199,
  200, 201, 202, 203, 204, 205, 206, 207,
  208, 209, 210, 211, 212, 213, 214, 215,
  216, 217, 218, 219, 220, 221, 222, 223,
  224, 225, 226, 227, 228, 229, 230, 231,
  232, 233, 234, 235, 236, 237, 238, 239,
  240, 241, 242, 243, 244, 245, 246, 247,
  248, 249, 250, 251, 252, 253, 254, 255,

/* This table contains bit maps for various character classes.
Each map is 32 bytes long and the bits run from the least
significant end of each byte. The classes that have their own
maps are: space, xdigit, digit, upper, lower, word, graph
print, punct, and cntrl. Other classes are built from combinations. */

  0x00, 0x3e, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x03,
  0x7e, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x03,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xfe, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0x07,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x03,
  0xfe, 0xff, 0xff, 0x87, 0xfe, 0xff, 0xff, 0x07,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x00, 0xfc,
  0x01, 0x00, 0x00, 0xf8, 0x01, 0x00, 0x00, 0x78,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

/* This table identifies various classes of character by individual bits:
  0x01   white space character
  0x02   letter
  0x04   decimal digit
  0x08   hexadecimal digit
  0x10   alphanumeric or '_'
  0x80   regular expression metacharacter or binary zero
*/

  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*   0-  7 */
  0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00,       /*   8- 15 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  16- 23 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  24- 31 */
  0x01, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,       /*    - '  */
  0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x80, 0x00,       /*  ( - /  */
  0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,       /*  0 - 7  */
  0x1c, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,       /*  8 - ?  */
  0x00, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x12,       /*  @ - G  */
  0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,       /*  H - O  */
  0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,       /*  P - W  */
  0x12, 0x12, 0x12, 0x80, 0x00, 0x00, 0x80, 0x10,       /*  X - _  */
  0x00, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x12,       /*  ` - g  */
  0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,       /*  h - o  */
  0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,       /*  p - w  */
  0x12, 0x12, 0x12, 0x80, 0x80, 0x00, 0x00, 0x00,       /*  x -127 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 128-135 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 136-143 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 144-151 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 152-159 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 160-167 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 168-175 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 176-183 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 184-191 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 192-199 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 200-207 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 208-215 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 216-223 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 224-231 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 232-239 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 240-247 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};                              /* 248-255 */

/* End of chartables.c */

/* Bits of pcre_tables.c */

/* This module contains some fixed tables that are used by more than one of the
PCRE code modules. The tables are also #included by the pcretest program, which
uses macros to change their names from _pcre_xxx to xxxx, thereby avoiding name
clashes with the library. */


/* Table of sizes for the fixed-length opcodes. It's defined in a macro so that
the definition is next to the definition of the opcodes in internal.h. */

const uschar _pcre_OP_lengths[] = { OP_LENGTHS };




/* End of pcre_tables.c */


/* pcre_try_flipped.c */

/* This module contains an internal function that tests a compiled pattern to
see if it was compiled with the opposite endianness. If so, it uses an
auxiliary local function to flip the appropriate bytes. */


/*************************************************
*         Flip bytes in an integer               *
*************************************************/

/* This function is called when the magic number in a regex doesn't match, in
order to flip its bytes to see if we are dealing with a pattern that was
compiled on a host of different endianness. If so, this function is used to
flip other byte values.

Arguments:
  value        the number to flip
  n            the number of bytes to flip (assumed to be 2 or 4)

Returns:       the flipped value
*/

static long int
byteflip(long int value, int n)
{
  if (n == 2)
    return ((value & 0x00ff) << 8) | ((value & 0xff00) >> 8);
  return ((value & 0x000000ff) << 24) |
    ((value & 0x0000ff00) << 8) |
    ((value & 0x00ff0000) >> 8) | ((value & 0xff000000) >> 24);
}



/*************************************************
*       Test for a byte-flipped compiled regex   *
*************************************************/

/* This function is called from pcre_exec(), pcre_dfa_exec(), and also from
pcre_fullinfo(). Its job is to test whether the regex is byte-flipped - that
is, it was compiled on a system of opposite endianness. The function is called
only when the native MAGIC_NUMBER test fails. If the regex is indeed flipped,
we flip all the relevant values into a different data block, and return it.

Arguments:
  re               points to the regex
  study            points to study data, or NULL
  internal_re      points to a new regex block
  internal_study   points to a new study block

Returns:           the new block if is is indeed a byte-flipped regex
                   NULL if it is not
*/

real_pcre *_pcre_try_flipped(const real_pcre *re, real_pcre *internal_re,
                             const pcre_study_data *study,
                             pcre_study_data *internal_study);

real_pcre *
_pcre_try_flipped(const real_pcre *re, real_pcre *internal_re,
                  const pcre_study_data *study, pcre_study_data *internal_study)
{
  if (byteflip(re->magic_number, sizeof(re->magic_number)) != MAGIC_NUMBER)
    return NULL;

  *internal_re = *re;           /* To copy other fields */
  internal_re->size = byteflip(re->size, sizeof(re->size));
  internal_re->options = byteflip(re->options, sizeof(re->options));
  internal_re->top_bracket =
    (pcre_uint16) byteflip(re->top_bracket, sizeof(re->top_bracket));
  internal_re->top_backref =
    (pcre_uint16) byteflip(re->top_backref, sizeof(re->top_backref));
  internal_re->first_byte =
    (pcre_uint16) byteflip(re->first_byte, sizeof(re->first_byte));
  internal_re->req_byte =
    (pcre_uint16) byteflip(re->req_byte, sizeof(re->req_byte));
  internal_re->name_table_offset =
    (pcre_uint16) byteflip(re->name_table_offset,
                           sizeof(re->name_table_offset));
  internal_re->name_entry_size =
    (pcre_uint16) byteflip(re->name_entry_size, sizeof(re->name_entry_size));
  internal_re->name_count =
    (pcre_uint16) byteflip(re->name_count, sizeof(re->name_count));

  if (study != NULL) {
    *internal_study = *study;   /* To copy other fields */
    internal_study->size = byteflip(study->size, sizeof(study->size));
    internal_study->options = byteflip(study->options, sizeof(study->options));
  }

  return internal_re;
}

/* End of pcre_tryflipped.c */


/* pcre_fullinfo.c */

/* This module contains the external function pcre_fullinfo(), which returns
information about a compiled pattern. */



/*************************************************
*        Return info about compiled pattern      *
*************************************************/

/* This is a newer "info" function which has an extensible interface so
that additional items can be added compatibly.

Arguments:
  argument_re      points to compiled code
  extra_data       points extra data, or NULL
  what             what information is required
  where            where to put the information

Returns:           0 if data returned, negative on error
*/

int
pcre_fullinfo(const pcre * argument_re, const pcre_extra * extra_data, int what,
              void *where)
{
  real_pcre internal_re;
  pcre_study_data internal_study;
  const real_pcre *re = (const real_pcre *) argument_re;
  const pcre_study_data *study = NULL;

  if (re == NULL || where == NULL)
    return PCRE_ERROR_NULL;

  if (extra_data != NULL && (extra_data->flags & PCRE_EXTRA_STUDY_DATA) != 0)
    study = (const pcre_study_data *) extra_data->study_data;

  if (re->magic_number != MAGIC_NUMBER) {
    re = _pcre_try_flipped(re, &internal_re, study, &internal_study);
    if (re == NULL)
      return PCRE_ERROR_BADMAGIC;
    if (study != NULL)
      study = &internal_study;
  }

  switch (what) {
  case PCRE_INFO_OPTIONS:
    *((unsigned long int *) where) = re->options & PUBLIC_OPTIONS;
    break;

  case PCRE_INFO_SIZE:
    *((size_t *) where) = re->size;
    break;

  case PCRE_INFO_STUDYSIZE:
    *((size_t *) where) = (study == NULL) ? 0 : study->size;
    break;

  case PCRE_INFO_CAPTURECOUNT:
    *((int *) where) = re->top_bracket;
    break;

  case PCRE_INFO_BACKREFMAX:
    *((int *) where) = re->top_backref;
    break;

  case PCRE_INFO_FIRSTBYTE:
    *((int *) where) =
      ((re->options & PCRE_FIRSTSET) != 0) ? re->first_byte :
      ((re->options & PCRE_STARTLINE) != 0) ? -1 : -2;
    break;

    /* Make sure we pass back the pointer to the bit vector in the external
       block, not the internal copy (with flipped integer fields). */

  case PCRE_INFO_FIRSTTABLE:
    *((const uschar **) where) =
      (study != NULL && (study->options & PCRE_STUDY_MAPPED) != 0) ?
      ((const pcre_study_data *) extra_data->study_data)->start_bits : NULL;
    break;

  case PCRE_INFO_LASTLITERAL:
    *((int *) where) = ((re->options & PCRE_REQCHSET) != 0) ? re->req_byte : -1;
    break;

  case PCRE_INFO_NAMEENTRYSIZE:
    *((int *) where) = re->name_entry_size;
    break;

  case PCRE_INFO_NAMECOUNT:
    *((int *) where) = re->name_count;
    break;

  case PCRE_INFO_NAMETABLE:
    *((const uschar **) where) = (const uschar *) re + re->name_table_offset;
    break;

  case PCRE_INFO_DEFAULT_TABLES:
    *((const uschar **) where) = (const uschar *) (_pcre_default_tables);
    break;

  default:
    return PCRE_ERROR_BADOPTION;
  }

  return 0;
}

/* End of pcre_fullinfo.c */


/* pcre_get.c */

/* This module contains some convenience functions for extracting substrings
from the subject string after a regex match has succeeded. The original idea
for these functions came from Scott Wimer. */


/*************************************************
*           Find number for named string         *
*************************************************/

/* This function is used by the two extraction functions below, as well
as being generally available.

Arguments:
  code        the compiled regex
  stringname  the name whose number is required

Returns:      the number of the named parentheses, or a negative number
                (PCRE_ERROR_NOSUBSTRING) if not found
*/

int
pcre_get_stringnumber(const pcre * code, const char *stringname)
{
  int rc;
  int entrysize;
  int top, bot;
  uschar *nametable;

  if ((rc = pcre_fullinfo(code, NULL, PCRE_INFO_NAMECOUNT, &top)) != 0)
    return rc;
  if (top <= 0)
    return PCRE_ERROR_NOSUBSTRING;

  if ((rc =
       pcre_fullinfo(code, NULL, PCRE_INFO_NAMEENTRYSIZE, &entrysize)) != 0)
    return rc;
  if ((rc = pcre_fullinfo(code, NULL, PCRE_INFO_NAMETABLE, &nametable)) != 0)
    return rc;

  bot = 0;
  while (top > bot) {
    int mid = (top + bot) / 2;
    uschar *entry = nametable + entrysize * mid;
    int c = strcmp(stringname, (char *) (entry + 2));
    if (c == 0)
      return (entry[0] << 8) + entry[1];
    if (c > 0)
      bot = mid + 1;
    else
      top = mid;
  }

  return PCRE_ERROR_NOSUBSTRING;
}



/*************************************************
*      Copy captured string to given buffer      *
*************************************************/

/* This function copies a single captured substring into a given buffer.
Note that we use memcpy() rather than strncpy() in case there are binary zeros
in the string.

Arguments:
  subject        the subject string that was matched
  ovector        pointer to the offsets table
  stringcount    the number of substrings that were captured
                   (i.e. the yield of the pcre_exec call, unless
                   that was zero, in which case it should be 1/3
                   of the offset table size)
  stringnumber   the number of the required substring
  buffer         where to put the substring
  size           the size of the buffer

Returns:         if successful:
                   the length of the copied string, not including the zero
                   that is put on the end; can be zero
                 if not successful:
                   PCRE_ERROR_NOMEMORY (-6) buffer too small
                   PCRE_ERROR_NOSUBSTRING (-7) no such captured substring
*/

int
pcre_copy_substring(const char *subject, int *ovector, int stringcount,
                    int stringnumber, char *buffer, int size)
{
  int yield;
  if (stringnumber < 0 || stringnumber >= stringcount)
    return PCRE_ERROR_NOSUBSTRING;
  stringnumber *= 2;
  yield = ovector[stringnumber + 1] - ovector[stringnumber];
  if (size < yield + 1)
    return PCRE_ERROR_NOMEMORY;
  memcpy(buffer, subject + ovector[stringnumber], yield);
  buffer[yield] = 0;
  return yield;
}



/*************************************************
*   Copy named captured string to given buffer   *
*************************************************/

/* This function copies a single captured substring into a given buffer,
identifying it by name.

Arguments:
  code           the compiled regex
  subject        the subject string that was matched
  ovector        pointer to the offsets table
  stringcount    the number of substrings that were captured
                   (i.e. the yield of the pcre_exec call, unless
                   that was zero, in which case it should be 1/3
                   of the offset table size)
  stringname     the name of the required substring
  buffer         where to put the substring
  size           the size of the buffer

Returns:         if successful:
                   the length of the copied string, not including the zero
                   that is put on the end; can be zero
                 if not successful:
                   PCRE_ERROR_NOMEMORY (-6) buffer too small
                   PCRE_ERROR_NOSUBSTRING (-7) no such captured substring
*/

int
pcre_copy_named_substring(const pcre * code, const char *subject, int *ovector,
                          int stringcount, const char *stringname, char *buffer,
                          int size)
{
  int n = pcre_get_stringnumber(code, stringname);
  if (n <= 0)
    return n;
  return pcre_copy_substring(subject, ovector, stringcount, n, buffer, size);
}



/*************************************************
*      Copy all captured strings to new store    *
*************************************************/

/* This function gets one chunk of store and builds a list of pointers and all
of the captured substrings in it. A NULL pointer is put on the end of the list.

Arguments:
  subject        the subject string that was matched
  ovector        pointer to the offsets table
  stringcount    the number of substrings that were captured
                   (i.e. the yield of the pcre_exec call, unless
                   that was zero, in which case it should be 1/3
                   of the offset table size)
  listptr        set to point to the list of pointers

Returns:         if successful: 0
                 if not successful:
                   PCRE_ERROR_NOMEMORY (-6) failed to get store
*/

int















pcre_get_substring_list(const char *subject, int *ovector, int stringcount,
                        const char ***listptr);

int
pcre_get_substring_list(const char *subject, int *ovector, int stringcount,
                        const char ***listptr)
{
  int i;
  int size = sizeof(char *);
  int double_count = stringcount * 2;
  char **stringlist;
  char *p;

  for (i = 0; i < double_count; i += 2)
    size += sizeof(char *) + ovector[i + 1] - ovector[i] + 1;

  stringlist = (char **) malloc(size);
  if (stringlist == NULL)
    return PCRE_ERROR_NOMEMORY;

  *listptr = (const char **) stringlist;
  p = (char *) (stringlist + stringcount + 1);

  for (i = 0; i < double_count; i += 2) {
    int len = ovector[i + 1] - ovector[i];
    memcpy(p, subject + ovector[i], len);
    *stringlist++ = p;
    p += len;
    *p++ = 0;
  }

  *stringlist = NULL;
  return 0;
}



/*************************************************
*   Free store obtained by get_substring_list    *
*************************************************/

/* This function exists for the benefit of people calling PCRE from non-C
programs that can call its functions, but not free() or free() directly.

Argument:   the result of a previous pcre_get_substring_list()
Returns:    nothing
*/
void
 pcre_free_substring_list(const char **pointer);

void
pcre_free_substring_list(const char **pointer)
{
  free((void *) pointer);
}



/*************************************************
*      Copy captured string to new store         *
*************************************************/

/* This function copies a single captured substring into a piece of new
store

Arguments:
  subject        the subject string that was matched
  ovector        pointer to the offsets table
  stringcount    the number of substrings that were captured
                   (i.e. the yield of the pcre_exec call, unless
                   that was zero, in which case it should be 1/3
                   of the offset table size)
  stringnumber   the number of the required substring
  stringptr      where to put a pointer to the substring

Returns:         if successful:
                   the length of the string, not including the zero that
                   is put on the end; can be zero
                 if not successful:
                   PCRE_ERROR_NOMEMORY (-6) failed to get store
                   PCRE_ERROR_NOSUBSTRING (-7) substring not present
*/

int















pcre_get_substring(const char *subject, int *ovector, int stringcount,
                   int stringnumber, const char **stringptr);

int
pcre_get_substring(const char *subject, int *ovector, int stringcount,
                   int stringnumber, const char **stringptr)
{
  int yield;
  char *substring;
  if (stringnumber < 0 || stringnumber >= stringcount)
    return PCRE_ERROR_NOSUBSTRING;
  stringnumber *= 2;
  yield = ovector[stringnumber + 1] - ovector[stringnumber];
  substring = (char *) malloc(yield + 1);
  if (substring == NULL)
    return PCRE_ERROR_NOMEMORY;
  memcpy(substring, subject + ovector[stringnumber], yield);
  substring[yield] = 0;
  *stringptr = substring;
  return yield;
}



/*************************************************
*   Copy named captured string to new store      *
*************************************************/

/* This function copies a single captured substring, identified by name, into
new store.

Arguments:
  code           the compiled regex
  subject        the subject string that was matched
  ovector        pointer to the offsets table
  stringcount    the number of substrings that were captured
                   (i.e. the yield of the pcre_exec call, unless
                   that was zero, in which case it should be 1/3
                   of the offset table size)
  stringname     the name of the required substring
  stringptr      where to put the pointer

Returns:         if successful:
                   the length of the copied string, not including the zero
                   that is put on the end; can be zero
                 if not successful:
                   PCRE_ERROR_NOMEMORY (-6) couldn't get memory
                   PCRE_ERROR_NOSUBSTRING (-7) no such captured substring
*/

int















pcre_get_named_substring(const pcre * code, const char *subject, int *ovector,
                         int stringcount, const char *stringname,
                         const char **stringptr);

int
pcre_get_named_substring(const pcre * code, const char *subject, int *ovector,
                         int stringcount, const char *stringname,
                         const char **stringptr)
{
  int n = pcre_get_stringnumber(code, stringname);
  if (n <= 0)
    return n;
  return pcre_get_substring(subject, ovector, stringcount, n, stringptr);
}




/*************************************************
*       Free store obtained by get_substring     *
*************************************************/

/* This function exists for the benefit of people calling PCRE from non-C
programs that can call its functions, but not free() or free() directly.

Argument:   the result of a previous pcre_get_substring()
Returns:    nothing
*/

void
 pcre_free_substring(const char *pointer);

void
pcre_free_substring(const char *pointer)
{
  free((void *) pointer);
}

/* End of pcre_get.c */

/* pcre_maketables.c */

/* This module contains the external function pcre_maketables(), which builds
character tables for PCRE in the current locale. The file is compiled on its
own as part of the PCRE library. However, it is also included in the
compilation of dftables.c, in which case the macro DFTABLES is defined. */


/*************************************************
*           Create PCRE character tables         *
*************************************************/

/* This function builds a set of character tables for use by PCRE and returns
a pointer to them. They are build using the ctype functions, and consequently
their contents will depend upon the current locale setting. When compiled as
part of the library, the store is obtained via pcre_malloc(), but when compiled
inside dftables, use malloc().

Arguments:   none
Returns:     pointer to the contiguous block of data
*/

const unsigned char *
pcre_maketables(void)
{
  unsigned char *yield, *p;
  int i;

  yield = (unsigned char *) malloc(tables_length);

  if (yield == NULL)
    return NULL;
  p = yield;

/* First comes the lower casing table */

  for (i = 0; i < 256; i++)
    *p++ = tolower(i);

/* Next the case-flipping table */

  for (i = 0; i < 256; i++)
    *p++ = islower(i) ? toupper(i) : tolower(i);

/* Then the character class tables. Don't try to be clever and save effort
on exclusive ones - in some locales things may be different. Note that the
table for "space" includes everything "isspace" gives, including VT in the
default locale. This makes it work for the POSIX class [:space:]. */

  memset(p, 0, cbit_length);
  for (i = 0; i < 256; i++) {
    if (isdigit(i)) {
      p[cbit_digit + i / 8] |= 1 << (i & 7);
      p[cbit_word + i / 8] |= 1 << (i & 7);
    }
    if (isupper(i)) {
      p[cbit_upper + i / 8] |= 1 << (i & 7);
      p[cbit_word + i / 8] |= 1 << (i & 7);
    }
    if (islower(i)) {
      p[cbit_lower + i / 8] |= 1 << (i & 7);
      p[cbit_word + i / 8] |= 1 << (i & 7);
    }
    if (i == '_')
      p[cbit_word + i / 8] |= 1 << (i & 7);
    if (isspace(i))
      p[cbit_space + i / 8] |= 1 << (i & 7);
    if (isxdigit(i))
      p[cbit_xdigit + i / 8] |= 1 << (i & 7);
    if (isgraph(i))
      p[cbit_graph + i / 8] |= 1 << (i & 7);
    if (isprint(i))
      p[cbit_print + i / 8] |= 1 << (i & 7);
    if (ispunct(i))
      p[cbit_punct + i / 8] |= 1 << (i & 7);
    if (iscntrl(i))
      p[cbit_cntrl + i / 8] |= 1 << (i & 7);
  }
  p += cbit_length;

/* Finally, the character type table. In this, we exclude VT from the white
space chars, because Perl doesn't recognize it as such for \s and for comments
within regexes. */

  for (i = 0; i < 256; i++) {
    int x = 0;
    if (i != 0x0b && isspace(i))
      x += ctype_space;
    if (isalpha(i))
      x += ctype_letter;
    if (isdigit(i))
      x += ctype_digit;
    if (isxdigit(i))
      x += ctype_xdigit;
    if (isalnum(i) || i == '_')
      x += ctype_word;

    /* Note: strchr includes the terminating zero in the characters it considers.
       In this instance, that is ok because we want binary zero to be flagged as a
       meta-character, which in this sense is any character that terminates a run
       of data characters. */

    if (strchr("*+?{^.$|()[", i) != 0)
      x += ctype_meta;
    *p++ = x;
  }

  return yield;
}

/* End of pcre_maketables.c */

/* pcre_study.c */


/* This module contains the external function pcre_study(), along with local
supporting functions. */



/*************************************************
*      Set a bit and maybe its alternate case    *
*************************************************/

/* Given a character, set its bit in the table, and also the bit for the other
version of a letter if we are caseless.

Arguments:
  start_bits    points to the bit map
  c             is the character
  caseless      the caseless flag
  cd            the block with char table pointers

Returns:        nothing
*/

static void
set_bit(uschar *start_bits, unsigned int c, BOOL caseless, compile_data *cd)
{
  start_bits[c / 8] |= (1 << (c & 7));
  if (caseless && (cd->ctypes[c] & ctype_letter) != 0)
    start_bits[cd->fcc[c] / 8] |= (1 << (cd->fcc[c] & 7));
}



/*************************************************
*          Create bitmap of starting chars       *
*************************************************/

/* This function scans a compiled unanchored expression and attempts to build a
bitmap of the set of initial characters. If it can't, it returns FALSE. As time
goes by, we may be able to get more clever at doing this.

Arguments:
  code         points to an expression
  start_bits   points to a 32-byte table, initialized to 0
  caseless     the current state of the caseless flag
  utf8         TRUE if in UTF-8 mode
  cd           the block with char table pointers

Returns:       TRUE if table built, FALSE otherwise
*/

static BOOL
set_start_bits(const uschar *code, uschar *start_bits, BOOL caseless,
               BOOL utf8, compile_data *cd)
{
  register int c;

/* This next statement and the later reference to dummy are here in order to
trick the optimizer of the IBM C compiler for OS/2 into generating correct
code. Apparently IBM isn't going to fix the problem, and we would rather not
disable optimization (in this module it actually makes a big difference, and
the pcre module can use all the optimization it can get). */

  volatile int dummy;

  do {
    const uschar *tcode = code + 1 + LINK_SIZE;
    BOOL try_next = TRUE;

    while (try_next) {
      /* If a branch starts with a bracket or a positive lookahead assertion,
         recurse to set bits from within them. That's all for this branch. */

      if ((int) *tcode >= OP_BRA || *tcode == OP_ASSERT) {
        if (!set_start_bits(tcode, start_bits, caseless, utf8, cd))
          return FALSE;
        try_next = FALSE;
      }

      else
        switch (*tcode) {
        default:
          return FALSE;

          /* Skip over callout */

        case OP_CALLOUT:
          tcode += 2 + 2 * LINK_SIZE;
          break;

          /* Skip over extended extraction bracket number */

        case OP_BRANUMBER:
          tcode += 3;
          break;

          /* Skip over lookbehind and negative lookahead assertions */

        case OP_ASSERT_NOT:
        case OP_ASSERTBACK:
        case OP_ASSERTBACK_NOT:
          do
            tcode += GET(tcode, 1);
          while (*tcode == OP_ALT);
          tcode += 1 + LINK_SIZE;
          break;

          /* Skip over an option setting, changing the caseless flag */

        case OP_OPT:
          caseless = (tcode[1] & PCRE_CASELESS) != 0;
          tcode += 2;
          break;

          /* BRAZERO does the bracket, but carries on. */

        case OP_BRAZERO:
        case OP_BRAMINZERO:
          if (!set_start_bits(++tcode, start_bits, caseless, utf8, cd))
            return FALSE;
          dummy = 1;
          do
            tcode += GET(tcode, 1);
          while (*tcode == OP_ALT);
          tcode += 1 + LINK_SIZE;
          break;

          /* Single-char * or ? sets the bit and tries the next item */

        case OP_STAR:
        case OP_MINSTAR:
        case OP_QUERY:
        case OP_MINQUERY:
          set_bit(start_bits, tcode[1], caseless, cd);
          tcode += 2;
          break;

          /* Single-char upto sets the bit and tries the next */

        case OP_UPTO:
        case OP_MINUPTO:
          set_bit(start_bits, tcode[3], caseless, cd);
          tcode += 4;
          break;

          /* At least one single char sets the bit and stops */

        case OP_EXACT:         /* Fall through */
          tcode += 2;

        case OP_CHAR:
        case OP_CHARNC:
        case OP_PLUS:
        case OP_MINPLUS:
          set_bit(start_bits, tcode[1], caseless, cd);
          try_next = FALSE;
          break;

          /* Single character type sets the bits and stops */

        case OP_NOT_DIGIT:
          for (c = 0; c < 32; c++)
            start_bits[c] |= ~cd->cbits[c + cbit_digit];
          try_next = FALSE;
          break;

        case OP_DIGIT:
          for (c = 0; c < 32; c++)
            start_bits[c] |= cd->cbits[c + cbit_digit];
          try_next = FALSE;
          break;

        case OP_NOT_WHITESPACE:
          for (c = 0; c < 32; c++)
            start_bits[c] |= ~cd->cbits[c + cbit_space];
          try_next = FALSE;
          break;

        case OP_WHITESPACE:
          for (c = 0; c < 32; c++)
            start_bits[c] |= cd->cbits[c + cbit_space];
          try_next = FALSE;
          break;

        case OP_NOT_WORDCHAR:
          for (c = 0; c < 32; c++)
            start_bits[c] |= ~cd->cbits[c + cbit_word];
          try_next = FALSE;
          break;

        case OP_WORDCHAR:
          for (c = 0; c < 32; c++)
            start_bits[c] |= cd->cbits[c + cbit_word];
          try_next = FALSE;
          break;

          /* One or more character type fudges the pointer and restarts, knowing
             it will hit a single character type and stop there. */

        case OP_TYPEPLUS:
        case OP_TYPEMINPLUS:
          tcode++;
          break;

        case OP_TYPEEXACT:
          tcode += 3;
          break;

          /* Zero or more repeats of character types set the bits and then
             try again. */

        case OP_TYPEUPTO:
        case OP_TYPEMINUPTO:
          tcode += 2;           /* Fall through */

        case OP_TYPESTAR:
        case OP_TYPEMINSTAR:
        case OP_TYPEQUERY:
        case OP_TYPEMINQUERY:
          switch (tcode[1]) {
          case OP_ANY:
            return FALSE;

          case OP_NOT_DIGIT:
            for (c = 0; c < 32; c++)
              start_bits[c] |= ~cd->cbits[c + cbit_digit];
            break;

          case OP_DIGIT:
            for (c = 0; c < 32; c++)
              start_bits[c] |= cd->cbits[c + cbit_digit];
            break;

          case OP_NOT_WHITESPACE:
            for (c = 0; c < 32; c++)
              start_bits[c] |= ~cd->cbits[c + cbit_space];
            break;

          case OP_WHITESPACE:
            for (c = 0; c < 32; c++)
              start_bits[c] |= cd->cbits[c + cbit_space];
            break;

          case OP_NOT_WORDCHAR:
            for (c = 0; c < 32; c++)
              start_bits[c] |= ~cd->cbits[c + cbit_word];
            break;

          case OP_WORDCHAR:
            for (c = 0; c < 32; c++)
              start_bits[c] |= cd->cbits[c + cbit_word];
            break;
          }

          tcode += 2;
          break;

          /* Character class where all the information is in a bit map: set the
             bits and either carry on or not, according to the repeat count. If it was
             a negative class, and we are operating with UTF-8 characters, any byte
             with a value >= 0xc4 is a potentially valid starter because it starts a
             character with a value > 255. */

        case OP_NCLASS:
          if (utf8) {
            start_bits[24] |= 0xf0;     /* Bits for 0xc4 - 0xc8 */
            memset(start_bits + 25, 0xff, 7);   /* Bits for 0xc9 - 0xff */
          }
          /* Fall through */

        case OP_CLASS:
          {
            tcode++;

            /* In UTF-8 mode, the bits in a bit map correspond to character
               values, not to byte values. However, the bit map we are constructing is
               for byte values. So we have to do a conversion for characters whose
               value is > 127. In fact, there are only two possible starting bytes for
               characters in the range 128 - 255. */

            if (utf8) {
              for (c = 0; c < 16; c++)
                start_bits[c] |= tcode[c];
              for (c = 128; c < 256; c++) {
                if ((tcode[c / 8] && (1 << (c & 7))) != 0) {
                  int d = (c >> 6) | 0xc0;      /* Set bit for this starter */
                  start_bits[d / 8] |= (1 << (d & 7));  /* and then skip on to the */
                  c = (c & 0xc0) + 0x40 - 1;    /* next relevant character. */
                }
              }
            }

            /* In non-UTF-8 mode, the two bit maps are completely compatible. */

            else {
              for (c = 0; c < 32; c++)
                start_bits[c] |= tcode[c];
            }

            /* Advance past the bit map, and act on what follows */

            tcode += 32;
            switch (*tcode) {
            case OP_CRSTAR:
            case OP_CRMINSTAR:
            case OP_CRQUERY:
            case OP_CRMINQUERY:
              tcode++;
              break;

            case OP_CRRANGE:
            case OP_CRMINRANGE:
              if (((tcode[1] << 8) + tcode[2]) == 0)
                tcode += 5;
              else
                try_next = FALSE;
              break;

            default:
              try_next = FALSE;
              break;
            }
          }
          break;                /* End of bitmap class handling */

        }                       /* End of switch */
    }                           /* End of try_next loop */

    code += GET(code, 1);       /* Advance to next branch */
  }
  while (*code == OP_ALT);
  return TRUE;
}



/*************************************************
*          Study a compiled expression           *
*************************************************/

/* This function is handed a compiled expression that it must study to produce
information that will speed up the matching. It returns a pcre_extra block
which then gets handed back to pcre_exec().

Arguments:
  re        points to the compiled expression
  options   contains option bits
  errorptr  points to where to place error messages;
            set NULL unless error

Returns:    pointer to a pcre_extra block, with study_data filled in and the
              appropriate flag set;
            NULL on error or if no optimization possible
*/

pcre_extra *
pcre_study(const pcre * external_re, int options, const char **errorptr)
{
  uschar start_bits[32];
  pcre_extra *extra;
  pcre_study_data *study;
  const uschar *tables;
  const real_pcre *re = (const real_pcre *) external_re;
  uschar *code = (uschar *) re + re->name_table_offset +
    (re->name_count * re->name_entry_size);
  compile_data compile_block;

  *errorptr = NULL;

  if (re == NULL || re->magic_number != MAGIC_NUMBER) {
    *errorptr = "argument is not a compiled regular expression";
    return NULL;
  }

  if ((options & ~PUBLIC_STUDY_OPTIONS) != 0) {
    *errorptr = "unknown or incorrect option bit(s) set";
    return NULL;
  }

/* For an anchored pattern, or an unanchored pattern that has a first char, or
a multiline pattern that matches only at "line starts", no further processing
at present. */

  if ((re->options & (PCRE_ANCHORED | PCRE_FIRSTSET | PCRE_STARTLINE)) != 0)
    return NULL;

/* Set the character tables in the block that is passed around */

  tables = re->tables;
  if (tables == NULL)
    (void) pcre_fullinfo(external_re, NULL, PCRE_INFO_DEFAULT_TABLES,
                         (void *) (&tables));

  compile_block.lcc = tables + lcc_offset;
  compile_block.fcc = tables + fcc_offset;
  compile_block.cbits = tables + cbits_offset;
  compile_block.ctypes = tables + ctypes_offset;

/* See if we can find a fixed set of initial characters for the pattern. */

  memset(start_bits, 0, 32 * sizeof(uschar));
  if (!set_start_bits(code, start_bits, (re->options & PCRE_CASELESS) != 0,
                      (re->options & PCRE_UTF8) != 0, &compile_block))
    return NULL;

/* Get a pcre_extra block and a pcre_study_data block. The study data is put in
the latter, which is pointed to by the former, which may also get additional
data set later by the calling program. At the moment, the size of
pcre_study_data is fixed. We nevertheless save it in a field for returning via
the pcre_fullinfo() function so that if it becomes variable in the future, we
don't have to change that code. */

  extra = (pcre_extra *) malloc(sizeof(pcre_extra) + sizeof(pcre_study_data));

  if (extra == NULL) {
    *errorptr = "failed to get memory";
    return NULL;
  }

  study = (pcre_study_data *) ((char *) extra + sizeof(pcre_extra));
  extra->flags = PCRE_EXTRA_STUDY_DATA;
  extra->study_data = study;

  study->size = sizeof(pcre_study_data);
  study->options = PCRE_STUDY_MAPPED;
  memcpy(study->start_bits, start_bits, sizeof(start_bits));

  return extra;
}

/* End of pcre_study.c */

/* pcre_compile.c */

/* This module contains the external function pcre_compile(), along with
supporting internal functions that are not used by other modules. */



/*************************************************
*      Code parameters and static tables         *
*************************************************/

/* Maximum number of items on the nested bracket stacks at compile time. This
applies to the nesting of all kinds of parentheses. It does not limit
un-nested, non-capturing parentheses. This number can be made bigger if
necessary - it is used to dimension one int and one unsigned char vector at
compile time. */

#define BRASTACK_SIZE 200


/* Table for handling escaped characters in the range '0'-'z'. Positive returns
are simple data values; negative values are for special things like \d and so
on. Zero means further processing is needed (for things like \x), or the escape
is invalid. */

#if !EBCDIC                     /* This is the "normal" table for ASCII systems */
static const short int escapes[] = {
  0, 0, 0, 0, 0, 0, 0, 0,       /* 0 - 7 */
  0, 0, ':', ';', '<', '=', '>', '?',   /* 8 - ? */
  '@', -ESC_A, -ESC_B, -ESC_C, -ESC_D, -ESC_E, 0, -ESC_G,       /* @ - G */
  0, 0, 0, 0, 0, 0, 0, 0,       /* H - O */
  -ESC_P, -ESC_Q, 0, -ESC_S, 0, 0, 0, -ESC_W,   /* P - W */
  -ESC_X, 0, -ESC_Z, '[', '\\', ']', '^', '_',  /* X - _ */
  '`', 7, -ESC_b, 0, -ESC_d, ESC_e, ESC_f, 0,   /* ` - g */
  0, 0, 0, 0, 0, 0, ESC_n, 0,   /* h - o */
  -ESC_p, 0, ESC_r, -ESC_s, ESC_tee, 0, 0, -ESC_w,      /* p - w */
  0, 0, -ESC_z                  /* x - z */
};

#else                           /* This is the "abnormal" table for EBCDIC systems */
static const short int escapes[] = {
/*  48 */ 0, 0, 0, '.', '<', '(', '+', '|',
/*  50 */ '&', 0, 0, 0, 0, 0, 0, 0,
/*  58 */ 0, 0, '!', '$', '*', ')', ';', '~',
/*  60 */ '-', '/', 0, 0, 0, 0, 0, 0,
/*  68 */ 0, 0, '|', ',', '%', '_', '>', '?',
/*  70 */ 0, 0, 0, 0, 0, 0, 0, 0,
/*  78 */ 0, '`', ':', '#', '@', '\'', '=', '"',
/*  80 */ 0, 7, -ESC_b, 0, -ESC_d, ESC_e, ESC_f, 0,
/*  88 */ 0, 0, 0, '{', 0, 0, 0, 0,
/*  90 */ 0, 0, 0, 'l', 0, ESC_n, 0, -ESC_p,
/*  98 */ 0, ESC_r, 0, '}', 0, 0, 0, 0,
/*  A0 */ 0, '~', -ESC_s, ESC_tee, 0, 0, -ESC_w, 0,
/*  A8 */ 0, -ESC_z, 0, 0, 0, '[', 0, 0,
/*  B0 */ 0, 0, 0, 0, 0, 0, 0, 0,
/*  B8 */ 0, 0, 0, 0, 0, ']', '=', '-',
/*  C0 */ '{', -ESC_A, -ESC_B, -ESC_C, -ESC_D, -ESC_E, 0, -ESC_G,
/*  C8 */ 0, 0, 0, 0, 0, 0, 0, 0,
/*  D0 */ '}', 0, 0, 0, 0, 0, 0, -ESC_P,
/*  D8 */ -ESC_Q, 0, 0, 0, 0, 0, 0, 0,
/*  E0 */ '\\', 0, -ESC_S, 0, 0, 0, -ESC_W, -ESC_X,
/*  E8 */ 0, -ESC_Z, 0, 0, 0, 0, 0, 0,
/*  F0 */ 0, 0, 0, 0, 0, 0, 0, 0,
/*  F8 */ 0, 0, 0, 0, 0, 0, 0, 0
};
#endif


/* Tables of names of POSIX character classes and their lengths. The list is
terminated by a zero length entry. The first three must be alpha, upper, lower,
as this is assumed for handling case independence. */

static const char *const posix_names[] = {
  "alpha", "lower", "upper",
  "alnum", "ascii", "blank", "cntrl", "digit", "graph",
  "print", "punct", "space", "word", "xdigit"
};

static const uschar posix_name_lengths[] = {
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 6, 0
};

/* Table of class bit maps for each POSIX class; up to three may be combined
to form the class. The table for [:blank:] is dynamically modified to remove
the vertical space characters. */

static const int posix_class_maps[] = {
  cbit_lower, cbit_upper, -1,   /* alpha */
  cbit_lower, -1, -1,           /* lower */
  cbit_upper, -1, -1,           /* upper */
  cbit_digit, cbit_lower, cbit_upper,   /* alnum */
  cbit_print, cbit_cntrl, -1,   /* ascii */
  cbit_space, -1, -1,           /* blank - a GNU extension */
  cbit_cntrl, -1, -1,           /* cntrl */
  cbit_digit, -1, -1,           /* digit */
  cbit_graph, -1, -1,           /* graph */
  cbit_print, -1, -1,           /* print */
  cbit_punct, -1, -1,           /* punct */
  cbit_space, -1, -1,           /* space */
  cbit_word, -1, -1,            /* word - a Perl extension */
  cbit_xdigit, -1, -1           /* xdigit */
};


/* The texts of compile-time error messages. These are "char *" because they
are passed to the outside world. */

static const char *error_texts[] = {
  "no error",
  "\\ at end of pattern",
  "\\c at end of pattern",
  "unrecognized character follows \\",
  "numbers out of order in {} quantifier",
  /* 5 */
  "number too big in {} quantifier",
  "missing terminating ] for character class",
  "invalid escape sequence in character class",
  "range out of order in character class",
  "nothing to repeat",
  /* 10 */
  "operand of unlimited repeat could match the empty string",
  "internal error: unexpected repeat",
  "unrecognized character after (?",
  "POSIX named classes are supported only within a class",
  "missing )",
  /* 15 */
  "reference to non-existent subpattern",
  "erroffset passed as NULL",
  "unknown option bit(s) set",
  "missing ) after comment",
  "parentheses nested too deeply",
  /* 20 */
  "regular expression too large",
  "failed to get memory",
  "unmatched parentheses",
  "internal error: code overflow",
  "unrecognized character after (?<",
  /* 25 */
  "lookbehind assertion is not fixed length",
  "malformed number after (?(",
  "conditional group contains more than two branches",
  "assertion expected after (?(",
  "(?R or (?digits must be followed by )",
  /* 30 */
  "unknown POSIX class name",
  "POSIX collating elements are not supported",
  "this version of PCRE is not compiled with PCRE_UTF8 support",
  "spare error",
  "character value in \\x{...} sequence is too large",
  /* 35 */
  "invalid condition (?(0)",
  "\\C not allowed in lookbehind assertion",
  "PCRE does not support \\L, \\l, \\N, \\U, or \\u",
  "number after (?C is > 255",
  "closing ) for (?C expected",
  /* 40 */
  "recursive call could loop indefinitely",
  "unrecognized character after (?P",
  "syntax error after (?P",
  "two named groups have the same name",
  "invalid UTF-8 string",
  /* 45 */
  "support for \\P, \\p, and \\X has not been compiled",
  "malformed \\P or \\p sequence",
  "unknown property name after \\P or \\p"
};


/* Table to identify digits and hex digits. This is used when compiling
patterns. Note that the tables in chartables are dependent on the locale, and
may mark arbitrary characters as digits - but the PCRE compiling code expects
to handle only 0-9, a-z, and A-Z as digits when compiling. That is why we have
a private table here. It costs 256 bytes, but it is a lot faster than doing
character value tests (at least in some simple cases I timed), and in some
applications one wants PCRE to compile efficiently as well as match
efficiently.

For convenience, we use the same bit definitions as in chartables:

  0x04   decimal digit
  0x08   hexadecimal digit

Then we can use ctype_digit and ctype_xdigit in the code. */

#if !EBCDIC                     /* This is the "normal" case, for ASCII systems */
static const unsigned char digitab[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*   0-  7 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*   8- 15 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  16- 23 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  24- 31 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*    - '  */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  ( - /  */
  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,       /*  0 - 7  */
  0x0c, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  8 - ?  */
  0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00,       /*  @ - G  */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  H - O  */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  P - W  */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  X - _  */
  0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00,       /*  ` - g  */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  h - o  */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  p - w  */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  x -127 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 128-135 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 136-143 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 144-151 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 152-159 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 160-167 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 168-175 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 176-183 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 184-191 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 192-199 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 200-207 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 208-215 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 216-223 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 224-231 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 232-239 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 240-247 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};                              /* 248-255 */

#else                           /* This is the "abnormal" case, for EBCDIC systems */
static const unsigned char digitab[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*   0-  7  0 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*   8- 15    */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  16- 23 10 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  24- 31    */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  32- 39 20 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  40- 47    */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  48- 55 30 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  56- 63    */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*    - 71 40 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  72- |     */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  & - 87 50 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  88- ¬     */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  - -103 60 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 104- ?     */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 112-119 70 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 120- "     */
  0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00,       /* 128- g  80 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  h -143    */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 144- p  90 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  q -159    */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 160- x  A0 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  y -175    */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  ^ -183 B0 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 184-191    */
  0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00,       /*  { - G  C0 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  H -207    */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  } - P  D0 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  Q -223    */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  \ - X  E0 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  Y -239    */
  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,       /*  0 - 7  F0 */
  0x0c, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};                              /*  8 -255    */

static const unsigned char ebcdic_chartab[] = { /* chartable partial dup */
  0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,       /*   0-  7 */
  0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,       /*   8- 15 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,       /*  16- 23 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  24- 31 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,       /*  32- 39 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  40- 47 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  48- 55 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  56- 63 */
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*    - 71 */
  0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x80, 0x80,       /*  72- |  */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  & - 87 */
  0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00,       /*  88- ¬  */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  - -103 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80,       /* 104- ?  */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 112-119 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 120- "  */
  0x00, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x12,       /* 128- g  */
  0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  h -143 */
  0x00, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,       /* 144- p  */
  0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  q -159 */
  0x00, 0x00, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,       /* 160- x  */
  0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  y -175 */
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  ^ -183 */
  0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,       /* 184-191 */
  0x80, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x12,       /*  { - G  */
  0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  H -207 */
  0x00, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,       /*  } - P  */
  0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  Q -223 */
  0x00, 0x00, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,       /*  \ - X  */
  0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /*  Y -239 */
  0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,       /*  0 - 7  */
  0x1c, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};                              /*  8 -255 */
#endif


/* Definition to allow mutual recursion */

static BOOL















compile_regex(int, int, int *, uschar **, const uschar **, int *, BOOL, int,
              int *, int *, branch_chain *, compile_data *);



/*************************************************
*            Handle escapes                      *
*************************************************/

/* This function is called when a \ has been encountered. It either returns a
positive value for a simple escape such as \n, or a negative value which
encodes one of the more complicated things such as \d. When UTF-8 is enabled,
a positive value greater than 255 may be returned. On entry, ptr is pointing at
the \. On exit, it is on the final character of the escape sequence.

Arguments:
  ptrptr         points to the pattern position pointer
  errorcodeptr   points to the errorcode variable
  bracount       number of previous extracting brackets
  options        the options bits
  isclass        TRUE if inside a character class

Returns:         zero or positive => a data character
                 negative => a special escape sequence
                 on error, errorptr is set
*/

static int
check_escape(const uschar **ptrptr, int *errorcodeptr, int bracount,
             int options, BOOL isclass)
{
  const uschar *ptr = *ptrptr;
  int c, i;

/* If backslash is at the end of the pattern, it's an error. */

  c = *(++ptr);
  if (c == 0)
    *errorcodeptr = ERR1;

/* Non-alphamerics are literals. For digits or letters, do an initial lookup in
a table. A non-zero result is something that can be returned immediately.
Otherwise further processing may be required. */

#if !EBCDIC                     /* ASCII coding */
  else if (c < '0' || c > 'z') {
  } /* Not alphameric */
  else if ((i = escapes[c - '0']) != 0)
    c = i;

#else                           /* EBCDIC coding */
  else if (c < 'a' || (ebcdic_chartab[c] & 0x0E) == 0) {
  } /* Not alphameric */
  else if ((i = escapes[c - 0x48]) != 0)
    c = i;
#endif

/* Escapes that need further processing, or are illegal. */

  else {
    const uschar *oldptr;
    switch (c) {
      /* A number of Perl escapes are not handled by PCRE. We give an explicit
         error. */

    case 'l':
    case 'L':
    case 'N':
    case 'u':
    case 'U':
      *errorcodeptr = ERR37;
      break;

      /* The handling of escape sequences consisting of a string of digits
         starting with one that is not zero is not straightforward. By experiment,
         the way Perl works seems to be as follows:

         Outside a character class, the digits are read as a decimal number. If the
         number is less than 10, or if there are that many previous extracting
         left brackets, then it is a back reference. Otherwise, up to three octal
         digits are read to form an escaped byte. Thus \123 is likely to be octal
         123 (cf \0123, which is octal 012 followed by the literal 3). If the octal
         value is greater than 377, the least significant 8 bits are taken. Inside a
         character class, \ followed by a digit is always an octal number. */

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':

      if (!isclass) {
        oldptr = ptr;
        c -= '0';
        while ((digitab[ptr[1]] & ctype_digit) != 0)
          c = c * 10 + *(++ptr) - '0';
        if (c < 10 || c <= bracount) {
          c = -(ESC_REF + c);
          break;
        }
        ptr = oldptr;           /* Put the pointer back and fall through */
      }

      /* Handle an octal number following \. If the first digit is 8 or 9, Perl
         generates a binary zero byte and treats the digit as a following literal.
         Thus we have to pull back the pointer by one. */

      if ((c = *ptr) >= '8') {
        ptr--;
        c = 0;
        break;
      }

      /* \0 always starts an octal number, but we may drop through to here with a
         larger first octal digit. */

    case '0':
      c -= '0';
      while (i++ < 2 && ptr[1] >= '0' && ptr[1] <= '7')
        c = c * 8 + *(++ptr) - '0';
      c &= 255;                 /* Take least significant 8 bits */
      break;

      /* \x is complicated when UTF-8 is enabled. \x{ddd} is a character number
         which can be greater than 0xff, but only if the ddd are hex digits. */

    case 'x':

      /* Read just a single hex char */

      c = 0;
      while (i++ < 2 && (digitab[ptr[1]] & ctype_xdigit) != 0) {
        int cc;                 /* Some compilers don't like ++ */
        cc = *(++ptr);          /* in initializers */
#if !EBCDIC                     /* ASCII coding */
        if (cc >= 'a')
          cc -= 32;             /* Convert to upper case */
        c = c * 16 + cc - ((cc < 'A') ? '0' : ('A' - 10));
#else                           /* EBCDIC coding */
        if (cc <= 'z')
          cc += 64;             /* Convert to upper case */
        c = c * 16 + cc - ((cc >= '0') ? '0' : ('A' - 10));
#endif
      }
      break;

      /* Other special escapes not starting with a digit are straightforward */

    case 'c':
      c = *(++ptr);
      if (c == 0) {
        *errorcodeptr = ERR2;
        return 0;
      }

      /* A letter is upper-cased; then the 0x40 bit is flipped. This coding
         is ASCII-specific, but then the whole concept of \cx is ASCII-specific.
         (However, an EBCDIC equivalent has now been added.) */

#if !EBCDIC                     /* ASCII coding */
      if (c >= 'a' && c <= 'z')
        c -= 32;
      c ^= 0x40;
#else                           /* EBCDIC coding */
      if (c >= 'a' && c <= 'z')
        c += 64;
      c ^= 0xC0;
#endif
      break;

      /* PCRE_EXTRA enables extensions to Perl in the matter of escapes. Any
         other alphameric following \ is an error if PCRE_EXTRA was set; otherwise,
         for Perl compatibility, it is a literal. This code looks a bit odd, but
         there used to be some cases other than the default, and there may be again
         in future, so I haven't "optimized" it. */

    default:
      if ((options & PCRE_EXTRA) != 0)
        switch (c) {
        default:
          *errorcodeptr = ERR3;
          break;
        }
      break;
    }
  }

  *ptrptr = ptr;
  return c;
}



#ifdef SUPPORT_UCP
/*************************************************
*               Handle \P and \p                 *
*************************************************/

/* This function is called after \P or \p has been encountered, provided that
PCRE is compiled with support for Unicode properties. On entry, ptrptr is
pointing at the P or p. On exit, it is pointing at the final character of the
escape sequence.

Argument:
  ptrptr         points to the pattern position pointer
  negptr         points to a boolean that is set TRUE for negation else FALSE
  errorcodeptr   points to the error code variable

Returns:     value from ucp_type_table, or -1 for an invalid type
*/

static int
get_ucp(const uschar **ptrptr, BOOL *negptr, int *errorcodeptr)
{
  int c, i, bot, top;
  const uschar *ptr = *ptrptr;
  char name[4];

  c = *(++ptr);
  if (c == 0)
    goto ERROR_RETURN;

  *negptr = FALSE;

/* \P or \p can be followed by a one- or two-character name in {}, optionally
preceded by ^ for negation. */

  if (c == '{') {
    if (ptr[1] == '^') {
      *negptr = TRUE;
      ptr++;
    }
    for (i = 0; i <= 2; i++) {
      c = *(++ptr);
      if (c == 0)
        goto ERROR_RETURN;
      if (c == '}')
        break;
      name[i] = c;
    }
    if (c != '}') {             /* Try to distinguish error cases */
      while (*(++ptr) != 0 && *ptr != '}') ;
      if (*ptr == '}')
        goto UNKNOWN_RETURN;
      else
        goto ERROR_RETURN;
    }
    name[i] = 0;
  }

/* Otherwise there is just one following character */

  else {
    name[0] = c;
    name[1] = 0;
  }

  *ptrptr = ptr;

/* Search for a recognized property name using binary chop */

  bot = 0;
  top = _pcre_utt_size;

  while (bot < top) {
    i = (bot + top) / 2;
    c = strcmp(name, _pcre_utt[i].name);
    if (c == 0)
      return _pcre_utt[i].value;
    if (c > 0)
      bot = i + 1;
    else
      top = i;
  }

UNKNOWN_RETURN:
  *errorcodeptr = ERR47;
  *ptrptr = ptr;
  return -1;

ERROR_RETURN:
  *errorcodeptr = ERR46;
  *ptrptr = ptr;
  return -1;
}
#endif




/*************************************************
*            Check for counted repeat            *
*************************************************/

/* This function is called when a '{' is encountered in a place where it might
start a quantifier. It looks ahead to see if it really is a quantifier or not.
It is only a quantifier if it is one of the forms {ddd} {ddd,} or {ddd,ddd}
where the ddds are digits.

Arguments:
  p         pointer to the first char after '{'

Returns:    TRUE or FALSE
*/

static BOOL
is_counted_repeat(const uschar *p)
{
  if ((digitab[*p++] & ctype_digit) == 0)
    return FALSE;
  while ((digitab[*p] & ctype_digit) != 0)
    p++;
  if (*p == '}')
    return TRUE;

  if (*p++ != ',')
    return FALSE;
  if (*p == '}')
    return TRUE;

  if ((digitab[*p++] & ctype_digit) == 0)
    return FALSE;
  while ((digitab[*p] & ctype_digit) != 0)
    p++;

  return (*p == '}');
}



/*************************************************
*         Read repeat counts                     *
*************************************************/

/* Read an item of the form {n,m} and return the values. This is called only
after is_counted_repeat() has confirmed that a repeat-count quantifier exists,
so the syntax is guaranteed to be correct, but we need to check the values.

Arguments:
  p              pointer to first char after '{'
  minp           pointer to int for min
  maxp           pointer to int for max
                 returned as -1 if no max
  errorcodeptr   points to error code variable

Returns:         pointer to '}' on success;
                 current ptr on error, with errorcodeptr set non-zero
*/

static const uschar *
read_repeat_counts(const uschar *p, int *minp, int *maxp, int *errorcodeptr)
{
  int min = 0;
  int max = -1;

/* Read the minimum value and do a paranoid check: a negative value indicates
an integer overflow. */

  while ((digitab[*p] & ctype_digit) != 0)
    min = min * 10 + *p++ - '0';
  if (min < 0 || min > 65535) {
    *errorcodeptr = ERR5;
    return p;
  }

/* Read the maximum value if there is one, and again do a paranoid on its size.
Also, max must not be less than min. */

  if (*p == '}')
    max = min;
  else {
    if (*(++p) != '}') {
      max = 0;
      while ((digitab[*p] & ctype_digit) != 0)
        max = max * 10 + *p++ - '0';
      if (max < 0 || max > 65535) {
        *errorcodeptr = ERR5;
        return p;
      }
      if (max < min) {
        *errorcodeptr = ERR4;
        return p;
      }
    }
  }

/* Fill in the required variables, and pass back the pointer to the terminating
'}'. */

  *minp = min;
  *maxp = max;
  return p;
}



/*************************************************
*      Find first significant op code            *
*************************************************/

/* This is called by several functions that scan a compiled expression looking
for a fixed first character, or an anchoring op code etc. It skips over things
that do not influence this. For some calls, a change of option is important.
For some calls, it makes sense to skip negative forward and all backward
assertions, and also the \b assertion; for others it does not.

Arguments:
  code         pointer to the start of the group
  options      pointer to external options
  optbit       the option bit whose changing is significant, or
                 zero if none are
  skipassert   TRUE if certain assertions are to be skipped

Returns:       pointer to the first significant opcode
*/

static const uschar *
first_significant_code(const uschar *code, int *options, int optbit,
                       BOOL skipassert)
{
  for (;;) {
    switch ((int) *code) {
    case OP_OPT:
      if (optbit > 0 && ((int) code[1] & optbit) != (*options & optbit))
        *options = (int) code[1];
      code += 2;
      break;

    case OP_ASSERT_NOT:
    case OP_ASSERTBACK:
    case OP_ASSERTBACK_NOT:
      if (!skipassert)
        return code;
      do
        code += GET(code, 1);
      while (*code == OP_ALT);
      code += _pcre_OP_lengths[*code];
      break;

    case OP_WORD_BOUNDARY:
    case OP_NOT_WORD_BOUNDARY:
      if (!skipassert)
        return code;
      /* Fall through */

    case OP_CALLOUT:
    case OP_CREF:
    case OP_BRANUMBER:
      code += _pcre_OP_lengths[*code];
      break;

    default:
      return code;
    }
  }
/* Control never reaches here */
}




/*************************************************
*        Find the fixed length of a pattern      *
*************************************************/

/* Scan a pattern and compute the fixed length of subject that will match it,
if the length is fixed. This is needed for dealing with backward assertions.
In UTF8 mode, the result is in characters rather than bytes.

Arguments:
  code     points to the start of the pattern (the bracket)
  options  the compiling options

Returns:   the fixed length, or -1 if there is no fixed length,
             or -2 if \C was encountered
*/

static int
find_fixedlength(uschar *code, int options)
{
  int length = -1;

  register int branchlength = 0;
  register uschar *cc = code + 1 + LINK_SIZE;

/* Scan along the opcodes for this branch. If we get to the end of the
branch, check the length against that of the other branches. */

  for (;;) {
    int d;
    register int op = *cc;
    if (op >= OP_BRA)
      op = OP_BRA;

    switch (op) {
    case OP_BRA:
    case OP_ONCE:
    case OP_COND:
      d = find_fixedlength(cc, options);
      if (d < 0)
        return d;
      branchlength += d;
      do
        cc += GET(cc, 1);
      while (*cc == OP_ALT);
      cc += 1 + LINK_SIZE;
      break;

      /* Reached end of a branch; if it's a ket it is the end of a nested
         call. If it's ALT it is an alternation in a nested call. If it is
         END it's the end of the outer call. All can be handled by the same code. */

    case OP_ALT:
    case OP_KET:
    case OP_KETRMAX:
    case OP_KETRMIN:
    case OP_END:
      if (length < 0)
        length = branchlength;
      else if (length != branchlength)
        return -1;
      if (*cc != OP_ALT)
        return length;
      cc += 1 + LINK_SIZE;
      branchlength = 0;
      break;

      /* Skip over assertive subpatterns */

    case OP_ASSERT:
    case OP_ASSERT_NOT:
    case OP_ASSERTBACK:
    case OP_ASSERTBACK_NOT:
      do
        cc += GET(cc, 1);
      while (*cc == OP_ALT);
      /* Fall through */

      /* Skip over things that don't match chars */

    case OP_REVERSE:
    case OP_BRANUMBER:
    case OP_CREF:
    case OP_OPT:
    case OP_CALLOUT:
    case OP_SOD:
    case OP_SOM:
    case OP_EOD:
    case OP_EODN:
    case OP_CIRC:
    case OP_DOLL:
    case OP_NOT_WORD_BOUNDARY:
    case OP_WORD_BOUNDARY:
      cc += _pcre_OP_lengths[*cc];
      break;

      /* Handle literal characters */

    case OP_CHAR:
    case OP_CHARNC:
      branchlength++;
      cc += 2;
      break;

      /* Handle exact repetitions. The count is already in characters, but we
         need to skip over a multibyte character in UTF8 mode.  */

    case OP_EXACT:
      branchlength += GET2(cc, 1);
      cc += 4;
      break;

    case OP_TYPEEXACT:
      branchlength += GET2(cc, 1);
      cc += 4;
      break;

      /* Handle single-char matchers */

    case OP_PROP:
    case OP_NOTPROP:
      cc++;
      /* Fall through */

    case OP_NOT_DIGIT:
    case OP_DIGIT:
    case OP_NOT_WHITESPACE:
    case OP_WHITESPACE:
    case OP_NOT_WORDCHAR:
    case OP_WORDCHAR:
    case OP_ANY:
      branchlength++;
      cc++;
      break;

      /* The single-byte matcher isn't allowed */

    case OP_ANYBYTE:
      return -2;

      /* Check a class for variable quantification */


    case OP_CLASS:
    case OP_NCLASS:
      cc += 33;

      switch (*cc) {
      case OP_CRSTAR:
      case OP_CRMINSTAR:
      case OP_CRQUERY:
      case OP_CRMINQUERY:
        return -1;

      case OP_CRRANGE:
      case OP_CRMINRANGE:
        if (GET2(cc, 1) != GET2(cc, 3))
          return -1;
        branchlength += GET2(cc, 1);
        cc += 5;
        break;

      default:
        branchlength++;
      }
      break;

      /* Anything else is variable length */

    default:
      return -1;
    }
  }
/* Control never gets here */
}




/*************************************************
*    Scan compiled regex for numbered bracket    *
*************************************************/

/* This little function scans through a compiled pattern until it finds a
capturing bracket with the given number.

Arguments:
  code        points to start of expression
  utf8        TRUE in UTF-8 mode
  number      the required bracket number

Returns:      pointer to the opcode for the bracket, or NULL if not found
*/

static const uschar *
find_bracket(const uschar *code, BOOL utf8, int number)
{
  utf8 = utf8;                  /* Stop pedantic compilers complaining */

  for (;;) {
    register int c = *code;
    if (c == OP_END)
      return NULL;
    else if (c > OP_BRA) {
      int n = c - OP_BRA;
      if (n > EXTRACT_BASIC_MAX)
        n = GET2(code, 2 + LINK_SIZE);
      if (n == number)
        return (uschar *) code;
      code += _pcre_OP_lengths[OP_BRA];
    } else {
      code += _pcre_OP_lengths[c];

    }
  }
}



/*************************************************
*   Scan compiled regex for recursion reference  *
*************************************************/

/* This little function scans through a compiled pattern until it finds an
instance of OP_RECURSE.

Arguments:
  code        points to start of expression
  utf8        TRUE in UTF-8 mode

Returns:      pointer to the opcode for OP_RECURSE, or NULL if not found
*/

static const uschar *
find_recurse(const uschar *code, BOOL utf8)
{
  utf8 = utf8;                  /* Stop pedantic compilers complaining */

  for (;;) {
    register int c = *code;
    if (c == OP_END)
      return NULL;
    else if (c == OP_RECURSE)
      return code;
    else if (c > OP_BRA) {
      code += _pcre_OP_lengths[OP_BRA];
    } else {
      code += _pcre_OP_lengths[c];

    }
  }
}



/*************************************************
*    Scan compiled branch for non-emptiness      *
*************************************************/

/* This function scans through a branch of a compiled pattern to see whether it
can match the empty string or not. It is called only from could_be_empty()
below. Note that first_significant_code() skips over assertions. If we hit an
unclosed bracket, we return "empty" - this means we've struck an inner bracket
whose current branch will already have been scanned.

Arguments:
  code        points to start of search
  endcode     points to where to stop
  utf8        TRUE if in UTF8 mode

Returns:      TRUE if what is matched could be empty
*/

static BOOL
could_be_empty_branch(const uschar *code, const uschar *endcode, BOOL utf8)
{
  register int c;
  for (code = first_significant_code(code + 1 + LINK_SIZE, NULL, 0, TRUE);
       code < endcode;
       code = first_significant_code(code + _pcre_OP_lengths[c], NULL, 0, TRUE))
  {
    const uschar *ccode;

    c = *code;

    if (c >= OP_BRA) {
      BOOL empty_branch;
      if (GET(code, 1) == 0)
        return TRUE;            /* Hit unclosed bracket */

      /* Scan a closed bracket */

      empty_branch = FALSE;
      do {
        if (!empty_branch && could_be_empty_branch(code, endcode, utf8))
          empty_branch = TRUE;
        code += GET(code, 1);
      }
      while (*code == OP_ALT);
      if (!empty_branch)
        return FALSE;           /* All branches are non-empty */
      code += 1 + LINK_SIZE;
      c = *code;
    }

    else
      switch (c) {
        /* Check for quantifiers after a class */


      case OP_CLASS:
      case OP_NCLASS:
        ccode = code + 33;


        switch (*ccode) {
        case OP_CRSTAR:        /* These could be empty; continue */
        case OP_CRMINSTAR:
        case OP_CRQUERY:
        case OP_CRMINQUERY:
          break;

        default:               /* Non-repeat => class must match */
        case OP_CRPLUS:        /* These repeats aren't empty */
        case OP_CRMINPLUS:
          return FALSE;

        case OP_CRRANGE:
        case OP_CRMINRANGE:
          if (GET2(ccode, 1) > 0)
            return FALSE;       /* Minimum > 0 */
          break;
        }
        break;

        /* Opcodes that must match a character */

      case OP_PROP:
      case OP_NOTPROP:
      case OP_EXTUNI:
      case OP_NOT_DIGIT:
      case OP_DIGIT:
      case OP_NOT_WHITESPACE:
      case OP_WHITESPACE:
      case OP_NOT_WORDCHAR:
      case OP_WORDCHAR:
      case OP_ANY:
      case OP_ANYBYTE:
      case OP_CHAR:
      case OP_CHARNC:
      case OP_NOT:
      case OP_PLUS:
      case OP_MINPLUS:
      case OP_EXACT:
      case OP_NOTPLUS:
      case OP_NOTMINPLUS:
      case OP_NOTEXACT:
      case OP_TYPEPLUS:
      case OP_TYPEMINPLUS:
      case OP_TYPEEXACT:
        return FALSE;

        /* End of branch */

      case OP_KET:
      case OP_KETRMAX:
      case OP_KETRMIN:
      case OP_ALT:
        return TRUE;

        /* In UTF-8 mode, STAR, MINSTAR, QUERY, MINQUERY, UPTO, and MINUPTO  may be
           followed by a multibyte character */

      }
  }

  return TRUE;
}



/*************************************************
*    Scan compiled regex for non-emptiness       *
*************************************************/

/* This function is called to check for left recursive calls. We want to check
the current branch of the current pattern to see if it could match the empty
string. If it could, we must look outwards for branches at other levels,
stopping when we pass beyond the bracket which is the subject of the recursion.

Arguments:
  code        points to start of the recursion
  endcode     points to where to stop (current RECURSE item)
  bcptr       points to the chain of current (unclosed) branch starts
  utf8        TRUE if in UTF-8 mode

Returns:      TRUE if what is matched could be empty
*/

static BOOL
could_be_empty(const uschar *code, const uschar *endcode,
               branch_chain *bcptr, BOOL utf8)
{
  while (bcptr != NULL && bcptr->current >= code) {
    if (!could_be_empty_branch(bcptr->current, endcode, utf8))
      return FALSE;
    bcptr = bcptr->outer;
  }
  return TRUE;
}



/*************************************************
*           Check for POSIX class syntax         *
*************************************************/

/* This function is called when the sequence "[:" or "[." or "[=" is
encountered in a character class. It checks whether this is followed by an
optional ^ and then a sequence of letters, terminated by a matching ":]" or
".]" or "=]".

Argument:
  ptr      pointer to the initial [
  endptr   where to return the end pointer
  cd       pointer to compile data

Returns:   TRUE or FALSE
*/

static BOOL
check_posix_syntax(const uschar *ptr, const uschar **endptr, compile_data *cd)
{
  int terminator;               /* Don't combine these lines; the Solaris cc */
  terminator = *(++ptr);        /* compiler warns about "non-constant" initializer. */
  if (*(++ptr) == '^')
    ptr++;
  while ((cd->ctypes[*ptr] & ctype_letter) != 0)
    ptr++;
  if (*ptr == terminator && ptr[1] == ']') {
    *endptr = ptr;
    return TRUE;
  }
  return FALSE;
}




/*************************************************
*          Check POSIX class name                *
*************************************************/

/* This function is called to check the name given in a POSIX-style class entry
such as [:alnum:].

Arguments:
  ptr        points to the first letter
  len        the length of the name

Returns:     a value representing the name, or -1 if unknown
*/

static int
check_posix_name(const uschar *ptr, int len)
{
  register int yield = 0;
  while (posix_name_lengths[yield] != 0) {
    if (len == posix_name_lengths[yield] &&
        strncmp((const char *) ptr, posix_names[yield], len) == 0)
      return yield;
    yield++;
  }
  return -1;
}


/*************************************************
*    Adjust OP_RECURSE items in repeated group   *
*************************************************/

/* OP_RECURSE items contain an offset from the start of the regex to the group
that is referenced. This means that groups can be replicated for fixed
repetition simply by copying (because the recursion is allowed to refer to
earlier groups that are outside the current group). However, when a group is
optional (i.e. the minimum quantifier is zero), OP_BRAZERO is inserted before
it, after it has been compiled. This means that any OP_RECURSE items within it
that refer to the group itself or any contained groups have to have their
offsets adjusted. That is the job of this function. Before it is called, the
partially compiled regex must be temporarily terminated with OP_END.

Arguments:
  group      points to the start of the group
  adjust     the amount by which the group is to be moved
  utf8       TRUE in UTF-8 mode
  cd         contains pointers to tables etc.

Returns:     nothing
*/

static void
adjust_recurse(uschar *group, int adjust, BOOL utf8, compile_data *cd)
{
  uschar *ptr = group;
  while ((ptr = (uschar *) find_recurse(ptr, utf8)) != NULL) {
    int offset = GET(ptr, 1);
    if (cd->start_code + offset >= group)
      PUT(ptr, 1, offset + adjust);
    ptr += 1 + LINK_SIZE;
  }
}



/*************************************************
*        Insert an automatic callout point       *
*************************************************/

/* This function is called when the PCRE_AUTO_CALLOUT option is set, to insert
callout points before each pattern item.

Arguments:
  code           current code pointer
  ptr            current pattern pointer
  cd             pointers to tables etc

Returns:         new code pointer
*/

static uschar *
auto_callout(uschar *code, const uschar *ptr, compile_data *cd)
{
  *code++ = OP_CALLOUT;
  *code++ = 255;
  PUT(code, 0, ptr - cd->start_pattern);        /* Pattern offset */
  PUT(code, LINK_SIZE, 0);      /* Default length */
  return code + 2 * LINK_SIZE;
}



/*************************************************
*         Complete a callout item                *
*************************************************/

/* A callout item contains the length of the next item in the pattern, which
we can't fill in till after we have reached the relevant point. This is used
for both automatic and manual callouts.

Arguments:
  previous_callout   points to previous callout item
  ptr                current pattern pointer
  cd                 pointers to tables etc

Returns:             nothing
*/

static void
complete_callout(uschar *previous_callout, const uschar *ptr, compile_data *cd)
{
  int length = ptr - cd->start_pattern - GET(previous_callout, 2);
  PUT(previous_callout, 2 + LINK_SIZE, length);
}



#ifdef SUPPORT_UCP
/*************************************************
*           Get othercase range                  *
*************************************************/

/* This function is passed the start and end of a class range, in UTF-8 mode
with UCP support. It searches up the characters, looking for internal ranges of
characters in the "other" case. Each call returns the next one, updating the
start address.

Arguments:
  cptr        points to starting character value; updated
  d           end value
  ocptr       where to put start of othercase range
  odptr       where to put end of othercase range

Yield:        TRUE when range returned; FALSE when no more
*/

static BOOL
get_othercase_range(int *cptr, int d, int *ocptr, int *odptr)
{
  int c, chartype, othercase, next;

  for (c = *cptr; c <= d; c++) {
    if (_pcre_ucp_findchar(c, &chartype, &othercase) == ucp_L && othercase != 0)
      break;
  }

  if (c > d)
    return FALSE;

  *ocptr = othercase;
  next = othercase + 1;

  for (++c; c <= d; c++) {
    if (_pcre_ucp_findchar(c, &chartype, &othercase) != ucp_L ||
        othercase != next)
      break;
    next++;
  }

  *odptr = next - 1;
  *cptr = c;

  return TRUE;
}
#endif                          /* SUPPORT_UCP */


/*************************************************
*           Compile one branch                   *
*************************************************/

/* Scan the pattern, compiling it into the code vector. If the options are
changed during the branch, the pointer is used to change the external options
bits.

Arguments:
  optionsptr     pointer to the option bits
  brackets       points to number of extracting brackets used
  codeptr        points to the pointer to the current code point
  ptrptr         points to the current pattern pointer
  errorcodeptr   points to error code variable
  firstbyteptr   set to initial literal character, or < 0 (REQ_UNSET, REQ_NONE)
  reqbyteptr     set to the last literal character required, else < 0
  bcptr          points to current branch chain
  cd             contains pointers to tables etc.

Returns:         TRUE on success
                 FALSE, with *errorcodeptr set non-zero on error
*/

static BOOL
compile_branch(int *optionsptr, int *brackets, uschar **codeptr,
               const uschar **ptrptr, int *errorcodeptr, int *firstbyteptr,
               int *reqbyteptr, branch_chain *bcptr, compile_data *cd)
{
  int repeat_type, op_type;
  int repeat_min = 0, repeat_max = 0;   /* To please picky compilers */
  int bravalue = 0;
  int greedy_default, greedy_non_default;
  int firstbyte, reqbyte;
  int zeroreqbyte, zerofirstbyte;
  int req_caseopt, reqvary, tempreqvary;
  int condcount = 0;
  int options = *optionsptr;
  int after_manual_callout = 0;
  register int c;
  register uschar *code = *codeptr;
  uschar *tempcode;
  BOOL inescq = FALSE;
  BOOL groupsetfirstbyte = FALSE;
  const uschar *ptr = *ptrptr;
  const uschar *tempptr;
  uschar *previous = NULL;
  uschar *previous_callout = NULL;
  uschar classbits[32];

  BOOL utf8 = FALSE;

/* Set up the default and non-default settings for greediness */

  greedy_default = ((options & PCRE_UNGREEDY) != 0);
  greedy_non_default = greedy_default ^ 1;

/* Initialize no first byte, no required byte. REQ_UNSET means "no char
matching encountered yet". It gets changed to REQ_NONE if we hit something that
matches a non-fixed char first char; reqbyte just remains unset if we never
find one.

When we hit a repeat whose minimum is zero, we may have to adjust these values
to take the zero repeat into account. This is implemented by setting them to
zerofirstbyte and zeroreqbyte when such a repeat is encountered. The individual
item types that can be repeated set these backoff variables appropriately. */

  firstbyte = reqbyte = zerofirstbyte = zeroreqbyte = REQ_UNSET;

/* The variable req_caseopt contains either the REQ_CASELESS value or zero,
according to the current setting of the caseless flag. REQ_CASELESS is a bit
value > 255. It is added into the firstbyte or reqbyte variables to record the
case status of the value. This is used only for ASCII characters. */

  req_caseopt = ((options & PCRE_CASELESS) != 0) ? REQ_CASELESS : 0;

/* Switch on next character until the end of the branch */

  for (;; ptr++) {
    BOOL negate_class;
    BOOL possessive_quantifier;
    BOOL is_quantifier;
    int class_charcount;
    int class_lastchar;
    int newoptions;
    int recno;
    int skipbytes;
    int subreqbyte;
    int subfirstbyte;
    int mclength;
    uschar mcbuffer[8];

    /* Next byte in the pattern */

    c = *ptr;

    /* If in \Q...\E, check for the end; if not, we have a literal */

    if (inescq && c != 0) {
      if (c == '\\' && ptr[1] == 'E') {
        inescq = FALSE;
        ptr++;
        continue;
      } else {
        if (previous_callout != NULL) {
          complete_callout(previous_callout, ptr, cd);
          previous_callout = NULL;
        }
        if ((options & PCRE_AUTO_CALLOUT) != 0) {
          previous_callout = code;
          code = auto_callout(code, ptr, cd);
        }
        goto NORMAL_CHAR;
      }
    }

    /* Fill in length of a previous callout, except when the next thing is
       a quantifier. */

    is_quantifier = c == '*' || c == '+' || c == '?' ||
      (c == '{' && is_counted_repeat(ptr + 1));

    if (!is_quantifier && previous_callout != NULL &&
        after_manual_callout-- <= 0) {
      complete_callout(previous_callout, ptr, cd);
      previous_callout = NULL;
    }

    /* In extended mode, skip white space and comments */

    if ((options & PCRE_EXTENDED) != 0) {
      if ((cd->ctypes[c] & ctype_space) != 0)
        continue;
      if (c == '#') {
        /* The space before the ; is to avoid a warning on a silly compiler
           on the Macintosh. */
        while ((c = *(++ptr)) != 0 && c != NEWLINE) ;
        if (c != 0)
          continue;             /* Else fall through to handle end of string */
      }
    }

    /* No auto callout for quantifiers. */

    if ((options & PCRE_AUTO_CALLOUT) != 0 && !is_quantifier) {
      previous_callout = code;
      code = auto_callout(code, ptr, cd);
    }

    switch (c) {
      /* The branch terminates at end of string, |, or ). */

    case 0:
    case '|':
    case ')':
      *firstbyteptr = firstbyte;
      *reqbyteptr = reqbyte;
      *codeptr = code;
      *ptrptr = ptr;
      return TRUE;

      /* Handle single-character metacharacters. In multiline mode, ^ disables
         the setting of any following char as a first character. */

    case '^':
      if ((options & PCRE_MULTILINE) != 0) {
        if (firstbyte == REQ_UNSET)
          firstbyte = REQ_NONE;
      }
      previous = NULL;
      *code++ = OP_CIRC;
      break;

    case '$':
      previous = NULL;
      *code++ = OP_DOLL;
      break;

      /* There can never be a first char if '.' is first, whatever happens about
         repeats. The value of reqbyte doesn't change either. */

    case '.':
      if (firstbyte == REQ_UNSET)
        firstbyte = REQ_NONE;
      zerofirstbyte = firstbyte;
      zeroreqbyte = reqbyte;
      previous = code;
      *code++ = OP_ANY;
      break;

      /* Character classes. If the included characters are all < 255 in value, we
         build a 32-byte bitmap of the permitted characters, except in the special
         case where there is only one such character. For negated classes, we build
         the map as usual, then invert it at the end. However, we use a different
         opcode so that data characters > 255 can be handled correctly.

         If the class contains characters outside the 0-255 range, a different
         opcode is compiled. It may optionally have a bit map for characters < 256,
         but those above are are explicitly listed afterwards. A flag byte tells
         whether the bitmap is present, and whether this is a negated class or not.
       */

    case '[':
      previous = code;

      /* PCRE supports POSIX class stuff inside a class. Perl gives an error if
         they are encountered at the top level, so we'll do that too. */

      if ((ptr[1] == ':' || ptr[1] == '.' || ptr[1] == '=') &&
          check_posix_syntax(ptr, &tempptr, cd)) {
        *errorcodeptr = (ptr[1] == ':') ? ERR13 : ERR31;
        goto FAILED;
      }

      /* If the first character is '^', set the negation flag and skip it. */

      if ((c = *(++ptr)) == '^') {
        negate_class = TRUE;
        c = *(++ptr);
      } else {
        negate_class = FALSE;
      }

      /* Keep a count of chars with values < 256 so that we can optimize the case
         of just a single character (as long as it's < 256). For higher valued UTF-8
         characters, we don't yet do any optimization. */

      class_charcount = 0;
      class_lastchar = -1;


      /* Initialize the 32-char bit map to all zeros. We have to build the
         map in a temporary bit of store, in case the class contains only 1
         character (< 256), because in that case the compiled code doesn't use the
         bit map. */

      memset(classbits, 0, 32 * sizeof(uschar));

      /* Process characters until ] is reached. By writing this as a "do" it
         means that an initial ] is taken as a data character. The first pass
         through the regex checked the overall syntax, so we don't need to be very
         strict here. At the start of the loop, c contains the first byte of the
         character. */

      do {

        /* Inside \Q...\E everything is literal except \E */

        if (inescq) {
          if (c == '\\' && ptr[1] == 'E') {
            inescq = FALSE;
            ptr++;
            continue;
          } else
            goto LONE_SINGLE_CHARACTER;
        }

        /* Handle POSIX class names. Perl allows a negation extension of the
           form [:^name:]. A square bracket that doesn't match the syntax is
           treated as a literal. We also recognize the POSIX constructions
           [.ch.] and [=ch=] ("collating elements") and fault them, as Perl
           5.6 and 5.8 do. */

        if (c == '[' &&
            (ptr[1] == ':' || ptr[1] == '.' || ptr[1] == '=') &&
            check_posix_syntax(ptr, &tempptr, cd)) {
          BOOL local_negate = FALSE;
          int posix_class, i;
          register const uschar *cbits = cd->cbits;

          if (ptr[1] != ':') {
            *errorcodeptr = ERR31;
            goto FAILED;
          }

          ptr += 2;
          if (*ptr == '^') {
            local_negate = TRUE;
            ptr++;
          }

          posix_class = check_posix_name(ptr, tempptr - ptr);
          if (posix_class < 0) {
            *errorcodeptr = ERR30;
            goto FAILED;
          }

          /* If matching is caseless, upper and lower are converted to
             alpha. This relies on the fact that the class table starts with
             alpha, lower, upper as the first 3 entries. */

          if ((options & PCRE_CASELESS) != 0 && posix_class <= 2)
            posix_class = 0;

          /* Or into the map we are building up to 3 of the static class
             tables, or their negations. The [:blank:] class sets up the same
             chars as the [:space:] class (all white space). We remove the vertical
             white space chars afterwards. */

          posix_class *= 3;
          for (i = 0; i < 3; i++) {
            BOOL blankclass = strncmp((char *) ptr, "blank", 5) == 0;
            int taboffset = posix_class_maps[posix_class + i];
            if (taboffset < 0)
              break;
            if (local_negate) {
              if (i == 0)
                for (c = 0; c < 32; c++)
                  classbits[c] |= ~cbits[c + taboffset];
              else
                for (c = 0; c < 32; c++)
                  classbits[c] &= ~cbits[c + taboffset];
              if (blankclass)
                classbits[1] |= 0x3c;
            } else {
              for (c = 0; c < 32; c++)
                classbits[c] |= cbits[c + taboffset];
              if (blankclass)
                classbits[1] &= ~0x3c;
            }
          }

          ptr = tempptr + 1;
          class_charcount = 10; /* Set > 1; assumes more than 1 per class */
          continue;             /* End of POSIX syntax handling */
        }

        /* Backslash may introduce a single character, or it may introduce one
           of the specials, which just set a flag. Escaped items are checked for
           validity in the pre-compiling pass. The sequence \b is a special case.
           Inside a class (and only there) it is treated as backspace. Elsewhere
           it marks a word boundary. Other escapes have preset maps ready to
           or into the one we are building. We assume they have more than one
           character in them, so set class_charcount bigger than one. */

        if (c == '\\') {
          c = check_escape(&ptr, errorcodeptr, *brackets, options, TRUE);

          if (-c == ESC_b)
            c = '\b';           /* \b is backslash in a class */
          else if (-c == ESC_X)
            c = 'X';            /* \X is literal X in a class */
          else if (-c == ESC_Q) {       /* Handle start of quoted string */
            if (ptr[1] == '\\' && ptr[2] == 'E') {
              ptr += 2;         /* avoid empty string */
            } else
              inescq = TRUE;
            continue;
          }

          if (c < 0) {
            register const uschar *cbits = cd->cbits;
            class_charcount += 2;       /* Greater than 1 is what matters */
            switch (-c) {
            case ESC_d:
              for (c = 0; c < 32; c++)
                classbits[c] |= cbits[c + cbit_digit];
              continue;

            case ESC_D:
              for (c = 0; c < 32; c++)
                classbits[c] |= ~cbits[c + cbit_digit];
              continue;

            case ESC_w:
              for (c = 0; c < 32; c++)
                classbits[c] |= cbits[c + cbit_word];
              continue;

            case ESC_W:
              for (c = 0; c < 32; c++)
                classbits[c] |= ~cbits[c + cbit_word];
              continue;

            case ESC_s:
              for (c = 0; c < 32; c++)
                classbits[c] |= cbits[c + cbit_space];
              classbits[1] &= ~0x08;    /* Perl 5.004 onwards omits VT from \s */
              continue;

            case ESC_S:
              for (c = 0; c < 32; c++)
                classbits[c] |= ~cbits[c + cbit_space];
              classbits[1] |= 0x08;     /* Perl 5.004 onwards omits VT from \s */
              continue;

#ifdef SUPPORT_UCP
            case ESC_p:
            case ESC_P:
              {
                BOOL negated;
                int property = get_ucp(&ptr, &negated, errorcodeptr);
                if (property < 0)
                  goto FAILED;
                class_utf8 = TRUE;
                *class_utf8data++ = ((-c == ESC_p) != negated) ?
                  XCL_PROP : XCL_NOTPROP;
                *class_utf8data++ = property;
                class_charcount -= 2;   /* Not a < 256 character */
              }
              continue;
#endif

              /* Unrecognized escapes are faulted if PCRE is running in its
                 strict mode. By default, for compatibility with Perl, they are
                 treated as literals. */

            default:
              if ((options & PCRE_EXTRA) != 0) {
                *errorcodeptr = ERR7;
                goto FAILED;
              }
              c = *ptr;         /* The final character */
              class_charcount -= 2;     /* Undo the default count from above */
            }
          }

          /* Fall through if we have a single character (c >= 0). This may be
             > 256 in UTF-8 mode. */

        }


        /* End of backslash handling */
        /* A single character may be followed by '-' to form a range. However,
           Perl does not permit ']' to be the end of the range. A '-' character
           here is treated as a literal. */
        if (ptr[1] == '-' && ptr[2] != ']') {
          int d;
          ptr += 2;

          d = *ptr;             /* Not UTF-8 mode */

          /* The second part of a range can be a single-character escape, but
             not any of the other escapes. Perl 5.6 treats a hyphen as a literal
             in such circumstances. */

          if (d == '\\') {
            const uschar *oldptr = ptr;
            d = check_escape(&ptr, errorcodeptr, *brackets, options, TRUE);

            /* \b is backslash; \X is literal X; any other special means the '-'
               was literal */

            if (d < 0) {
              if (d == -ESC_b)
                d = '\b';
              else if (d == -ESC_X)
                d = 'X';
              else {
                ptr = oldptr - 2;
                goto LONE_SINGLE_CHARACTER;     /* A few lines below */
              }
            }
          }

          /* The check that the two values are in the correct order happens in
             the pre-pass. Optimize one-character ranges */

          if (d == c)
            goto LONE_SINGLE_CHARACTER; /* A few lines below */

          /* In UTF-8 mode, if the upper limit is > 255, or > 127 for caseless
             matching, we have to use an XCLASS with extra data items. Caseless
             matching for characters > 127 is available only if UCP support is
             available. */


          /* We use the bit map for all cases when not in UTF-8 mode; else
             ranges that lie entirely within 0-127 when there is UCP support; else
             for partial ranges without UCP support. */

          for (; c <= d; c++) {
            classbits[c / 8] |= (1 << (c & 7));
            if ((options & PCRE_CASELESS) != 0) {
              int uc = cd->fcc[c];      /* flip case */
              classbits[uc / 8] |= (1 << (uc & 7));
            }
            class_charcount++;  /* in case a one-char range */
            class_lastchar = c;
          }

          continue;             /* Go get the next char in the class */
        }

        /* Handle a lone single character - we can get here for a normal
           non-escape char, or after \ that introduces a single character or for an
           apparent range that isn't. */

      LONE_SINGLE_CHARACTER:

        /* Handle a character that cannot go in the bit map */


        /* Handle a single-byte character */
        {
          classbits[c / 8] |= (1 << (c & 7));
          if ((options & PCRE_CASELESS) != 0) {
            c = cd->fcc[c];     /* flip case */
            classbits[c / 8] |= (1 << (c & 7));
          }
          class_charcount++;
          class_lastchar = c;
        }
      }

      /* Loop until ']' reached; the check for end of string happens inside the
         loop. This "while" is the end of the "do" above. */

      while ((c = *(++ptr)) != ']' || inescq);

      /* If class_charcount is 1, we saw precisely one character whose value is
         less than 256. In non-UTF-8 mode we can always optimize. In UTF-8 mode, we
         can optimize the negative case only if there were no characters >= 128
         because OP_NOT and the related opcodes like OP_NOTSTAR operate on
         single-bytes only. This is an historical hangover. Maybe one day we can
         tidy these opcodes to handle multi-byte characters.

         The optimization throws away the bit map. We turn the item into a
         1-character OP_CHAR[NC] if it's positive, or OP_NOT if it's negative. Note
         that OP_NOT does not support multibyte characters. In the positive case, it
         can cause firstbyte to be set. Otherwise, there can be no first char if
         this item is first, whatever repeat count may follow. In the case of
         reqbyte, save the previous value for reinstating. */

      if (class_charcount == 1) {
        zeroreqbyte = reqbyte;

        /* The OP_NOT opcode works on one-byte characters only. */

        if (negate_class) {
          if (firstbyte == REQ_UNSET)
            firstbyte = REQ_NONE;
          zerofirstbyte = firstbyte;
          *code++ = OP_NOT;
          *code++ = class_lastchar;
          break;
        }

        /* For a single, positive character, get the value into mcbuffer, and
           then we can handle this with the normal one-character code. */

        {
          mcbuffer[0] = class_lastchar;
          mclength = 1;
        }
        goto ONE_CHAR;
      }


      /* End of 1-char optimization */
      /* The general case - not the one-char optimization. If this is the first
         thing in the branch, there can be no first char setting, whatever the
         repeat count. Any reqbyte setting must remain unchanged after any kind of
         repeat. */
      if (firstbyte == REQ_UNSET)
        firstbyte = REQ_NONE;
      zerofirstbyte = firstbyte;
      zeroreqbyte = reqbyte;

      /* If there are characters with values > 255, we have to compile an
         extended class, with its own opcode. If there are no characters < 256,
         we can omit the bitmap. */


      /* If there are no characters > 255, negate the 32-byte map if necessary,
         and copy it into the code vector. If this is the first thing in the branch,
         there can be no first char setting, whatever the repeat count. Any reqbyte
         setting must remain unchanged after any kind of repeat. */

      if (negate_class) {
        *code++ = OP_NCLASS;
        for (c = 0; c < 32; c++)
          code[c] = ~classbits[c];
      } else {
        *code++ = OP_CLASS;
        memcpy(code, classbits, 32);
      }
      code += 32;
      break;

      /* Various kinds of repeat; '{' is not necessarily a quantifier, but this
         has been tested above. */

    case '{':
      if (!is_quantifier)
        goto NORMAL_CHAR;
      ptr = read_repeat_counts(ptr + 1, &repeat_min, &repeat_max, errorcodeptr);
      if (*errorcodeptr != 0)
        goto FAILED;
      goto REPEAT;

    case '*':
      repeat_min = 0;
      repeat_max = -1;
      goto REPEAT;

    case '+':
      repeat_min = 1;
      repeat_max = -1;
      goto REPEAT;

    case '?':
      repeat_min = 0;
      repeat_max = 1;

    REPEAT:
      if (previous == NULL) {
        *errorcodeptr = ERR9;
        goto FAILED;
      }

      if (repeat_min == 0) {
        firstbyte = zerofirstbyte;      /* Adjust for zero repeat */
        reqbyte = zeroreqbyte;  /* Ditto */
      }

      /* Remember whether this is a variable length repeat */

      reqvary = (repeat_min == repeat_max) ? 0 : REQ_VARY;

      op_type = 0;              /* Default single-char op codes */
      possessive_quantifier = FALSE;    /* Default not possessive quantifier */

      /* Save start of previous item, in case we have to move it up to make space
         for an inserted OP_ONCE for the additional '+' extension. */

      tempcode = previous;

      /* If the next character is '+', we have a possessive quantifier. This
         implies greediness, whatever the setting of the PCRE_UNGREEDY option.
         If the next character is '?' this is a minimizing repeat, by default,
         but if PCRE_UNGREEDY is set, it works the other way round. We change the
         repeat type to the non-default. */

      if (ptr[1] == '+') {
        repeat_type = 0;        /* Force greedy */
        possessive_quantifier = TRUE;
        ptr++;
      } else if (ptr[1] == '?') {
        repeat_type = greedy_non_default;
        ptr++;
      } else
        repeat_type = greedy_default;

      /* If previous was a recursion, we need to wrap it inside brackets so that
         it can be replicated if necessary. */

      if (*previous == OP_RECURSE) {
        memmove(previous + 1 + LINK_SIZE, previous, 1 + LINK_SIZE);
        code += 1 + LINK_SIZE;
        *previous = OP_BRA;
        PUT(previous, 1, code - previous);
        *code = OP_KET;
        PUT(code, 1, code - previous);
        code += 1 + LINK_SIZE;
      }

      /* If previous was a character match, abolish the item and generate a
         repeat item instead. If a char item has a minumum of more than one, ensure
         that it is set in reqbyte - it might not be if a sequence such as x{3} is
         the first thing in a branch because the x will have gone into firstbyte
         instead.  */

      if (*previous == OP_CHAR || *previous == OP_CHARNC) {
        /* Deal with UTF-8 characters that take up more than one byte. It's
           easier to write this out separately than try to macrify it. Use c to
           hold the length of the character in bytes, plus 0x80 to flag that it's a
           length rather than a small character. */


        /* Handle the case of a single byte - either with no UTF8 support, or
           with UTF-8 disabled, or for a UTF-8 character < 128. */

        {
          c = code[-1];
          if (repeat_min > 1)
            reqbyte = c | req_caseopt | cd->req_varyopt;
        }

        goto OUTPUT_SINGLE_REPEAT;      /* Code shared with single character types */
      }

      /* If previous was a single negated character ([^a] or similar), we use
         one of the special opcodes, replacing it. The code is shared with single-
         character repeats by setting opt_type to add a suitable offset into
         repeat_type. OP_NOT is currently used only for single-byte chars. */

      else if (*previous == OP_NOT) {
        op_type = OP_NOTSTAR - OP_STAR; /* Use "not" opcodes */
        c = previous[1];
        goto OUTPUT_SINGLE_REPEAT;
      }

      /* If previous was a character type match (\d or similar), abolish it and
         create a suitable repeat item. The code is shared with single-character
         repeats by setting op_type to add a suitable offset into repeat_type. Note
         the the Unicode property types will be present only when SUPPORT_UCP is
         defined, but we don't wrap the little bits of code here because it just
         makes it horribly messy. */

      else if (*previous < OP_EODN) {
        uschar *oldcode;
        int prop_type;
        op_type = OP_TYPESTAR - OP_STAR;        /* Use type opcodes */
        c = *previous;

      OUTPUT_SINGLE_REPEAT:
        prop_type = (*previous == OP_PROP || *previous == OP_NOTPROP) ?
          previous[1] : -1;

        oldcode = code;
        code = previous;        /* Usually overwrite previous item */

        /* If the maximum is zero then the minimum must also be zero; Perl allows
           this case, so we do too - by simply omitting the item altogether. */

        if (repeat_max == 0)
          goto END_REPEAT;

        /* All real repeats make it impossible to handle partial matching (maybe
           one day we will be able to remove this restriction). */

        if (repeat_max != 1)
          cd->nopartial = TRUE;

        /* Combine the op_type with the repeat_type */

        repeat_type += op_type;

        /* A minimum of zero is handled either as the special case * or ?, or as
           an UPTO, with the maximum given. */

        if (repeat_min == 0) {
          if (repeat_max == -1)
            *code++ = OP_STAR + repeat_type;
          else if (repeat_max == 1)
            *code++ = OP_QUERY + repeat_type;
          else {
            *code++ = OP_UPTO + repeat_type;
            PUT2INC(code, 0, repeat_max);
          }
        }

        /* A repeat minimum of 1 is optimized into some special cases. If the
           maximum is unlimited, we use OP_PLUS. Otherwise, the original item it
           left in place and, if the maximum is greater than 1, we use OP_UPTO with
           one less than the maximum. */

        else if (repeat_min == 1) {
          if (repeat_max == -1)
            *code++ = OP_PLUS + repeat_type;
          else {
            code = oldcode;     /* leave previous item in place */
            if (repeat_max == 1)
              goto END_REPEAT;
            *code++ = OP_UPTO + repeat_type;
            PUT2INC(code, 0, repeat_max - 1);
          }
        }

        /* The case {n,n} is just an EXACT, while the general case {n,m} is
           handled as an EXACT followed by an UPTO. */

        else {
          *code++ = OP_EXACT + op_type; /* NB EXACT doesn't have repeat_type */
          PUT2INC(code, 0, repeat_min);

          /* If the maximum is unlimited, insert an OP_STAR. Before doing so,
             we have to insert the character for the previous code. For a repeated
             Unicode property match, there is an extra byte that defines the
             required property. In UTF-8 mode, long characters have their length in
             c, with the 0x80 bit as a flag. */

          if (repeat_max < 0) {
            {
              *code++ = c;
              if (prop_type >= 0)
                *code++ = prop_type;
            }
            *code++ = OP_STAR + repeat_type;
          }

          /* Else insert an UPTO if the max is greater than the min, again
             preceded by the character, for the previously inserted code. */

          else if (repeat_max != repeat_min) {
            *code++ = c;
            if (prop_type >= 0)
              *code++ = prop_type;
            repeat_max -= repeat_min;
            *code++ = OP_UPTO + repeat_type;
            PUT2INC(code, 0, repeat_max);
          }
        }

        /* The character or character type itself comes last in all cases. */

        *code++ = c;

        /* For a repeated Unicode property match, there is an extra byte that
           defines the required property. */

#ifdef SUPPORT_UCP
        if (prop_type >= 0)
          *code++ = prop_type;
#endif
      }

      /* If previous was a character class or a back reference, we put the repeat
         stuff after it, but just skip the item if the repeat was {0,0}. */

      else if (*previous == OP_CLASS ||
               *previous == OP_NCLASS || *previous == OP_REF) {
        if (repeat_max == 0) {
          code = previous;
          goto END_REPEAT;
        }

        /* All real repeats make it impossible to handle partial matching (maybe
           one day we will be able to remove this restriction). */

        if (repeat_max != 1)
          cd->nopartial = TRUE;

        if (repeat_min == 0 && repeat_max == -1)
          *code++ = OP_CRSTAR + repeat_type;
        else if (repeat_min == 1 && repeat_max == -1)
          *code++ = OP_CRPLUS + repeat_type;
        else if (repeat_min == 0 && repeat_max == 1)
          *code++ = OP_CRQUERY + repeat_type;
        else {
          *code++ = OP_CRRANGE + repeat_type;
          PUT2INC(code, 0, repeat_min);
          if (repeat_max == -1)
            repeat_max = 0;     /* 2-byte encoding for max */
          PUT2INC(code, 0, repeat_max);
        }
      }

      /* If previous was a bracket group, we may have to replicate it in certain
         cases. */

      else if (*previous >= OP_BRA || *previous == OP_ONCE ||
               *previous == OP_COND) {
        register int i;
        int ketoffset = 0;
        int len = code - previous;
        uschar *bralink = NULL;

        /* If the maximum repeat count is unlimited, find the end of the bracket
           by scanning through from the start, and compute the offset back to it
           from the current code pointer. There may be an OP_OPT setting following
           the final KET, so we can't find the end just by going back from the code
           pointer. */

        if (repeat_max == -1) {
          register uschar *ket = previous;
          do
            ket += GET(ket, 1);
          while (*ket != OP_KET);
          ketoffset = code - ket;
        }

        /* The case of a zero minimum is special because of the need to stick
           OP_BRAZERO in front of it, and because the group appears once in the
           data, whereas in other cases it appears the minimum number of times. For
           this reason, it is simplest to treat this case separately, as otherwise
           the code gets far too messy. There are several special subcases when the
           minimum is zero. */

        if (repeat_min == 0) {
          /* If the maximum is also zero, we just omit the group from the output
             altogether. */

          if (repeat_max == 0) {
            code = previous;
            goto END_REPEAT;
          }

          /* If the maximum is 1 or unlimited, we just have to stick in the
             BRAZERO and do no more at this point. However, we do need to adjust
             any OP_RECURSE calls inside the group that refer to the group itself or
             any internal group, because the offset is from the start of the whole
             regex. Temporarily terminate the pattern while doing this. */

          if (repeat_max <= 1) {
            *code = OP_END;
            adjust_recurse(previous, 1, utf8, cd);
            memmove(previous + 1, previous, len);
            code++;
            *previous++ = OP_BRAZERO + repeat_type;
          }

          /* If the maximum is greater than 1 and limited, we have to replicate
             in a nested fashion, sticking OP_BRAZERO before each set of brackets.
             The first one has to be handled carefully because it's the original
             copy, which has to be moved up. The remainder can be handled by code
             that is common with the non-zero minimum case below. We have to
             adjust the value or repeat_max, since one less copy is required. Once
             again, we may have to adjust any OP_RECURSE calls inside the group. */

          else {
            int offset;
            *code = OP_END;
            adjust_recurse(previous, 2 + LINK_SIZE, utf8, cd);
            memmove(previous + 2 + LINK_SIZE, previous, len);
            code += 2 + LINK_SIZE;
            *previous++ = OP_BRAZERO + repeat_type;
            *previous++ = OP_BRA;

            /* We chain together the bracket offset fields that have to be
               filled in later when the ends of the brackets are reached. */

            offset = (bralink == NULL) ? 0 : previous - bralink;
            bralink = previous;
            PUTINC(previous, 0, offset);
          }

          repeat_max--;
        }

        /* If the minimum is greater than zero, replicate the group as many
           times as necessary, and adjust the maximum to the number of subsequent
           copies that we need. If we set a first char from the group, and didn't
           set a required char, copy the latter from the former. */

        else {
          if (repeat_min > 1) {
            if (groupsetfirstbyte && reqbyte < 0)
              reqbyte = firstbyte;
            for (i = 1; i < repeat_min; i++) {
              memcpy(code, previous, len);
              code += len;
            }
          }
          if (repeat_max > 0)
            repeat_max -= repeat_min;
        }

        /* This code is common to both the zero and non-zero minimum cases. If
           the maximum is limited, it replicates the group in a nested fashion,
           remembering the bracket starts on a stack. In the case of a zero minimum,
           the first one was set up above. In all cases the repeat_max now specifies
           the number of additional copies needed. */

        if (repeat_max >= 0) {
          for (i = repeat_max - 1; i >= 0; i--) {
            *code++ = OP_BRAZERO + repeat_type;

            /* All but the final copy start a new nesting, maintaining the
               chain of brackets outstanding. */

            if (i != 0) {
              int offset;
              *code++ = OP_BRA;
              offset = (bralink == NULL) ? 0 : code - bralink;
              bralink = code;
              PUTINC(code, 0, offset);
            }

            memcpy(code, previous, len);
            code += len;
          }

          /* Now chain through the pending brackets, and fill in their length
             fields (which are holding the chain links pro tem). */

          while (bralink != NULL) {
            int oldlinkoffset;
            int offset = code - bralink + 1;
            uschar *bra = code - offset;
            oldlinkoffset = GET(bra, 1);
            bralink = (oldlinkoffset == 0) ? NULL : bralink - oldlinkoffset;
            *code++ = OP_KET;
            PUTINC(code, 0, offset);
            PUT(bra, 1, offset);
          }
        }

        /* If the maximum is unlimited, set a repeater in the final copy. We
           can't just offset backwards from the current code point, because we
           don't know if there's been an options resetting after the ket. The
           correct offset was computed above. */

        else
          code[-ketoffset] = OP_KETRMAX + repeat_type;
      }

      /* Else there's some kind of shambles */

      else {
        *errorcodeptr = ERR11;
        goto FAILED;
      }

      /* If the character following a repeat is '+', we wrap the entire repeated
         item inside OP_ONCE brackets. This is just syntactic sugar, taken from
         Sun's Java package. The repeated item starts at tempcode, not at previous,
         which might be the first part of a string whose (former) last char we
         repeated. However, we don't support '+' after a greediness '?'. */

      if (possessive_quantifier) {
        int len = code - tempcode;
        memmove(tempcode + 1 + LINK_SIZE, tempcode, len);
        code += 1 + LINK_SIZE;
        len += 1 + LINK_SIZE;
        tempcode[0] = OP_ONCE;
        *code++ = OP_KET;
        PUTINC(code, 0, len);
        PUT(tempcode, 1, len);
      }

      /* In all case we no longer have a previous item. We also set the
         "follows varying string" flag for subsequently encountered reqbytes if
         it isn't already set and we have just passed a varying length item. */

    END_REPEAT:
      previous = NULL;
      cd->req_varyopt |= reqvary;
      break;


      /* Start of nested bracket sub-expression, or comment or lookahead or
         lookbehind or option setting or condition. First deal with special things
         that can come after a bracket; all are introduced by ?, and the appearance
         of any of them means that this is not a referencing group. They were
         checked for validity in the first pass over the string, so we don't have to
         check for syntax errors here.  */

    case '(':
      newoptions = options;
      skipbytes = 0;

      if (*(++ptr) == '?') {
        int set, unset;
        int *optset;

        switch (*(++ptr)) {
        case '#':              /* Comment; skip to ket */
          ptr++;
          while (*ptr != ')')
            ptr++;
          continue;

        case ':':              /* Non-extracting bracket */
          bravalue = OP_BRA;
          ptr++;
          break;

        case '(':
          bravalue = OP_COND;   /* Conditional group */

          /* Condition to test for recursion */

          if (ptr[1] == 'R') {
            code[1 + LINK_SIZE] = OP_CREF;
            PUT2(code, 2 + LINK_SIZE, CREF_RECURSE);
            skipbytes = 3;
            ptr += 3;
          }

          /* Condition to test for a numbered subpattern match. We know that
             if a digit follows ( then there will just be digits until ) because
             the syntax was checked in the first pass. */

          else if ((digitab[ptr[1]] && ctype_digit) != 0) {
            int condref;        /* Don't amalgamate; some compilers */
            condref = *(++ptr) - '0';   /* grumble at autoincrement in declaration */
            while (*(++ptr) != ')')
              condref = condref * 10 + *ptr - '0';
            if (condref == 0) {
              *errorcodeptr = ERR35;
              goto FAILED;
            }
            ptr++;
            code[1 + LINK_SIZE] = OP_CREF;
            PUT2(code, 2 + LINK_SIZE, condref);
            skipbytes = 3;
          }
          /* For conditions that are assertions, we just fall through, having
             set bravalue above. */
          break;

        case '=':              /* Positive lookahead */
          bravalue = OP_ASSERT;
          ptr++;
          break;

        case '!':              /* Negative lookahead */
          bravalue = OP_ASSERT_NOT;
          ptr++;
          break;

        case '<':              /* Lookbehinds */
          switch (*(++ptr)) {
          case '=':            /* Positive lookbehind */
            bravalue = OP_ASSERTBACK;
            ptr++;
            break;

          case '!':            /* Negative lookbehind */
            bravalue = OP_ASSERTBACK_NOT;
            ptr++;
            break;
          }
          break;

        case '>':              /* One-time brackets */
          bravalue = OP_ONCE;
          ptr++;
          break;

        case 'C':              /* Callout - may be followed by digits; */
          previous_callout = code;      /* Save for later completion */
          after_manual_callout = 1;     /* Skip one item before completing */
          *code++ = OP_CALLOUT; /* Already checked that the terminating */
          {                     /* closing parenthesis is present. */
            int n = 0;
            while ((digitab[*(++ptr)] & ctype_digit) != 0)
              n = n * 10 + *ptr - '0';
            if (n > 255) {
              *errorcodeptr = ERR38;
              goto FAILED;
            }
            *code++ = n;
            PUT(code, 0, ptr - cd->start_pattern + 1);  /* Pattern offset */
            PUT(code, LINK_SIZE, 0);    /* Default length */
            code += 2 * LINK_SIZE;
          }
          previous = NULL;
          continue;

        case 'P':              /* Named subpattern handling */
          if (*(++ptr) == '<') {        /* Definition */
            int i, namelen;
            uschar *slot = cd->name_table;
            const uschar *name; /* Don't amalgamate; some compilers */
            name = ++ptr;       /* grumble at autoincrement in declaration */

            while (*ptr++ != '>') ;
            namelen = ptr - name - 1;

            for (i = 0; i < cd->names_found; i++) {
              int crc = memcmp(name, slot + 2, namelen);
              if (crc == 0) {
                if (slot[2 + namelen] == 0) {
                  *errorcodeptr = ERR43;
                  goto FAILED;
                }
                crc = -1;       /* Current name is substring */
              }
              if (crc < 0) {
                memmove(slot + cd->name_entry_size, slot,
                        (cd->names_found - i) * cd->name_entry_size);
                break;
              }
              slot += cd->name_entry_size;
            }

            PUT2(slot, 0, *brackets + 1);
            memcpy(slot + 2, name, namelen);
            slot[2 + namelen] = 0;
            cd->names_found++;
            goto NUMBERED_GROUP;
          }

          if (*ptr == '=' || *ptr == '>') {     /* Reference or recursion */
            int i, namelen;
            int type = *ptr++;
            const uschar *name = ptr;
            uschar *slot = cd->name_table;

            while (*ptr != ')')
              ptr++;
            namelen = ptr - name;

            for (i = 0; i < cd->names_found; i++) {
              if (strncmp((char *) name, (char *) slot + 2, namelen) == 0)
                break;
              slot += cd->name_entry_size;
            }
            if (i >= cd->names_found) {
              *errorcodeptr = ERR15;
              goto FAILED;
            }

            recno = GET2(slot, 0);

            if (type == '>')
              goto HANDLE_RECURSION;    /* A few lines below */

            /* Back reference */

            previous = code;
            *code++ = OP_REF;
            PUT2INC(code, 0, recno);
            cd->backref_map |= (recno < 32) ? (1 << recno) : 1;
            if (recno > cd->top_backref)
              cd->top_backref = recno;
            continue;
          }

          /* Should never happen */
          break;

        case 'R':              /* Pattern recursion */
          ptr++;                /* Same as (?0)      */
          /* Fall through */

          /* Recursion or "subroutine" call */

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          {
            const uschar *called;
            recno = 0;
            while ((digitab[*ptr] & ctype_digit) != 0)
              recno = recno * 10 + *ptr++ - '0';

            /* Come here from code above that handles a named recursion */

          HANDLE_RECURSION:

            previous = code;

            /* Find the bracket that is being referenced. Temporarily end the
               regex in case it doesn't exist. */

            *code = OP_END;
            called = (recno == 0) ?
              cd->start_code : find_bracket(cd->start_code, utf8, recno);

            if (called == NULL) {
              *errorcodeptr = ERR15;
              goto FAILED;
            }

            /* If the subpattern is still open, this is a recursive call. We
               check to see if this is a left recursion that could loop for ever,
               and diagnose that case. */

            if (GET(called, 1) == 0
                && could_be_empty(called, code, bcptr, utf8)) {
              *errorcodeptr = ERR40;
              goto FAILED;
            }

            /* Insert the recursion/subroutine item */

            *code = OP_RECURSE;
            PUT(code, 1, called - cd->start_code);
            code += 1 + LINK_SIZE;
          }
          continue;

          /* Character after (? not specially recognized */

        default:               /* Option setting */
          set = unset = 0;
          optset = &set;

          while (*ptr != ')' && *ptr != ':') {
            switch (*ptr++) {
            case '-':
              optset = &unset;
              break;

            case 'i':
              *optset |= PCRE_CASELESS;
              break;
            case 'm':
              *optset |= PCRE_MULTILINE;
              break;
            case 's':
              *optset |= PCRE_DOTALL;
              break;
            case 'x':
              *optset |= PCRE_EXTENDED;
              break;
            case 'U':
              *optset |= PCRE_UNGREEDY;
              break;
            case 'X':
              *optset |= PCRE_EXTRA;
              break;
            }
          }

          /* Set up the changed option bits, but don't change anything yet. */

          newoptions = (options | set) & (~unset);

          /* If the options ended with ')' this is not the start of a nested
             group with option changes, so the options change at this level. Compile
             code to change the ims options if this setting actually changes any of
             them. We also pass the new setting back so that it can be put at the
             start of any following branches, and when this group ends (if we are in
             a group), a resetting item can be compiled.

             Note that if this item is right at the start of the pattern, the
             options will have been abstracted and made global, so there will be no
             change to compile. */

          if (*ptr == ')') {
            if ((options & PCRE_IMS) != (newoptions & PCRE_IMS)) {
              *code++ = OP_OPT;
              *code++ = newoptions & PCRE_IMS;
            }

            /* Change options at this level, and pass them back for use
               in subsequent branches. Reset the greedy defaults and the case
               value for firstbyte and reqbyte. */

            *optionsptr = options = newoptions;
            greedy_default = ((newoptions & PCRE_UNGREEDY) != 0);
            greedy_non_default = greedy_default ^ 1;
            req_caseopt = ((options & PCRE_CASELESS) != 0) ? REQ_CASELESS : 0;

            previous = NULL;    /* This item can't be repeated */
            continue;           /* It is complete */
          }

          /* If the options ended with ':' we are heading into a nested group
             with possible change of options. Such groups are non-capturing and are
             not assertions of any kind. All we need to do is skip over the ':';
             the newoptions value is handled below. */

          bravalue = OP_BRA;
          ptr++;
        }
      }

      /* If PCRE_NO_AUTO_CAPTURE is set, all unadorned brackets become
         non-capturing and behave like (?:...) brackets */

      else if ((options & PCRE_NO_AUTO_CAPTURE) != 0) {
        bravalue = OP_BRA;
      }

      /* Else we have a referencing group; adjust the opcode. If the bracket
         number is greater than EXTRACT_BASIC_MAX, we set the opcode one higher, and
         arrange for the true number to follow later, in an OP_BRANUMBER item. */

      else {
      NUMBERED_GROUP:
        if (++(*brackets) > EXTRACT_BASIC_MAX) {
          bravalue = OP_BRA + EXTRACT_BASIC_MAX + 1;
          code[1 + LINK_SIZE] = OP_BRANUMBER;
          PUT2(code, 2 + LINK_SIZE, *brackets);
          skipbytes = 3;
        } else
          bravalue = OP_BRA + *brackets;
      }

      /* Process nested bracketed re. Assertions may not be repeated, but other
         kinds can be. We copy code into a non-register variable in order to be able
         to pass its address because some compilers complain otherwise. Pass in a
         new setting for the ims options if they have changed. */

      previous = (bravalue >= OP_ONCE) ? code : NULL;
      *code = bravalue;
      tempcode = code;
      tempreqvary = cd->req_varyopt;    /* Save value before bracket */

      if (!compile_regex(newoptions,    /* The complete new option state */
                         options & PCRE_IMS,    /* The previous ims option state */
                         brackets,      /* Extracting bracket count */
                         &tempcode,     /* Where to put code (updated) */
                         &ptr,  /* Input pointer (updated) */
                         errorcodeptr,  /* Where to put an error message */
                         (bravalue == OP_ASSERTBACK || bravalue == OP_ASSERTBACK_NOT),  /* TRUE if back assert */
                         skipbytes,     /* Skip over OP_COND/OP_BRANUMBER */
                         &subfirstbyte, /* For possible first char */
                         &subreqbyte,   /* For possible last char */
                         bcptr, /* Current branch chain */
                         cd))   /* Tables block */
        goto FAILED;

      /* At the end of compiling, code is still pointing to the start of the
         group, while tempcode has been updated to point past the end of the group
         and any option resetting that may follow it. The pattern pointer (ptr)
         is on the bracket. */

      /* If this is a conditional bracket, check that there are no more than
         two branches in the group. */

      else if (bravalue == OP_COND) {
        uschar *tc = code;
        condcount = 0;

        do {
          condcount++;
          tc += GET(tc, 1);
        }
        while (*tc != OP_KET);

        if (condcount > 2) {
          *errorcodeptr = ERR27;
          goto FAILED;
        }

        /* If there is just one branch, we must not make use of its firstbyte or
           reqbyte, because this is equivalent to an empty second branch. */

        if (condcount == 1)
          subfirstbyte = subreqbyte = REQ_NONE;
      }

      /* Handle updating of the required and first characters. Update for normal
         brackets of all kinds, and conditions with two branches (see code above).
         If the bracket is followed by a quantifier with zero repeat, we have to
         back off. Hence the definition of zeroreqbyte and zerofirstbyte outside the
         main loop so that they can be accessed for the back off. */

      zeroreqbyte = reqbyte;
      zerofirstbyte = firstbyte;
      groupsetfirstbyte = FALSE;

      if (bravalue >= OP_BRA || bravalue == OP_ONCE || bravalue == OP_COND) {
        /* If we have not yet set a firstbyte in this branch, take it from the
           subpattern, remembering that it was set here so that a repeat of more
           than one can replicate it as reqbyte if necessary. If the subpattern has
           no firstbyte, set "none" for the whole branch. In both cases, a zero
           repeat forces firstbyte to "none". */

        if (firstbyte == REQ_UNSET) {
          if (subfirstbyte >= 0) {
            firstbyte = subfirstbyte;
            groupsetfirstbyte = TRUE;
          } else
            firstbyte = REQ_NONE;
          zerofirstbyte = REQ_NONE;
        }

        /* If firstbyte was previously set, convert the subpattern's firstbyte
           into reqbyte if there wasn't one, using the vary flag that was in
           existence beforehand. */

        else if (subfirstbyte >= 0 && subreqbyte < 0)
          subreqbyte = subfirstbyte | tempreqvary;

        /* If the subpattern set a required byte (or set a first byte that isn't
           really the first byte - see above), set it. */

        if (subreqbyte >= 0)
          reqbyte = subreqbyte;
      }

      /* For a forward assertion, we take the reqbyte, if set. This can be
         helpful if the pattern that follows the assertion doesn't set a different
         char. For example, it's useful for /(?=abcde).+/. We can't set firstbyte
         for an assertion, however because it leads to incorrect effect for patterns
         such as /(?=a)a.+/ when the "real" "a" would then become a reqbyte instead
         of a firstbyte. This is overcome by a scan at the end if there's no
         firstbyte, looking for an asserted first char. */

      else if (bravalue == OP_ASSERT && subreqbyte >= 0)
        reqbyte = subreqbyte;

      /* Now update the main code pointer to the end of the group. */

      code = tempcode;

      /* Error if hit end of pattern */

      if (*ptr != ')') {
        *errorcodeptr = ERR14;
        goto FAILED;
      }
      break;

      /* Check \ for being a real metacharacter; if not, fall through and handle
         it as a data character at the start of a string. Escape items are checked
         for validity in the pre-compiling pass. */

    case '\\':
      tempptr = ptr;
      c = check_escape(&ptr, errorcodeptr, *brackets, options, FALSE);

      /* Handle metacharacters introduced by \. For ones like \d, the ESC_ values
         are arranged to be the negation of the corresponding OP_values. For the
         back references, the values are ESC_REF plus the reference number. Only
         back references and those types that consume a character may be repeated.
         We can test for values between ESC_b and ESC_Z for the latter; this may
         have to change if any new ones are ever created. */

      if (c < 0) {
        if (-c == ESC_Q) {      /* Handle start of quoted string */
          if (ptr[1] == '\\' && ptr[2] == 'E')
            ptr += 2;           /* avoid empty string */
          else
            inescq = TRUE;
          continue;
        }

        /* For metasequences that actually match a character, we disable the
           setting of a first character if it hasn't already been set. */

        if (firstbyte == REQ_UNSET && -c > ESC_b && -c < ESC_Z)
          firstbyte = REQ_NONE;

        /* Set values to reset to if this is followed by a zero repeat. */

        zerofirstbyte = firstbyte;
        zeroreqbyte = reqbyte;

        /* Back references are handled specially */

        if (-c >= ESC_REF) {
          int number = -c - ESC_REF;
          previous = code;
          *code++ = OP_REF;
          PUT2INC(code, 0, number);
        }

        /* So are Unicode property matches, if supported. We know that get_ucp
           won't fail because it was tested in the pre-pass. */

#ifdef SUPPORT_UCP
        else if (-c == ESC_P || -c == ESC_p) {
          BOOL negated;
          int value = get_ucp(&ptr, &negated, errorcodeptr);
          previous = code;
          *code++ = ((-c == ESC_p) != negated) ? OP_PROP : OP_NOTPROP;
          *code++ = value;
        }
#endif

        /* For the rest, we can obtain the OP value by negating the escape
           value */

        else {
          previous = (-c > ESC_b && -c < ESC_Z) ? code : NULL;
          *code++ = -c;
        }
        continue;
      }

      /* We have a data character whose value is in c. In UTF-8 mode it may have
         a value > 127. We set its representation in the length/buffer, and then
         handle it as a data character. */


      {
        mcbuffer[0] = c;
        mclength = 1;
      }

      goto ONE_CHAR;

      /* Handle a literal character. It is guaranteed not to be whitespace or #
         when the extended flag is set. If we are in UTF-8 mode, it may be a
         multi-byte literal character. */

    default:
    NORMAL_CHAR:
      mclength = 1;
      mcbuffer[0] = c;


      /* At this point we have the character's bytes in mcbuffer, and the length
         in mclength. When not in UTF-8 mode, the length is always 1. */

    ONE_CHAR:
      previous = code;
      *code++ = ((options & PCRE_CASELESS) != 0) ? OP_CHARNC : OP_CHAR;
      for (c = 0; c < mclength; c++)
        *code++ = mcbuffer[c];

      /* Set the first and required bytes appropriately. If no previous first
         byte, set it from this character, but revert to none on a zero repeat.
         Otherwise, leave the firstbyte value alone, and don't change it on a zero
         repeat. */

      if (firstbyte == REQ_UNSET) {
        zerofirstbyte = REQ_NONE;
        zeroreqbyte = reqbyte;

        /* If the character is more than one byte long, we can set firstbyte
           only if it is not to be matched caselessly. */

        if (mclength == 1 || req_caseopt == 0) {
          firstbyte = mcbuffer[0] | req_caseopt;
          if (mclength != 1)
            reqbyte = code[-1] | cd->req_varyopt;
        } else
          firstbyte = reqbyte = REQ_NONE;
      }

      /* firstbyte was previously set; we can set reqbyte only the length is
         1 or the matching is caseful. */

      else {
        zerofirstbyte = firstbyte;
        zeroreqbyte = reqbyte;
        if (mclength == 1 || req_caseopt == 0)
          reqbyte = code[-1] | req_caseopt | cd->req_varyopt;
      }

      break;                    /* End of literal character handling */
    }
  }                             /* end of big loop */

/* Control never reaches here by falling through, only by a goto for all the
error states. Pass back the position in the pattern so that it can be displayed
to the user for diagnosing the error. */

FAILED:
  *ptrptr = ptr;
  return FALSE;
}




/*************************************************
*     Compile sequence of alternatives           *
*************************************************/

/* On entry, ptr is pointing past the bracket character, but on return
it points to the closing bracket, or vertical bar, or end of string.
The code variable is pointing at the byte into which the BRA operator has been
stored. If the ims options are changed at the start (for a (?ims: group) or
during any branch, we need to insert an OP_OPT item at the start of every
following branch to ensure they get set correctly at run time, and also pass
the new options into every subsequent branch compile.

Argument:
  options        option bits, including any changes for this subpattern
  oldims         previous settings of ims option bits
  brackets       -> int containing the number of extracting brackets used
  codeptr        -> the address of the current code pointer
  ptrptr         -> the address of the current pattern pointer
  errorcodeptr   -> pointer to error code variable
  lookbehind     TRUE if this is a lookbehind assertion
  skipbytes      skip this many bytes at start (for OP_COND, OP_BRANUMBER)
  firstbyteptr   place to put the first required character, or a negative number
  reqbyteptr     place to put the last required character, or a negative number
  bcptr          pointer to the chain of currently open branches
  cd             points to the data block with tables pointers etc.

Returns:      TRUE on success
*/

static BOOL
compile_regex(int options, int oldims, int *brackets, uschar **codeptr,
              const uschar **ptrptr, int *errorcodeptr, BOOL lookbehind,
              int skipbytes, int *firstbyteptr, int *reqbyteptr,
              branch_chain *bcptr, compile_data *cd)
{
  const uschar *ptr = *ptrptr;
  uschar *code = *codeptr;
  uschar *last_branch = code;
  uschar *start_bracket = code;
  uschar *reverse_count = NULL;
  int firstbyte, reqbyte;
  int branchfirstbyte, branchreqbyte;
  branch_chain bc;

  bc.outer = bcptr;
  bc.current = code;

  firstbyte = reqbyte = REQ_UNSET;

/* Offset is set zero to mark that this bracket is still open */

  PUT(code, 1, 0);
  code += 1 + LINK_SIZE + skipbytes;

/* Loop for each alternative branch */

  for (;;) {
    /* Handle a change of ims options at the start of the branch */

    if ((options & PCRE_IMS) != oldims) {
      *code++ = OP_OPT;
      *code++ = options & PCRE_IMS;
    }

    /* Set up dummy OP_REVERSE if lookbehind assertion */

    if (lookbehind) {
      *code++ = OP_REVERSE;
      reverse_count = code;
      PUTINC(code, 0, 0);
    }

    /* Now compile the branch */

    if (!compile_branch(&options, brackets, &code, &ptr, errorcodeptr,
                        &branchfirstbyte, &branchreqbyte, &bc, cd)) {
      *ptrptr = ptr;
      return FALSE;
    }

    /* If this is the first branch, the firstbyte and reqbyte values for the
       branch become the values for the regex. */

    if (*last_branch != OP_ALT) {
      firstbyte = branchfirstbyte;
      reqbyte = branchreqbyte;
    }

    /* If this is not the first branch, the first char and reqbyte have to
       match the values from all the previous branches, except that if the previous
       value for reqbyte didn't have REQ_VARY set, it can still match, and we set
       REQ_VARY for the regex. */

    else {
      /* If we previously had a firstbyte, but it doesn't match the new branch,
         we have to abandon the firstbyte for the regex, but if there was previously
         no reqbyte, it takes on the value of the old firstbyte. */

      if (firstbyte >= 0 && firstbyte != branchfirstbyte) {
        if (reqbyte < 0)
          reqbyte = firstbyte;
        firstbyte = REQ_NONE;
      }

      /* If we (now or from before) have no firstbyte, a firstbyte from the
         branch becomes a reqbyte if there isn't a branch reqbyte. */

      if (firstbyte < 0 && branchfirstbyte >= 0 && branchreqbyte < 0)
        branchreqbyte = branchfirstbyte;

      /* Now ensure that the reqbytes match */

      if ((reqbyte & ~REQ_VARY) != (branchreqbyte & ~REQ_VARY))
        reqbyte = REQ_NONE;
      else
        reqbyte |= branchreqbyte;       /* To "or" REQ_VARY */
    }

    /* If lookbehind, check that this branch matches a fixed-length string,
       and put the length into the OP_REVERSE item. Temporarily mark the end of
       the branch with OP_END. */

    if (lookbehind) {
      int length;
      *code = OP_END;
      length = find_fixedlength(last_branch, options);
      if (length < 0) {
        *errorcodeptr = (length == -2) ? ERR36 : ERR25;
        *ptrptr = ptr;
        return FALSE;
      }
      PUT(reverse_count, 0, length);
    }

    /* Reached end of expression, either ')' or end of pattern. Go back through
       the alternative branches and reverse the chain of offsets, with the field in
       the BRA item now becoming an offset to the first alternative. If there are
       no alternatives, it points to the end of the group. The length in the
       terminating ket is always the length of the whole bracketed item. If any of
       the ims options were changed inside the group, compile a resetting op-code
       following, except at the very end of the pattern. Return leaving the pointer
       at the terminating char. */

    if (*ptr != '|') {
      int length = code - last_branch;
      do {
        int prev_length = GET(last_branch, 1);
        PUT(last_branch, 1, length);
        length = prev_length;
        last_branch -= length;
      }
      while (length > 0);

      /* Fill in the ket */

      *code = OP_KET;
      PUT(code, 1, code - start_bracket);
      code += 1 + LINK_SIZE;

      /* Resetting option if needed */

      if ((options & PCRE_IMS) != oldims && *ptr == ')') {
        *code++ = OP_OPT;
        *code++ = oldims;
      }

      /* Set values to pass back */

      *codeptr = code;
      *ptrptr = ptr;
      *firstbyteptr = firstbyte;
      *reqbyteptr = reqbyte;
      return TRUE;
    }

    /* Another branch follows; insert an "or" node. Its length field points back
       to the previous branch while the bracket remains open. At the end the chain
       is reversed. It's done like this so that the start of the bracket has a
       zero offset until it is closed, making it possible to detect recursion. */

    *code = OP_ALT;
    PUT(code, 1, code - last_branch);
    bc.current = last_branch = code;
    code += 1 + LINK_SIZE;
    ptr++;
  }
/* Control never reaches here */
}




/*************************************************
*          Check for anchored expression         *
*************************************************/

/* Try to find out if this is an anchored regular expression. Consider each
alternative branch. If they all start with OP_SOD or OP_CIRC, or with a bracket
all of whose alternatives start with OP_SOD or OP_CIRC (recurse ad lib), then
it's anchored. However, if this is a multiline pattern, then only OP_SOD
counts, since OP_CIRC can match in the middle.

We can also consider a regex to be anchored if OP_SOM starts all its branches.
This is the code for \G, which means "match at start of match position, taking
into account the match offset".

A branch is also implicitly anchored if it starts with .* and DOTALL is set,
because that will try the rest of the pattern at all possible matching points,
so there is no point trying again.... er ....

.... except when the .* appears inside capturing parentheses, and there is a
subsequent back reference to those parentheses. We haven't enough information
to catch that case precisely.

At first, the best we could do was to detect when .* was in capturing brackets
and the highest back reference was greater than or equal to that level.
However, by keeping a bitmap of the first 31 back references, we can catch some
of the more common cases more precisely.

Arguments:
  code           points to start of expression (the bracket)
  options        points to the options setting
  bracket_map    a bitmap of which brackets we are inside while testing; this
                  handles up to substring 31; after that we just have to take
                  the less precise approach
  backref_map    the back reference bitmap

Returns:     TRUE or FALSE
*/

static BOOL
is_anchored(register const uschar *code, int *options,
            unsigned int bracket_map, unsigned int backref_map)
{
  do {
    const uschar *scode =
      first_significant_code(code + 1 + LINK_SIZE, options, PCRE_MULTILINE,
                             FALSE);
    register int op = *scode;

    /* Capturing brackets */

    if (op > OP_BRA) {
      int new_map;
      op -= OP_BRA;
      if (op > EXTRACT_BASIC_MAX)
        op = GET2(scode, 2 + LINK_SIZE);
      new_map = bracket_map | ((op < 32) ? (1 << op) : 1);
      if (!is_anchored(scode, options, new_map, backref_map))
        return FALSE;
    }

    /* Other brackets */

    else if (op == OP_BRA || op == OP_ASSERT || op == OP_ONCE || op == OP_COND) {
      if (!is_anchored(scode, options, bracket_map, backref_map))
        return FALSE;
    }

    /* .* is not anchored unless DOTALL is set and it isn't in brackets that
       are or may be referenced. */

    else if ((op == OP_TYPESTAR || op == OP_TYPEMINSTAR) &&
             (*options & PCRE_DOTALL) != 0) {
      if (scode[1] != OP_ANY || (bracket_map & backref_map) != 0)
        return FALSE;
    }

    /* Check for explicit anchoring */

    else if (op != OP_SOD && op != OP_SOM &&
             ((*options & PCRE_MULTILINE) != 0 || op != OP_CIRC))
      return FALSE;
    code += GET(code, 1);
  }
  while (*code == OP_ALT);      /* Loop for each alternative */
  return TRUE;
}



/*************************************************
*         Check for starting with ^ or .*        *
*************************************************/

/* This is called to find out if every branch starts with ^ or .* so that
"first char" processing can be done to speed things up in multiline
matching and for non-DOTALL patterns that start with .* (which must start at
the beginning or after \n). As in the case of is_anchored() (see above), we
have to take account of back references to capturing brackets that contain .*
because in that case we can't make the assumption.

Arguments:
  code           points to start of expression (the bracket)
  bracket_map    a bitmap of which brackets we are inside while testing; this
                  handles up to substring 31; after that we just have to take
                  the less precise approach
  backref_map    the back reference bitmap

Returns:         TRUE or FALSE
*/

static BOOL
is_startline(const uschar *code, unsigned int bracket_map,
             unsigned int backref_map)
{
  do {
    const uschar *scode = first_significant_code(code + 1 + LINK_SIZE, NULL, 0,
                                                 FALSE);
    register int op = *scode;

    /* Capturing brackets */

    if (op > OP_BRA) {
      int new_map;
      op -= OP_BRA;
      if (op > EXTRACT_BASIC_MAX)
        op = GET2(scode, 2 + LINK_SIZE);
      new_map = bracket_map | ((op < 32) ? (1 << op) : 1);
      if (!is_startline(scode, new_map, backref_map))
        return FALSE;
    }

    /* Other brackets */

    else if (op == OP_BRA || op == OP_ASSERT || op == OP_ONCE || op == OP_COND) {
      if (!is_startline(scode, bracket_map, backref_map))
        return FALSE;
    }

    /* .* means "start at start or after \n" if it isn't in brackets that
       may be referenced. */

    else if (op == OP_TYPESTAR || op == OP_TYPEMINSTAR) {
      if (scode[1] != OP_ANY || (bracket_map & backref_map) != 0)
        return FALSE;
    }

    /* Check for explicit circumflex */

    else if (op != OP_CIRC)
      return FALSE;

    /* Move on to the next alternative */

    code += GET(code, 1);
  }
  while (*code == OP_ALT);      /* Loop for each alternative */
  return TRUE;
}



/*************************************************
*       Check for asserted fixed first char      *
*************************************************/

/* During compilation, the "first char" settings from forward assertions are
discarded, because they can cause conflicts with actual literals that follow.
However, if we end up without a first char setting for an unanchored pattern,
it is worth scanning the regex to see if there is an initial asserted first
char. If all branches start with the same asserted char, or with a bracket all
of whose alternatives start with the same asserted char (recurse ad lib), then
we return that char, otherwise -1.

Arguments:
  code       points to start of expression (the bracket)
  options    pointer to the options (used to check casing changes)
  inassert   TRUE if in an assertion

Returns:     -1 or the fixed first char
*/

static int
find_firstassertedchar(const uschar *code, int *options, BOOL inassert)
{
  register int c = -1;
  do {
    int d;
    const uschar *scode =
      first_significant_code(code + 1 + LINK_SIZE, options, PCRE_CASELESS,
                             TRUE);
    register int op = *scode;

    if (op >= OP_BRA)
      op = OP_BRA;

    switch (op) {
    default:
      return -1;

    case OP_BRA:
    case OP_ASSERT:
    case OP_ONCE:
    case OP_COND:
      if ((d = find_firstassertedchar(scode, options, op == OP_ASSERT)) < 0)
        return -1;
      if (c < 0)
        c = d;
      else if (c != d)
        return -1;
      break;

    case OP_EXACT:             /* Fall through */
      scode += 2;

    case OP_CHAR:
    case OP_CHARNC:
    case OP_PLUS:
    case OP_MINPLUS:
      if (!inassert)
        return -1;
      if (c < 0) {
        c = scode[1];
        if ((*options & PCRE_CASELESS) != 0)
          c |= REQ_CASELESS;
      } else if (c != scode[1])
        return -1;
      break;
    }

    code += GET(code, 1);
  }
  while (*code == OP_ALT);
  return c;
}


pcre *pcre_compile2(const char *, int, int *, const char **,
                    int *, const unsigned char *);


/*************************************************
*        Compile a Regular Expression            *
*************************************************/

/* This function takes a string and returns a pointer to a block of store
holding a compiled version of the expression. The original API for this
function had no error code return variable; it is retained for backwards
compatibility. The new function is given a new name.

Arguments:
  pattern       the regular expression
  options       various option bits
  errorcodeptr  pointer to error code variable (pcre_compile2() only)
                  can be NULL if you don't want a code value
  errorptr      pointer to pointer to error text
  erroroffset   ptr offset in pattern where error was detected
  tables        pointer to character tables or NULL

Returns:        pointer to compiled data block, or NULL on error,
                with errorptr and erroroffset set
*/

pcre *
pcre_compile(const char *pattern, int options, const char **errorptr,
             int *erroroffset, const unsigned char *tables)
{
  return pcre_compile2(pattern, options, NULL, errorptr, erroroffset, tables);
}


pcre *
pcre_compile2(const char *pattern, int options, int *errorcodeptr,
              const char **errorptr, int *erroroffset,
              const unsigned char *tables)
{
  real_pcre *re;
  int length = 1 + LINK_SIZE;   /* For initial BRA plus length */
  int c, firstbyte, reqbyte;
  int bracount = 0;
  int branch_extra = 0;
  int branch_newextra;
  int item_count = -1;
  int name_count = 0;
  int max_name_size = 0;
  int lastitemlength = 0;
  int errorcode = 0;
  BOOL inescq = FALSE;
  BOOL capturing;
  unsigned int brastackptr = 0;
  size_t size;
  uschar *code;
  const uschar *codestart;
  const uschar *ptr;
  compile_data compile_block;
  int brastack[BRASTACK_SIZE];
  uschar bralenstack[BRASTACK_SIZE];

/* We can't pass back an error message if errorptr is NULL; I guess the best we
can do is just return NULL, but we can set a code value if there is a code
pointer. */

  if (errorptr == NULL) {
    if (errorcodeptr != NULL)
      *errorcodeptr = 99;
    return NULL;
  }

  *errorptr = NULL;
  if (errorcodeptr != NULL)
    *errorcodeptr = ERR0;

/* However, we can give a message for this error */

  if (erroroffset == NULL) {
    errorcode = ERR16;
    goto PCRE_EARLY_ERROR_RETURN;
  }

  *erroroffset = 0;

/* Can't support UTF8 unless PCRE has been compiled to include the code. */

  if ((options & PCRE_UTF8) != 0) {
    errorcode = ERR32;
    goto PCRE_EARLY_ERROR_RETURN;
  }

  if ((options & ~PUBLIC_OPTIONS) != 0) {
    errorcode = ERR17;
    goto PCRE_EARLY_ERROR_RETURN;
  }

/* Set up pointers to the individual character tables */

  if (tables == NULL)
    tables = _pcre_default_tables;
  compile_block.lcc = tables + lcc_offset;
  compile_block.fcc = tables + fcc_offset;
  compile_block.cbits = tables + cbits_offset;
  compile_block.ctypes = tables + ctypes_offset;

/* Maximum back reference and backref bitmap. This is updated for numeric
references during the first pass, but for named references during the actual
compile pass. The bitmap records up to 31 back references to help in deciding
whether (.*) can be treated as anchored or not. */

  compile_block.top_backref = 0;
  compile_block.backref_map = 0;

/* Reflect pattern for debugging output */

/* The first thing to do is to make a pass over the pattern to compute the
amount of store required to hold the compiled code. This does not have to be
perfect as long as errors are overestimates. At the same time we can detect any
flag settings right at the start, and extract them. Make an attempt to correct
for any counted white space if an "extended" flag setting appears late in the
pattern. We can't be so clever for #-comments. */

  ptr = (const uschar *) (pattern - 1);
  while ((c = *(++ptr)) != 0) {
    int min, max;
    int class_optcount;
    int bracket_length;
    int duplength;

    /* If we are inside a \Q...\E sequence, all chars are literal */

    if (inescq) {
      if ((options & PCRE_AUTO_CALLOUT) != 0)
        length += 2 + 2 * LINK_SIZE;
      goto NORMAL_CHAR;
    }

    /* Otherwise, first check for ignored whitespace and comments */

    if ((options & PCRE_EXTENDED) != 0) {
      if ((compile_block.ctypes[c] & ctype_space) != 0)
        continue;
      if (c == '#') {
        /* The space before the ; is to avoid a warning on a silly compiler
           on the Macintosh. */
        while ((c = *(++ptr)) != 0 && c != NEWLINE) ;
        if (c == 0)
          break;
        continue;
      }
    }

    item_count++;               /* Is zero for the first non-comment item */

    /* Allow space for auto callout before every item except quantifiers. */

    if ((options & PCRE_AUTO_CALLOUT) != 0 &&
        c != '*' && c != '+' && c != '?' &&
        (c != '{' || !is_counted_repeat(ptr + 1)))
      length += 2 + 2 * LINK_SIZE;

    switch (c) {
      /* A backslashed item may be an escaped data character or it may be a
         character type. */

    case '\\':
      c = check_escape(&ptr, &errorcode, bracount, options, FALSE);
      if (errorcode != 0)
        goto PCRE_ERROR_RETURN;

      lastitemlength = 1;       /* Default length of last item for repeats */

      if (c >= 0) {             /* Data character */
        length += 2;            /* For a one-byte character */


        continue;
      }

      /* If \Q, enter "literal" mode */

      if (-c == ESC_Q) {
        inescq = TRUE;
        continue;
      }

      /* \X is supported only if Unicode property support is compiled */

#ifndef SUPPORT_UCP
      if (-c == ESC_X) {
        errorcode = ERR45;
        goto PCRE_ERROR_RETURN;
      }
#endif

      /* \P and \p are for Unicode properties, but only when the support has
         been compiled. Each item needs 2 bytes. */

      else if (-c == ESC_P || -c == ESC_p) {
#ifdef SUPPORT_UCP
        BOOL negated;
        length += 2;
        lastitemlength = 2;
        if (get_ucp(&ptr, &negated, &errorcode) < 0)
          goto PCRE_ERROR_RETURN;
        continue;
#else
        errorcode = ERR45;
        goto PCRE_ERROR_RETURN;
#endif
      }

      /* Other escapes need one byte */

      length++;

      /* A back reference needs an additional 2 bytes, plus either one or 5
         bytes for a repeat. We also need to keep the value of the highest
         back reference. */

      if (c <= -ESC_REF) {
        int refnum = -c - ESC_REF;
        compile_block.backref_map |= (refnum < 32) ? (1 << refnum) : 1;
        if (refnum > compile_block.top_backref)
          compile_block.top_backref = refnum;
        length += 2;            /* For single back reference */
        if (ptr[1] == '{' && is_counted_repeat(ptr + 2)) {
          ptr = read_repeat_counts(ptr + 2, &min, &max, &errorcode);
          if (errorcode != 0)
            goto PCRE_ERROR_RETURN;
          if ((min == 0 && (max == 1 || max == -1)) || (min == 1 && max == -1))
            length++;
          else
            length += 5;
          if (ptr[1] == '?')
            ptr++;
        }
      }
      continue;

    case '^':                  /* Single-byte metacharacters */
    case '.':
    case '$':
      length++;
      lastitemlength = 1;
      continue;

    case '*':                  /* These repeats won't be after brackets; */
    case '+':                  /* those are handled separately */
    case '?':
      length++;
      goto POSESSIVE;           /* A few lines below */

      /* This covers the cases of braced repeats after a single char, metachar,
         class, or back reference. */

    case '{':
      if (!is_counted_repeat(ptr + 1))
        goto NORMAL_CHAR;
      ptr = read_repeat_counts(ptr + 1, &min, &max, &errorcode);
      if (errorcode != 0)
        goto PCRE_ERROR_RETURN;

      /* These special cases just insert one extra opcode */

      if ((min == 0 && (max == 1 || max == -1)) || (min == 1 && max == -1))
        length++;

      /* These cases might insert additional copies of a preceding character. */

      else {
        if (min != 1) {
          length -= lastitemlength;     /* Uncount the original char or metachar */
          if (min > 0)
            length += 3 + lastitemlength;
        }
        length += lastitemlength + ((max > 0) ? 3 : 1);
      }

      if (ptr[1] == '?')
        ptr++;                  /* Needs no extra length */

    POSESSIVE:                 /* Test for possessive quantifier */
      if (ptr[1] == '+') {
        ptr++;
        length += 2 + 2 * LINK_SIZE;    /* Allow for atomic brackets */
      }
      continue;

      /* An alternation contains an offset to the next branch or ket. If any ims
         options changed in the previous branch(es), and/or if we are in a
         lookbehind assertion, extra space will be needed at the start of the
         branch. This is handled by branch_extra. */

    case '|':
      length += 1 + LINK_SIZE + branch_extra;
      continue;

      /* A character class uses 33 characters provided that all the character
         values are less than 256. Otherwise, it uses a bit map for low valued
         characters, and individual items for others. Don't worry about character
         types that aren't allowed in classes - they'll get picked up during the
         compile. A character class that contains only one single-byte character
         uses 2 or 3 bytes, depending on whether it is negated or not. Notice this
         where we can. (In UTF-8 mode we can do this only for chars < 128.) */

    case '[':
      if (*(++ptr) == '^') {
        class_optcount = 10;    /* Greater than one */
        ptr++;
      } else
        class_optcount = 0;


      /* Written as a "do" so that an initial ']' is taken as data */

      if (*ptr != 0)
        do {
          /* Inside \Q...\E everything is literal except \E */

          if (inescq) {
            if (*ptr != '\\' || ptr[1] != 'E')
              goto GET_ONE_CHARACTER;
            inescq = FALSE;
            ptr += 1;
            continue;
          }

          /* Outside \Q...\E, check for escapes */

          if (*ptr == '\\') {
            c = check_escape(&ptr, &errorcode, bracount, options, TRUE);
            if (errorcode != 0)
              goto PCRE_ERROR_RETURN;

            /* \b is backspace inside a class; \X is literal */

            if (-c == ESC_b)
              c = '\b';
            else if (-c == ESC_X)
              c = 'X';

            /* \Q enters quoting mode */

            else if (-c == ESC_Q) {
              inescq = TRUE;
              continue;
            }

            /* Handle escapes that turn into characters */

            if (c >= 0)
              goto NON_SPECIAL_CHARACTER;

            /* Escapes that are meta-things. The normal ones just affect the
               bit map, but Unicode properties require an XCLASS extended item. */

            else {
              class_optcount = 10;      /* \d, \s etc; make sure > 1 */
            }
          }

          /* Check the syntax for POSIX stuff. The bits we actually handle are
             checked during the real compile phase. */

          else if (*ptr == '[' && check_posix_syntax(ptr, &ptr, &compile_block)) {
            ptr++;
            class_optcount = 10;        /* Make sure > 1 */
          }

          /* Anything else increments the possible optimization count. We have to
             detect ranges here so that we can compute the number of extra ranges for
             caseless wide characters when UCP support is available. If there are wide
             characters, we are going to have to use an XCLASS, even for single
             characters. */

          else {
            int d;

          GET_ONE_CHARACTER:

            c = *ptr;

            /* Come here from handling \ above when it escapes to a char value */

          NON_SPECIAL_CHARACTER:
            class_optcount++;

            d = -1;
            if (ptr[1] == '-') {
              uschar const *hyptr = ptr++;
              if (ptr[1] == '\\') {
                ptr++;
                d = check_escape(&ptr, &errorcode, bracount, options, TRUE);
                if (errorcode != 0)
                  goto PCRE_ERROR_RETURN;
                if (-d == ESC_b)
                  d = '\b';     /* backspace */
                else if (-d == ESC_X)
                  d = 'X';      /* literal X in a class */
              } else if (ptr[1] != 0 && ptr[1] != ']') {
                ptr++;
                d = *ptr;
              }
              if (d < 0)
                ptr = hyptr;    /* go back to hyphen as data */
            }

            /* If d >= 0 we have a range. In UTF-8 mode, if the end is > 255, or >
               127 for caseless matching, we will need to use an XCLASS. */

            if (d >= 0) {
              class_optcount = 10;      /* Ensure > 1 */
              if (d < c) {
                errorcode = ERR8;
                goto PCRE_ERROR_RETURN;
              }


            }

            /* We have a single character. There is nothing to be done unless we
               are in UTF-8 mode. If the char is > 255, or 127 when caseless, we must
               allow for an XCL_SINGLE item, doubled for caselessness if there is UCP
               support. */

            else {
            }
          }
        }
        while (*(++ptr) != 0 && (inescq || *ptr != ']'));       /* Concludes "do" above */

      if (*ptr == 0) {          /* Missing terminating ']' */
        errorcode = ERR6;
        goto PCRE_ERROR_RETURN;
      }

      /* We can optimize when there was only one optimizable character. Repeats
         for positive and negated single one-byte chars are handled by the general
         code. Here, we handle repeats for the class opcodes. */

      if (class_optcount == 1)
        length += 3;
      else {
        length += 33;

        /* A repeat needs either 1 or 5 bytes. If it is a possessive quantifier,
           we also need extra for wrapping the whole thing in a sub-pattern. */

        if (*ptr != 0 && ptr[1] == '{' && is_counted_repeat(ptr + 2)) {
          ptr = read_repeat_counts(ptr + 2, &min, &max, &errorcode);
          if (errorcode != 0)
            goto PCRE_ERROR_RETURN;
          if ((min == 0 && (max == 1 || max == -1)) || (min == 1 && max == -1))
            length++;
          else
            length += 5;
          if (ptr[1] == '+') {
            ptr++;
            length += 2 + 2 * LINK_SIZE;
          } else if (ptr[1] == '?')
            ptr++;
        }
      }
      continue;

      /* Brackets may be genuine groups or special things */

    case '(':
      branch_newextra = 0;
      bracket_length = 1 + LINK_SIZE;
      capturing = FALSE;

      /* Handle special forms of bracket, which all start (? */

      if (ptr[1] == '?') {
        int set, unset;
        int *optset;

        switch (c = ptr[2]) {
          /* Skip over comments entirely */
        case '#':
          ptr += 3;
          while (*ptr != 0 && *ptr != ')')
            ptr++;
          if (*ptr == 0) {
            errorcode = ERR18;
            goto PCRE_ERROR_RETURN;
          }
          continue;

          /* Non-referencing groups and lookaheads just move the pointer on, and
             then behave like a non-special bracket, except that they don't increment
             the count of extracting brackets. Ditto for the "once only" bracket,
             which is in Perl from version 5.005. */

        case ':':
        case '=':
        case '!':
        case '>':
          ptr += 2;
          break;

          /* (?R) specifies a recursive call to the regex, which is an extension
             to provide the facility which can be obtained by (?p{perl-code}) in
             Perl 5.6. In Perl 5.8 this has become (??{perl-code}).

             From PCRE 4.00, items such as (?3) specify subroutine-like "calls" to
             the appropriate numbered brackets. This includes both recursive and
             non-recursive calls. (?R) is now synonymous with (?0). */

        case 'R':
          ptr++;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          ptr += 2;
          if (c != 'R')
            while ((digitab[*(++ptr)] & ctype_digit) != 0) ;
          if (*ptr != ')') {
            errorcode = ERR29;
            goto PCRE_ERROR_RETURN;
          }
          length += 1 + LINK_SIZE;

          /* If this item is quantified, it will get wrapped inside brackets so
             as to use the code for quantified brackets. We jump down and use the
             code that handles this for real brackets. */

          if (ptr[1] == '+' || ptr[1] == '*' || ptr[1] == '?' || ptr[1] == '{') {
            length += 2 + 2 * LINK_SIZE;        /* to make bracketed */
            duplength = 5 + 3 * LINK_SIZE;
            goto HANDLE_QUANTIFIED_BRACKETS;
          }
          continue;

          /* (?C) is an extension which provides "callout" - to provide a bit of
             the functionality of the Perl (?{...}) feature. An optional number may
             follow (default is zero). */

        case 'C':
          ptr += 2;
          while ((digitab[*(++ptr)] & ctype_digit) != 0) ;
          if (*ptr != ')') {
            errorcode = ERR39;
            goto PCRE_ERROR_RETURN;
          }
          length += 2 + 2 * LINK_SIZE;
          continue;

          /* Named subpatterns are an extension copied from Python */

        case 'P':
          ptr += 3;

          /* Handle the definition of a named subpattern */

          if (*ptr == '<') {
            const uschar *p;    /* Don't amalgamate; some compilers */
            p = ++ptr;          /* grumble at autoincrement in declaration */
            while ((compile_block.ctypes[*ptr] & ctype_word) != 0)
              ptr++;
            if (*ptr != '>') {
              errorcode = ERR42;
              goto PCRE_ERROR_RETURN;
            }
            name_count++;
            if (ptr - p > max_name_size)
              max_name_size = (ptr - p);
            capturing = TRUE;   /* Named parentheses are always capturing */
            break;
          }

          /* Handle back references and recursive calls to named subpatterns */

          if (*ptr == '=' || *ptr == '>') {
            while ((compile_block.ctypes[*(++ptr)] & ctype_word) != 0) ;
            if (*ptr != ')') {
              errorcode = ERR42;
              goto PCRE_ERROR_RETURN;
            }
            break;
          }

          /* Unknown character after (?P */

          errorcode = ERR41;
          goto PCRE_ERROR_RETURN;

          /* Lookbehinds are in Perl from version 5.005 */

        case '<':
          ptr += 3;
          if (*ptr == '=' || *ptr == '!') {
            branch_newextra = 1 + LINK_SIZE;
            length += 1 + LINK_SIZE;    /* For the first branch */
            break;
          }
          errorcode = ERR24;
          goto PCRE_ERROR_RETURN;

          /* Conditionals are in Perl from version 5.005. The bracket must either
             be followed by a number (for bracket reference) or by an assertion
             group, or (a PCRE extension) by 'R' for a recursion test. */

        case '(':
          if (ptr[3] == 'R' && ptr[4] == ')') {
            ptr += 4;
            length += 3;
          } else if ((digitab[ptr[3]] & ctype_digit) != 0) {
            ptr += 4;
            length += 3;
            while ((digitab[*ptr] & ctype_digit) != 0)
              ptr++;
            if (*ptr != ')') {
              errorcode = ERR26;
              goto PCRE_ERROR_RETURN;
            }
          } else {              /* An assertion must follow */

            ptr++;              /* Can treat like ':' as far as spacing is concerned */
            if (ptr[2] != '?' ||
                (ptr[3] != '=' && ptr[3] != '!' && ptr[3] != '<')) {
              ptr += 2;         /* To get right offset in message */
              errorcode = ERR28;
              goto PCRE_ERROR_RETURN;
            }
          }
          break;

          /* Else loop checking valid options until ) is met. Anything else is an
             error. If we are without any brackets, i.e. at top level, the settings
             act as if specified in the options, so massage the options immediately.
             This is for backward compatibility with Perl 5.004. */

        default:
          set = unset = 0;
          optset = &set;
          ptr += 2;

          for (;; ptr++) {
            c = *ptr;
            switch (c) {
            case 'i':
              *optset |= PCRE_CASELESS;
              continue;

            case 'm':
              *optset |= PCRE_MULTILINE;
              continue;

            case 's':
              *optset |= PCRE_DOTALL;
              continue;

            case 'x':
              *optset |= PCRE_EXTENDED;
              continue;

            case 'X':
              *optset |= PCRE_EXTRA;
              continue;

            case 'U':
              *optset |= PCRE_UNGREEDY;
              continue;

            case '-':
              optset = &unset;
              continue;

              /* A termination by ')' indicates an options-setting-only item; if
                 this is at the very start of the pattern (indicated by item_count
                 being zero), we use it to set the global options. This is helpful
                 when analyzing the pattern for first characters, etc. Otherwise
                 nothing is done here and it is handled during the compiling
                 process.

                 We allow for more than one options setting at the start. If such
                 settings do not change the existing options, nothing is compiled.
                 However, we must leave space just in case something is compiled.
                 This can happen for pathological sequences such as (?i)(?-i)
                 because the global options will end up with -i set. The space is
                 small and not significant. (Before I did this there was a reported
                 bug with (?i)(?-i) in a machine-generated pattern.)

                 [Historical note: Up to Perl 5.8, options settings at top level
                 were always global settings, wherever they appeared in the pattern.
                 That is, they were equivalent to an external setting. From 5.8
                 onwards, they apply only to what follows (which is what you might
                 expect).] */

            case ')':
              if (item_count == 0) {
                options = (options | set) & (~unset);
                set = unset = 0;        /* To save length */
                item_count--;   /* To allow for several */
                length += 2;
              }

              /* Fall through */

              /* A termination by ':' indicates the start of a nested group with
                 the given options set. This is again handled at compile time, but
                 we must allow for compiled space if any of the ims options are
                 set. We also have to allow for resetting space at the end of
                 the group, which is why 4 is added to the length and not just 2.
                 If there are several changes of options within the same group, this
                 will lead to an over-estimate on the length, but this shouldn't
                 matter very much. We also have to allow for resetting options at
                 the start of any alternations, which we do by setting
                 branch_newextra to 2. Finally, we record whether the case-dependent
                 flag ever changes within the regex. This is used by the "required
                 character" code. */

            case ':':
              if (((set | unset) & PCRE_IMS) != 0) {
                length += 4;
                branch_newextra = 2;
                if (((set | unset) & PCRE_CASELESS) != 0)
                  options |= PCRE_ICHANGED;
              }
              goto END_OPTIONS;

              /* Unrecognized option character */

            default:
              errorcode = ERR12;
              goto PCRE_ERROR_RETURN;
            }
          }

          /* If we hit a closing bracket, that's it - this is a freestanding
             option-setting. We need to ensure that branch_extra is updated if
             necessary. The only values branch_newextra can have here are 0 or 2.
             If the value is 2, then branch_extra must either be 2 or 5, depending
             on whether this is a lookbehind group or not. */

        END_OPTIONS:
          if (c == ')') {
            if (branch_newextra == 2 &&
                (branch_extra == 0 || branch_extra == 1 + LINK_SIZE))
              branch_extra += branch_newextra;
            continue;
          }

          /* If options were terminated by ':' control comes here. This is a
             non-capturing group with an options change. There is nothing more that
             needs to be done because "capturing" is already set FALSE by default;
             we can just fall through. */

        }
      }

      /* Ordinary parentheses, not followed by '?', are capturing unless
         PCRE_NO_AUTO_CAPTURE is set. */

      else
        capturing = (options & PCRE_NO_AUTO_CAPTURE) == 0;

      /* Capturing brackets must be counted so we can process escapes in a
         Perlish way. If the number exceeds EXTRACT_BASIC_MAX we are going to need
         an additional 3 bytes of memory per capturing bracket. */

      if (capturing) {
        bracount++;
        if (bracount > EXTRACT_BASIC_MAX)
          bracket_length += 3;
      }

      /* Save length for computing whole length at end if there's a repeat that
         requires duplication of the group. Also save the current value of
         branch_extra, and start the new group with the new value. If non-zero, this
         will either be 2 for a (?imsx: group, or 3 for a lookbehind assertion. */

      if (brastackptr >= sizeof(brastack) / sizeof(int)) {
        errorcode = ERR19;
        goto PCRE_ERROR_RETURN;
      }

      bralenstack[brastackptr] = branch_extra;
      branch_extra = branch_newextra;

      brastack[brastackptr++] = length;
      length += bracket_length;
      continue;

      /* Handle ket. Look for subsequent max/min; for certain sets of values we
         have to replicate this bracket up to that many times. If brastackptr is
         0 this is an unmatched bracket which will generate an error, but take care
         not to try to access brastack[-1] when computing the length and restoring
         the branch_extra value. */

    case ')':
      length += 1 + LINK_SIZE;
      if (brastackptr > 0) {
        duplength = length - brastack[--brastackptr];
        branch_extra = bralenstack[brastackptr];
      } else
        duplength = 0;

      /* The following code is also used when a recursion such as (?3) is
         followed by a quantifier, because in that case, it has to be wrapped inside
         brackets so that the quantifier works. The value of duplength must be
         set before arrival. */

    HANDLE_QUANTIFIED_BRACKETS:

      /* Leave ptr at the final char; for read_repeat_counts this happens
         automatically; for the others we need an increment. */

      if ((c = ptr[1]) == '{' && is_counted_repeat(ptr + 2)) {
        ptr = read_repeat_counts(ptr + 2, &min, &max, &errorcode);
        if (errorcode != 0)
          goto PCRE_ERROR_RETURN;
      } else if (c == '*') {
        min = 0;
        max = -1;
        ptr++;
      } else if (c == '+') {
        min = 1;
        max = -1;
        ptr++;
      } else if (c == '?') {
        min = 0;
        max = 1;
        ptr++;
      } else {
        min = 1;
        max = 1;
      }

      /* If the minimum is zero, we have to allow for an OP_BRAZERO before the
         group, and if the maximum is greater than zero, we have to replicate
         maxval-1 times; each replication acquires an OP_BRAZERO plus a nesting
         bracket set. */

      if (min == 0) {
        length++;
        if (max > 0)
          length += (max - 1) * (duplength + 3 + 2 * LINK_SIZE);
      }

      /* When the minimum is greater than zero, we have to replicate up to
         minval-1 times, with no additions required in the copies. Then, if there
         is a limited maximum we have to replicate up to maxval-1 times allowing
         for a BRAZERO item before each optional copy and nesting brackets for all
         but one of the optional copies. */

      else {
        length += (min - 1) * duplength;
        if (max > min)          /* Need this test as max=-1 means no limit */
          length += (max - min) * (duplength + 3 + 2 * LINK_SIZE)
            - (2 + 2 * LINK_SIZE);
      }

      /* Allow space for once brackets for "possessive quantifier" */

      if (ptr[1] == '+') {
        ptr++;
        length += 2 + 2 * LINK_SIZE;
      }
      continue;

      /* Non-special character. It won't be space or # in extended mode, so it is
         always a genuine character. If we are in a \Q...\E sequence, check for the
         end; if not, we have a literal. */

    default:
    NORMAL_CHAR:

      if (inescq && c == '\\' && ptr[1] == 'E') {
        inescq = FALSE;
        ptr++;
        continue;
      }

      length += 2;              /* For a one-byte character */
      lastitemlength = 1;       /* Default length of last item for repeats */

      /* In UTF-8 mode, check for additional bytes. */


      continue;
    }
  }

  length += 2 + LINK_SIZE;      /* For final KET and END */

  if ((options & PCRE_AUTO_CALLOUT) != 0)
    length += 2 + 2 * LINK_SIZE;        /* For final callout */

  if (length > MAX_PATTERN_SIZE) {
    errorcode = ERR20;
    goto PCRE_EARLY_ERROR_RETURN;
  }

/* Compute the size of data block needed and get it, either from malloc or
externally provided function. */

  size = length + sizeof(real_pcre) + name_count * (max_name_size + 3);
  re = (real_pcre *) malloc(size);

  if (re == NULL) {
    errorcode = ERR21;
    goto PCRE_EARLY_ERROR_RETURN;
  }

/* Put in the magic number, and save the sizes, options, and character table
pointer. NULL is used for the default character tables. The nullpad field is at
the end; it's there to help in the case when a regex compiled on a system with
4-byte pointers is run on another with 8-byte pointers. */

  re->magic_number = MAGIC_NUMBER;
  re->size = size;
  re->options = options;
  re->dummy1 = 0;
  re->name_table_offset = sizeof(real_pcre);
  re->name_entry_size = max_name_size + 3;
  re->name_count = name_count;
  re->ref_count = 0;
  re->tables = (tables == _pcre_default_tables) ? NULL : tables;
  re->nullpad = NULL;

/* The starting points of the name/number translation table and of the code are
passed around in the compile data block. */

  compile_block.names_found = 0;
  compile_block.name_entry_size = max_name_size + 3;
  compile_block.name_table = (uschar *) re + re->name_table_offset;
  codestart = compile_block.name_table + re->name_entry_size * re->name_count;
  compile_block.start_code = codestart;
  compile_block.start_pattern = (const uschar *) pattern;
  compile_block.req_varyopt = 0;
  compile_block.nopartial = FALSE;

/* Set up a starting, non-extracting bracket, then compile the expression. On
error, errorcode will be set non-zero, so we don't need to look at the result
of the function here. */

  ptr = (const uschar *) pattern;
  code = (uschar *) codestart;
  *code = OP_BRA;
  bracount = 0;
  (void) compile_regex(options, options & PCRE_IMS, &bracount, &code, &ptr,
                       &errorcode, FALSE, 0, &firstbyte, &reqbyte, NULL,
                       &compile_block);
  re->top_bracket = bracount;
  re->top_backref = compile_block.top_backref;

  if (compile_block.nopartial)
    re->options |= PCRE_NOPARTIAL;

/* If not reached end of pattern on success, there's an excess bracket. */

  if (errorcode == 0 && *ptr != 0)
    errorcode = ERR22;

/* Fill in the terminating state and check for disastrous overflow, but
if debugging, leave the test till after things are printed out. */

  *code++ = OP_END;

  if (code - codestart > length)
    errorcode = ERR23;

/* Give an error if there's back reference to a non-existent capturing
subpattern. */

  if (re->top_backref > re->top_bracket)
    errorcode = ERR15;

/* Failed to compile, or error while post-processing */

  if (errorcode != 0) {
    free(re);
  PCRE_ERROR_RETURN:
    *erroroffset = ptr - (const uschar *) pattern;
  PCRE_EARLY_ERROR_RETURN:
    *errorptr = error_texts[errorcode];
    if (errorcodeptr != NULL)
      *errorcodeptr = errorcode;
    return NULL;
  }

/* If the anchored option was not passed, set the flag if we can determine that
the pattern is anchored by virtue of ^ characters or \A or anything else (such
as starting with .* when DOTALL is set).

Otherwise, if we know what the first character has to be, save it, because that
speeds up unanchored matches no end. If not, see if we can set the
PCRE_STARTLINE flag. This is helpful for multiline matches when all branches
start with ^. and also when all branches start with .* for non-DOTALL matches.
*/

  if ((options & PCRE_ANCHORED) == 0) {
    int temp_options = options;
    if (is_anchored(codestart, &temp_options, 0, compile_block.backref_map))
      re->options |= PCRE_ANCHORED;
    else {
      if (firstbyte < 0)
        firstbyte = find_firstassertedchar(codestart, &temp_options, FALSE);
      if (firstbyte >= 0) {     /* Remove caseless flag for non-caseable chars */
        int ch = firstbyte & 255;
        re->first_byte = ((firstbyte & REQ_CASELESS) != 0 &&
                          compile_block.fcc[ch] == ch) ? ch : firstbyte;
        re->options |= PCRE_FIRSTSET;
      } else if (is_startline(codestart, 0, compile_block.backref_map))
        re->options |= PCRE_STARTLINE;
    }
  }

/* For an anchored pattern, we use the "required byte" only if it follows a
variable length item in the regex. Remove the caseless flag for non-caseable
bytes. */

  if (reqbyte >= 0 &&
      ((re->options & PCRE_ANCHORED) == 0 || (reqbyte & REQ_VARY) != 0)) {
    int ch = reqbyte & 255;
    re->req_byte = ((reqbyte & REQ_CASELESS) != 0 &&
                    compile_block.fcc[ch] ==
                    ch) ? (reqbyte & ~REQ_CASELESS) : reqbyte;
    re->options |= PCRE_REQCHSET;
  }

/* Print out the compiled data if debugging is enabled. This is never the
case when building a production library. */


  return (pcre *) re;
}

/* End of pcre_compile.c */

/* pcre_exec.c */


/* This module contains pcre_exec(), the externally visible function that does
pattern matching using an NFA algorithm, trying to mimic Perl as closely as
possible. There are also some static supporting functions. */



/* Structure for building a chain of data that actually lives on the
stack, for holding the values of the subject pointer at the start of each
subpattern, so as to detect when an empty string has been matched by a
subpattern - to break infinite loops. When NO_RECURSE is set, these blocks
are on the heap, not on the stack. */

typedef struct eptrblock {
  struct eptrblock *epb_prev;
  const uschar *epb_saved_eptr;
} eptrblock;

/* Flag bits for the match() function */

#define match_condassert   0x01 /* Called to check a condition assertion */
#define match_isgroup      0x02 /* Set if start of bracketed group */

/* Non-error returns from the match() function. Error returns are externally
defined PCRE_ERROR_xxx codes, which are all negative. */

#define MATCH_MATCH        1
#define MATCH_NOMATCH      0

/* Maximum number of ints of offset to save on the stack for recursive calls.
If the offset vector is bigger, malloc is used. This should be a multiple of 3,
because the offset vector is always a multiple of 3 long. */

#define REC_STACK_SAVE_MAX 30

/* Min and max values for the common repeats; for the maxima, 0 => infinity */

static const char rep_min[] = { 0, 0, 1, 1, 0, 0 };
static const char rep_max[] = { 0, 0, 0, 0, 1, 1 };






/*************************************************
*          Match a back-reference                *
*************************************************/

/* If a back reference hasn't been set, the length that is passed is greater
than the number of characters left in the string, so the match fails.

Arguments:
  offset      index into the offset vector
  eptr        points into the subject
  length      length to be matched
  md          points to match data block
  ims         the ims flags

Returns:      TRUE if matched
*/

static BOOL
match_ref(int offset, register const uschar *eptr, int length, match_data *md,
          unsigned long int ims)
{
  const uschar *p = md->start_subject + md->offset_vector[offset];


/* Always fail if not enough characters left */

  if (length > md->end_subject - eptr)
    return FALSE;

/* Separate the caselesss case for speed */

  if ((ims & PCRE_CASELESS) != 0) {
    while (length-- > 0)
      if (md->lcc[*p++] != md->lcc[*eptr++])
        return FALSE;
  } else {
    while (length-- > 0)
      if (*p++ != *eptr++)
        return FALSE;
  }

  return TRUE;
}



/***************************************************************************
****************************************************************************
                   RECURSION IN THE match() FUNCTION

The match() function is highly recursive. Some regular expressions can cause
it to recurse thousands of times. I was writing for Unix, so I just let it
call itself recursively. This uses the stack for saving everything that has
to be saved for a recursive call. On Unix, the stack can be large, and this
works fine.

It turns out that on non-Unix systems there are problems with programs that
use a lot of stack. (This despite the fact that every last chip has oodles
of memory these days, and techniques for extending the stack have been known
for decades.) So....

There is a fudge, triggered by defining NO_RECURSE, which avoids recursive
calls by keeping local variables that need to be preserved in blocks of memory
obtained from malloc instead instead of on the stack. Macros are used to
achieve this so that the actual code doesn't look very different to what it
always used to.
****************************************************************************
***************************************************************************/


/* These versions of the macros use the stack, as normal */

#ifndef NO_RECURSE
#define REGISTER register
#define RMATCH(rx,ra,rb,rc,rd,re,rf,rg) rx = match(ra,rb,rc,rd,re,rf,rg)
#define RRETURN(ra) return ra
#else


/* These versions of the macros manage a private stack on the heap. Note
that the rd argument of RMATCH isn't actually used. It's the md argument of
match(), which never changes. */

#define REGISTER

#define RMATCH(rx,ra,rb,rc,rd,re,rf,rg)\
  {\
  heapframe *newframe = malloc(sizeof(heapframe));\
  if (setjmp(frame->Xwhere) == 0)\
    {\
    newframe->Xeptr = ra;\
    newframe->Xecode = rb;\
    newframe->Xoffset_top = rc;\
    newframe->Xims = re;\
    newframe->Xeptrb = rf;\
    newframe->Xflags = rg;\
    newframe->Xprevframe = frame;\
    frame = newframe;\
    goto HEAP_RECURSE;\
    }\
  else\
    {\
    frame = md->thisframe;\
    rx = frame->Xresult;\
    }\
  }

#define RRETURN(ra)\
  {\
  heapframe *newframe = frame;\
  frame = newframe->Xprevframe;\
  free(newframe);\
  if (frame != NULL)\
    {\
    frame->Xresult = ra;\
    md->thisframe = frame;\
    longjmp(frame->Xwhere, 1);\
    }\
  return ra;\
  }


/* Structure for remembering the local variables in a private frame */

typedef struct heapframe {
  struct heapframe *Xprevframe;

  /* Function arguments that may change */

  const uschar *Xeptr;
  const uschar *Xecode;
  int Xoffset_top;
  long int Xims;
  eptrblock *Xeptrb;
  int Xflags;

  /* Function local variables */

  const uschar *Xcallpat;
  const uschar *Xcharptr;
  const uschar *Xdata;
  const uschar *Xnext;
  const uschar *Xpp;
  const uschar *Xprev;
  const uschar *Xsaved_eptr;

  recursion_info Xnew_recursive;

  BOOL Xcur_is_word;
  BOOL Xcondition;
  BOOL Xminimize;
  BOOL Xprev_is_word;

  unsigned long int Xoriginal_ims;

#ifdef SUPPORT_UCP
  int Xprop_type;
  int Xprop_fail_result;
  int Xprop_category;
  int Xprop_chartype;
  int Xprop_othercase;
  int Xprop_test_against;
  int *Xprop_test_variable;
#endif

  int Xctype;
  int Xfc;
  int Xfi;
  int Xlength;
  int Xmax;
  int Xmin;
  int Xnumber;
  int Xoffset;
  int Xop;
  int Xsave_capture_last;
  int Xsave_offset1, Xsave_offset2, Xsave_offset3;
  int Xstacksave[REC_STACK_SAVE_MAX];

  eptrblock Xnewptrb;

  /* Place to pass back result, and where to jump back to */

  int Xresult;
  jmp_buf Xwhere;

} heapframe;

#endif


/***************************************************************************
***************************************************************************/



/*************************************************
*         Match from current position            *
*************************************************/

/* On entry ecode points to the first opcode, and eptr to the first character
in the subject string, while eptrb holds the value of eptr at the start of the
last bracketed group - used for breaking infinite loops matching zero-length
strings. This function is called recursively in many circumstances. Whenever it
returns a negative (error) response, the outer incarnation must also return the
same response.

Performance note: It might be tempting to extract commonly used fields from the
md structure (e.g. utf8, end_subject) into individual variables to improve
performance. Tests using gcc on a SPARC disproved this; in the first case, it
made performance worse.

Arguments:
   eptr        pointer in subject
   ecode       position in code
   offset_top  current top pointer
   md          pointer to "static" info for the match
   ims         current /i, /m, and /s options
   eptrb       pointer to chain of blocks containing eptr at start of
                 brackets - for testing for empty matches
   flags       can contain
                 match_condassert - this is an assertion condition
                 match_isgroup - this is the start of a bracketed group

Returns:       MATCH_MATCH if matched            )  these values are >= 0
               MATCH_NOMATCH if failed to match  )
               a negative PCRE_ERROR_xxx value if aborted by an error condition
                 (e.g. stopped by recursion limit)
*/

static int
match(REGISTER const uschar *eptr, REGISTER const uschar *ecode,
      int offset_top, match_data *md, unsigned long int ims, eptrblock *eptrb,
      int flags)
{
/* These variables do not need to be preserved over recursion in this function,
so they can be ordinary variables in all cases. Mark them with "register"
because they are used a lot in loops. */

  register int rrc;             /* Returns from recursive calls */
  register int i;               /* Used for loops not involving calls to RMATCH() */
  register int c;               /* Character values not kept over RMATCH() calls */
  register BOOL utf8;           /* Local copy of UTF-8 flag for speed */

/* When recursion is not being used, all "local" variables that have to be
preserved over calls to RMATCH() are part of a "frame" which is obtained from
heap storage. Set up the top-level frame here; others are obtained from the
heap whenever RMATCH() does a "recursion". See the macro definitions above. */

#ifdef NO_RECURSE
  heapframe *frame = malloc(sizeof(heapframe));
  frame->Xprevframe = NULL;     /* Marks the top level */

/* Copy in the original argument variables */

  frame->Xeptr = eptr;
  frame->Xecode = ecode;
  frame->Xoffset_top = offset_top;
  frame->Xims = ims;
  frame->Xeptrb = eptrb;
  frame->Xflags = flags;

/* This is where control jumps back to to effect "recursion" */

HEAP_RECURSE:

/* Macros make the argument variables come from the current frame */

#define eptr               frame->Xeptr
#define ecode              frame->Xecode
#define offset_top         frame->Xoffset_top
#define ims                frame->Xims
#define eptrb              frame->Xeptrb
#define flags              frame->Xflags

/* Ditto for the local variables */

#define callpat            frame->Xcallpat
#define data               frame->Xdata
#define next               frame->Xnext
#define pp                 frame->Xpp
#define prev               frame->Xprev
#define saved_eptr         frame->Xsaved_eptr

#define new_recursive      frame->Xnew_recursive

#define cur_is_word        frame->Xcur_is_word
#define condition          frame->Xcondition
#define minimize           frame->Xminimize
#define prev_is_word       frame->Xprev_is_word

#define original_ims       frame->Xoriginal_ims

#ifdef SUPPORT_UCP
#define prop_type          frame->Xprop_type
#define prop_fail_result   frame->Xprop_fail_result
#define prop_category      frame->Xprop_category
#define prop_chartype      frame->Xprop_chartype
#define prop_othercase     frame->Xprop_othercase
#define prop_test_against  frame->Xprop_test_against
#define prop_test_variable frame->Xprop_test_variable
#endif

#define ctype              frame->Xctype
#define fc                 frame->Xfc
#define fi                 frame->Xfi
#define length             frame->Xlength
#define max                frame->Xmax
#define min                frame->Xmin
#define number             frame->Xnumber
#define offset             frame->Xoffset
#define op                 frame->Xop
#define save_capture_last  frame->Xsave_capture_last
#define save_offset1       frame->Xsave_offset1
#define save_offset2       frame->Xsave_offset2
#define save_offset3       frame->Xsave_offset3
#define stacksave          frame->Xstacksave

#define newptrb            frame->Xnewptrb

/* When recursion is being used, local variables are allocated on the stack and
get preserved during recursion in the normal way. In this environment, fi and
i, and fc and c, can be the same variables. */

#else
#define fi i
#define fc c


  const uschar *callpat;        /* them within each of those blocks.    */
  const uschar *data;           /* However, in order to accommodate the */
  const uschar *next;           /* version of this code that uses an    */
  const uschar *pp;             /* external "stack" implemented on the  */
  const uschar *prev;           /* heap, it is easier to declare them   */
  const uschar *saved_eptr;     /* all here, so the declarations can    */
  /* be cut out in a block. The only      */
  recursion_info new_recursive; /* declarations within blocks below are */
  /* for variables that do not have to    */
  BOOL cur_is_word;             /* be preserved over a recursive call   */
  BOOL condition;               /* to RMATCH().                         */
  BOOL minimize;
  BOOL prev_is_word;

  unsigned long int original_ims;

#ifdef SUPPORT_UCP
  int prop_type;
  int prop_fail_result;
  int prop_category;
  int prop_chartype;
  int prop_othercase;
  int prop_test_against;
  int *prop_test_variable;
#endif

  int ctype;
  int length;
  int max;
  int min;
  int number;
  int offset;
  int op;
  int save_capture_last;
  int save_offset1, save_offset2, save_offset3;
  int stacksave[REC_STACK_SAVE_MAX];

  eptrblock newptrb;
#endif

/* These statements are here to stop the compiler complaining about unitialized
variables. */

#ifdef SUPPORT_UCP
  prop_fail_result = 0;
  prop_test_against = 0;
  prop_test_variable = NULL;
#endif

/* OK, now we can get on with the real code of the function. Recursion is
specified by the macros RMATCH and RRETURN. When NO_RECURSE is *not* defined,
these just turn into a recursive call to match() and a "return", respectively.
However, RMATCH isn't like a function call because it's quite a complicated
macro. It has to be used in one particular way. This shouldn't, however, impact
performance when true recursion is being used. */

  if (md->match_call_count++ >= md->match_limit)
    RRETURN(PCRE_ERROR_MATCHLIMIT);

  original_ims = ims;           /* Save for resetting on ')' */
  utf8 = md->utf8;              /* Local copy of the flag */

/* At the start of a bracketed group, add the current subject pointer to the
stack of such pointers, to be re-instated at the end of the group when we hit
the closing ket. When match() is called in other circumstances, we don't add to
this stack. */

  if ((flags & match_isgroup) != 0) {
    newptrb.epb_prev = eptrb;
    newptrb.epb_saved_eptr = eptr;
    eptrb = &newptrb;
  }

/* Now start processing the operations. */

  for (;;) {
    op = *ecode;
    minimize = FALSE;

    /* For partial matching, remember if we ever hit the end of the subject after
       matching at least one subject character. */

    if (md->partial && eptr >= md->end_subject && eptr > md->start_match)
      md->hitend = TRUE;

    /* Opening capturing bracket. If there is space in the offset vector, save
       the current subject position in the working slot at the top of the vector. We
       mustn't change the current values of the data slot, because they may be set
       from a previous iteration of this group, and be referred to by a reference
       inside the group.

       If the bracket fails to match, we need to restore this value and also the
       values of the final offsets, in case they were set by a previous iteration of
       the same bracket.

       If there isn't enough space in the offset vector, treat this as if it were a
       non-capturing bracket. Don't worry about setting the flag for the error case
       here; that is handled in the code for KET. */

    if (op > OP_BRA) {
      number = op - OP_BRA;

      /* For extended extraction brackets (large number), we have to fish out the
         number from a dummy opcode at the start. */

      if (number > EXTRACT_BASIC_MAX)
        number = GET2(ecode, 2 + LINK_SIZE);
      offset = number << 1;


      if (offset < md->offset_max) {
        save_offset1 = md->offset_vector[offset];
        save_offset2 = md->offset_vector[offset + 1];
        save_offset3 = md->offset_vector[md->offset_end - number];
        save_capture_last = md->capture_last;

        md->offset_vector[md->offset_end - number] = eptr - md->start_subject;

        do {
          RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims, eptrb,
                 match_isgroup);
          if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);
          md->capture_last = save_capture_last;
          ecode += GET(ecode, 1);
        }
        while (*ecode == OP_ALT);


        md->offset_vector[offset] = save_offset1;
        md->offset_vector[offset + 1] = save_offset2;
        md->offset_vector[md->offset_end - number] = save_offset3;

        RRETURN(MATCH_NOMATCH);
      }

      /* Insufficient room for saving captured contents */

      else
        op = OP_BRA;
    }

    /* Other types of node can be handled by a switch */

    switch (op) {
    case OP_BRA:               /* Non-capturing bracket: optimized */
      do {
        RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims, eptrb,
               match_isgroup);
        if (rrc != MATCH_NOMATCH)
          RRETURN(rrc);
        ecode += GET(ecode, 1);
      }
      while (*ecode == OP_ALT);
      RRETURN(MATCH_NOMATCH);

      /* Conditional group: compilation checked that there are no more than
         two branches. If the condition is false, skipping the first branch takes us
         past the end if there is only one branch, but that's OK because that is
         exactly what going to the ket would do. */

    case OP_COND:
      if (ecode[LINK_SIZE + 1] == OP_CREF) {    /* Condition extract or recurse test */
        offset = GET2(ecode, LINK_SIZE + 2) << 1;       /* Doubled ref number */
        condition = (offset == CREF_RECURSE * 2) ?
          (md->recursive != NULL) :
          (offset < offset_top && md->offset_vector[offset] >= 0);
        RMATCH(rrc, eptr, ecode + (condition ?
                                   (LINK_SIZE + 4) : (LINK_SIZE + 1 +
                                                      GET(ecode, 1))),
               offset_top, md, ims, eptrb, match_isgroup);
        RRETURN(rrc);
      }

      /* The condition is an assertion. Call match() to evaluate it - setting
         the final argument TRUE causes it to stop at the end of an assertion. */

      else {
        RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims, NULL,
               match_condassert | match_isgroup);
        if (rrc == MATCH_MATCH) {
          ecode += 1 + LINK_SIZE + GET(ecode, LINK_SIZE + 2);
          while (*ecode == OP_ALT)
            ecode += GET(ecode, 1);
        } else if (rrc != MATCH_NOMATCH) {
          RRETURN(rrc);         /* Need braces because of following else */
        } else
          ecode += GET(ecode, 1);
        RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims, eptrb,
               match_isgroup);
        RRETURN(rrc);
      }
      /* Control never reaches here */

      /* Skip over conditional reference or large extraction number data if
         encountered. */

    case OP_CREF:
    case OP_BRANUMBER:
      ecode += 3;
      break;

      /* End of the pattern. If we are in a recursion, we should restore the
         offsets appropriately and continue from after the call. */

    case OP_END:
      if (md->recursive != NULL && md->recursive->group_num == 0) {
        recursion_info *rec = md->recursive;
        md->recursive = rec->prevrec;
        memmove(md->offset_vector, rec->offset_save,
                rec->saved_max * sizeof(int));
        md->start_match = rec->save_start;
        ims = original_ims;
        ecode = rec->after_call;
        break;
      }

      /* Otherwise, if PCRE_NOTEMPTY is set, fail if we have matched an empty
         string - backtracking will then try other alternatives, if any. */

      if (md->notempty && eptr == md->start_match)
        RRETURN(MATCH_NOMATCH);
      md->end_match_ptr = eptr; /* Record where we ended */
      md->end_offset_top = offset_top;  /* and how many extracts were taken */
      RRETURN(MATCH_MATCH);

      /* Change option settings */

    case OP_OPT:
      ims = ecode[1];
      ecode += 2;
      break;

      /* Assertion brackets. Check the alternative branches in turn - the
         matching won't pass the KET for an assertion. If any one branch matches,
         the assertion is true. Lookbehind assertions have an OP_REVERSE item at the
         start of each branch to move the current point backwards, so the code at
         this level is identical to the lookahead case. */

    case OP_ASSERT:
    case OP_ASSERTBACK:
      do {
        RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims, NULL,
               match_isgroup);
        if (rrc == MATCH_MATCH)
          break;
        if (rrc != MATCH_NOMATCH)
          RRETURN(rrc);
        ecode += GET(ecode, 1);
      }
      while (*ecode == OP_ALT);
      if (*ecode == OP_KET)
        RRETURN(MATCH_NOMATCH);

      /* If checking an assertion for a condition, return MATCH_MATCH. */

      if ((flags & match_condassert) != 0)
        RRETURN(MATCH_MATCH);

      /* Continue from after the assertion, updating the offsets high water
         mark, since extracts may have been taken during the assertion. */

      do
        ecode += GET(ecode, 1);
      while (*ecode == OP_ALT);
      ecode += 1 + LINK_SIZE;
      offset_top = md->end_offset_top;
      continue;

      /* Negative assertion: all branches must fail to match */

    case OP_ASSERT_NOT:
    case OP_ASSERTBACK_NOT:
      do {
        RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims, NULL,
               match_isgroup);
        if (rrc == MATCH_MATCH)
          RRETURN(MATCH_NOMATCH);
        if (rrc != MATCH_NOMATCH)
          RRETURN(rrc);
        ecode += GET(ecode, 1);
      }
      while (*ecode == OP_ALT);

      if ((flags & match_condassert) != 0)
        RRETURN(MATCH_MATCH);

      ecode += 1 + LINK_SIZE;
      continue;

      /* Move the subject pointer back. This occurs only at the start of
         each branch of a lookbehind assertion. If we are too close to the start to
         move back, this match function fails. When working with UTF-8 we move
         back a number of characters, not bytes. */

    case OP_REVERSE:

      /* No UTF-8 support, or not in UTF-8 mode: count is byte count */

      {
        eptr -= GET(ecode, 1);
        if (eptr < md->start_subject)
          RRETURN(MATCH_NOMATCH);
      }

      /* Skip to next op code */

      ecode += 1 + LINK_SIZE;
      break;

      /* The callout item calls an external function, if one is provided, passing
         details of the match so far. This is mainly for debugging, though the
         function is able to force a failure. */

    case OP_CALLOUT:
      if (pcre_callout != NULL) {
        pcre_callout_block cb;
        cb.version = 1;         /* Version 1 of the callout block */
        cb.callout_number = ecode[1];
        cb.offset_vector = md->offset_vector;
        cb.subject = (const char *) md->start_subject;
        cb.subject_length = md->end_subject - md->start_subject;
        cb.start_match = md->start_match - md->start_subject;
        cb.current_position = eptr - md->start_subject;
        cb.pattern_position = GET(ecode, 2);
        cb.next_item_length = GET(ecode, 2 + LINK_SIZE);
        cb.capture_top = offset_top / 2;
        cb.capture_last = md->capture_last;
        cb.callout_data = md->callout_data;
        if ((rrc = (*pcre_callout) (&cb)) > 0)
          RRETURN(MATCH_NOMATCH);
        if (rrc < 0)
          RRETURN(rrc);
      }
      ecode += 2 + 2 * LINK_SIZE;
      break;

      /* Recursion either matches the current regex, or some subexpression. The
         offset data is the offset to the starting bracket from the start of the
         whole pattern. (This is so that it works from duplicated subpatterns.)

         If there are any capturing brackets started but not finished, we have to
         save their starting points and reinstate them after the recursion. However,
         we don't know how many such there are (offset_top records the completed
         total) so we just have to save all the potential data. There may be up to
         65535 such values, which is too large to put on the stack, but using malloc
         for small numbers seems expensive. As a compromise, the stack is used when
         there are no more than REC_STACK_SAVE_MAX values to store; otherwise malloc
         is used. A problem is what to do if the malloc fails ... there is no way of
         returning to the top level with an error. Save the top REC_STACK_SAVE_MAX
         values on the stack, and accept that the rest may be wrong.

         There are also other values that have to be saved. We use a chained
         sequence of blocks that actually live on the stack. Thanks to Robin Houston
         for the original version of this logic. */

    case OP_RECURSE:
      {
        callpat = md->start_code + GET(ecode, 1);
        new_recursive.group_num = *callpat - OP_BRA;

        /* For extended extraction brackets (large number), we have to fish out
           the number from a dummy opcode at the start. */

        if (new_recursive.group_num > EXTRACT_BASIC_MAX)
          new_recursive.group_num = GET2(callpat, 2 + LINK_SIZE);

        /* Add to "recursing stack" */

        new_recursive.prevrec = md->recursive;
        md->recursive = &new_recursive;

        /* Find where to continue from afterwards */

        ecode += 1 + LINK_SIZE;
        new_recursive.after_call = ecode;

        /* Now save the offset data. */

        new_recursive.saved_max = md->offset_end;
        if (new_recursive.saved_max <= REC_STACK_SAVE_MAX)
          new_recursive.offset_save = stacksave;
        else {
          new_recursive.offset_save =
            (int *) malloc(new_recursive.saved_max * sizeof(int));
          if (new_recursive.offset_save == NULL)
            RRETURN(PCRE_ERROR_NOMEMORY);
        }

        memcpy(new_recursive.offset_save, md->offset_vector,
               new_recursive.saved_max * sizeof(int));
        new_recursive.save_start = md->start_match;
        md->start_match = eptr;

        /* OK, now we can do the recursion. For each top-level alternative we
           restore the offset and recursion data. */

        do {
          RMATCH(rrc, eptr, callpat + 1 + LINK_SIZE, offset_top, md, ims,
                 eptrb, match_isgroup);
          if (rrc == MATCH_MATCH) {
            md->recursive = new_recursive.prevrec;
            if (new_recursive.offset_save != stacksave)
              free(new_recursive.offset_save);
            RRETURN(MATCH_MATCH);
          } else if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);

          md->recursive = &new_recursive;
          memcpy(md->offset_vector, new_recursive.offset_save,
                 new_recursive.saved_max * sizeof(int));
          callpat += GET(callpat, 1);
        }
        while (*callpat == OP_ALT);

        md->recursive = new_recursive.prevrec;
        if (new_recursive.offset_save != stacksave)
          free(new_recursive.offset_save);
        RRETURN(MATCH_NOMATCH);
      }
      /* Control never reaches here */

      /* "Once" brackets are like assertion brackets except that after a match,
         the point in the subject string is not moved back. Thus there can never be
         a move back into the brackets. Friedl calls these "atomic" subpatterns.
         Check the alternative branches in turn - the matching won't pass the KET
         for this kind of subpattern. If any one branch matches, we carry on as at
         the end of a normal bracket, leaving the subject pointer. */

    case OP_ONCE:
      {
        prev = ecode;
        saved_eptr = eptr;

        do {
          RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims,
                 eptrb, match_isgroup);
          if (rrc == MATCH_MATCH)
            break;
          if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);
          ecode += GET(ecode, 1);
        }
        while (*ecode == OP_ALT);

        /* If hit the end of the group (which could be repeated), fail */

        if (*ecode != OP_ONCE && *ecode != OP_ALT)
          RRETURN(MATCH_NOMATCH);

        /* Continue as from after the assertion, updating the offsets high water
           mark, since extracts may have been taken. */

        do
          ecode += GET(ecode, 1);
        while (*ecode == OP_ALT);

        offset_top = md->end_offset_top;
        eptr = md->end_match_ptr;

        /* For a non-repeating ket, just continue at this level. This also
           happens for a repeating ket if no characters were matched in the group.
           This is the forcible breaking of infinite loops as implemented in Perl
           5.005. If there is an options reset, it will get obeyed in the normal
           course of events. */

        if (*ecode == OP_KET || eptr == saved_eptr) {
          ecode += 1 + LINK_SIZE;
          break;
        }

        /* The repeating kets try the rest of the pattern or restart from the
           preceding bracket, in the appropriate order. We need to reset any options
           that changed within the bracket before re-running it, so check the next
           opcode. */

        if (ecode[1 + LINK_SIZE] == OP_OPT) {
          ims = (ims & ~PCRE_IMS) | ecode[4];
        }

        if (*ecode == OP_KETRMIN) {
          RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims, eptrb,
                 0);
          if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);
          RMATCH(rrc, eptr, prev, offset_top, md, ims, eptrb, match_isgroup);
          if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);
        } else {                /* OP_KETRMAX */

          RMATCH(rrc, eptr, prev, offset_top, md, ims, eptrb, match_isgroup);
          if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);
          RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims, eptrb,
                 0);
          if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);
        }
      }
      RRETURN(MATCH_NOMATCH);

      /* An alternation is the end of a branch; scan along to find the end of the
         bracketed group and go to there. */

    case OP_ALT:
      do
        ecode += GET(ecode, 1);
      while (*ecode == OP_ALT);
      break;

      /* BRAZERO and BRAMINZERO occur just before a bracket group, indicating
         that it may occur zero times. It may repeat infinitely, or not at all -
         i.e. it could be ()* or ()? in the pattern. Brackets with fixed upper
         repeat limits are compiled as a number of copies, with the optional ones
         preceded by BRAZERO or BRAMINZERO. */

    case OP_BRAZERO:
      {
        next = ecode + 1;
        RMATCH(rrc, eptr, next, offset_top, md, ims, eptrb, match_isgroup);
        if (rrc != MATCH_NOMATCH)
          RRETURN(rrc);
        do
          next += GET(next, 1);
        while (*next == OP_ALT);
        ecode = next + 1 + LINK_SIZE;
      }
      break;

    case OP_BRAMINZERO:
      {
        next = ecode + 1;
        do
          next += GET(next, 1);
        while (*next == OP_ALT);
        RMATCH(rrc, eptr, next + 1 + LINK_SIZE, offset_top, md, ims, eptrb,
               match_isgroup);
        if (rrc != MATCH_NOMATCH)
          RRETURN(rrc);
        ecode++;
      }
      break;

      /* End of a group, repeated or non-repeating. If we are at the end of
         an assertion "group", stop matching and return MATCH_MATCH, but record the
         current high water mark for use by positive assertions. Do this also
         for the "once" (not-backup up) groups. */

    case OP_KET:
    case OP_KETRMIN:
    case OP_KETRMAX:
      {
        prev = ecode - GET(ecode, 1);
        saved_eptr = eptrb->epb_saved_eptr;

        /* Back up the stack of bracket start pointers. */

        eptrb = eptrb->epb_prev;

        if (*prev == OP_ASSERT || *prev == OP_ASSERT_NOT ||
            *prev == OP_ASSERTBACK || *prev == OP_ASSERTBACK_NOT ||
            *prev == OP_ONCE) {
          md->end_match_ptr = eptr;     /* For ONCE */
          md->end_offset_top = offset_top;
          RRETURN(MATCH_MATCH);
        }

        /* In all other cases except a conditional group we have to check the
           group number back at the start and if necessary complete handling an
           extraction by setting the offsets and bumping the high water mark. */

        if (*prev != OP_COND) {
          number = *prev - OP_BRA;

          /* For extended extraction brackets (large number), we have to fish out
             the number from a dummy opcode at the start. */

          if (number > EXTRACT_BASIC_MAX)
            number = GET2(prev, 2 + LINK_SIZE);
          offset = number << 1;

#ifdef DEBUG
          printf("end bracket %d", number);
          printf("\n");
#endif

          /* Test for a numbered group. This includes groups called as a result
             of recursion. Note that whole-pattern recursion is coded as a recurse
             into group 0, so it won't be picked up here. Instead, we catch it when
             the OP_END is reached. */

          if (number > 0) {
            md->capture_last = number;
            if (offset >= md->offset_max)
              md->offset_overflow = TRUE;
            else {
              md->offset_vector[offset] =
                md->offset_vector[md->offset_end - number];
              md->offset_vector[offset + 1] = eptr - md->start_subject;
              if (offset_top <= offset)
                offset_top = offset + 2;
            }

            /* Handle a recursively called group. Restore the offsets
               appropriately and continue from after the call. */

            if (md->recursive != NULL && md->recursive->group_num == number) {
              recursion_info *rec = md->recursive;
              md->recursive = rec->prevrec;
              md->start_match = rec->save_start;
              memcpy(md->offset_vector, rec->offset_save,
                     rec->saved_max * sizeof(int));
              ecode = rec->after_call;
              ims = original_ims;
              break;
            }
          }
        }

        /* Reset the value of the ims flags, in case they got changed during
           the group. */

        ims = original_ims;

        /* For a non-repeating ket, just continue at this level. This also
           happens for a repeating ket if no characters were matched in the group.
           This is the forcible breaking of infinite loops as implemented in Perl
           5.005. If there is an options reset, it will get obeyed in the normal
           course of events. */

        if (*ecode == OP_KET || eptr == saved_eptr) {
          ecode += 1 + LINK_SIZE;
          break;
        }

        /* The repeating kets try the rest of the pattern or restart from the
           preceding bracket, in the appropriate order. */

        if (*ecode == OP_KETRMIN) {
          RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims, eptrb,
                 0);
          if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);
          RMATCH(rrc, eptr, prev, offset_top, md, ims, eptrb, match_isgroup);
          if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);
        } else {                /* OP_KETRMAX */

          RMATCH(rrc, eptr, prev, offset_top, md, ims, eptrb, match_isgroup);
          if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);
          RMATCH(rrc, eptr, ecode + 1 + LINK_SIZE, offset_top, md, ims, eptrb,
                 0);
          if (rrc != MATCH_NOMATCH)
            RRETURN(rrc);
        }
      }

      RRETURN(MATCH_NOMATCH);

      /* Start of subject unless notbol, or after internal newline if multiline */

    case OP_CIRC:
      if (md->notbol && eptr == md->start_subject)
        RRETURN(MATCH_NOMATCH);
      if ((ims & PCRE_MULTILINE) != 0) {
        if (eptr != md->start_subject && eptr[-1] != NEWLINE)
          RRETURN(MATCH_NOMATCH);
        ecode++;
        break;
      }
      /* ... else fall through */

      /* Start of subject assertion */

    case OP_SOD:
      if (eptr != md->start_subject)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

      /* Start of match assertion */

    case OP_SOM:
      if (eptr != md->start_subject + md->start_offset)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

      /* Assert before internal newline if multiline, or before a terminating
         newline unless endonly is set, else end of subject unless noteol is set. */

    case OP_DOLL:
      if ((ims & PCRE_MULTILINE) != 0) {
        if (eptr < md->end_subject) {
          if (*eptr != NEWLINE)
            RRETURN(MATCH_NOMATCH);
        } else {
          if (md->noteol)
            RRETURN(MATCH_NOMATCH);
        }
        ecode++;
        break;
      } else {
        if (md->noteol)
          RRETURN(MATCH_NOMATCH);
        if (!md->endonly) {
          if (eptr < md->end_subject - 1 ||
              (eptr == md->end_subject - 1 && *eptr != NEWLINE))
            RRETURN(MATCH_NOMATCH);
          ecode++;
          break;
        }
      }
      /* ... else fall through */

      /* End of subject assertion (\z) */

    case OP_EOD:
      if (eptr < md->end_subject)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

      /* End of subject or ending \n assertion (\Z) */

    case OP_EODN:
      if (eptr < md->end_subject - 1 ||
          (eptr == md->end_subject - 1 && *eptr != NEWLINE))
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

      /* Word boundary assertions */

    case OP_NOT_WORD_BOUNDARY:
    case OP_WORD_BOUNDARY:
      {

        /* Find out if the previous and current characters are "word" characters.
           It takes a bit more work in UTF-8 mode. Characters > 255 are assumed to
           be "non-word" characters. */


        /* More streamlined when not in UTF-8 mode */

        {
          prev_is_word = (eptr != md->start_subject) &&
            ((md->ctypes[eptr[-1]] & ctype_word) != 0);
          cur_is_word = (eptr < md->end_subject) &&
            ((md->ctypes[*eptr] & ctype_word) != 0);
        }

        /* Now see if the situation is what we want */

        if ((*ecode++ == OP_WORD_BOUNDARY) ?
            cur_is_word == prev_is_word : cur_is_word != prev_is_word)
          RRETURN(MATCH_NOMATCH);
      }
      break;

      /* Match a single character type; inline for speed */

    case OP_ANY:
      if ((ims & PCRE_DOTALL) == 0 && eptr < md->end_subject
          && *eptr == NEWLINE)
        RRETURN(MATCH_NOMATCH);
      if (eptr++ >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

      /* Match a single byte, even in UTF-8 mode. This opcode really does match
         any byte, even newline, independent of the setting of PCRE_DOTALL. */

    case OP_ANYBYTE:
      if (eptr++ >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

    case OP_NOT_DIGIT:
      if (eptr >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      GETCHARINCTEST(c, eptr);
      if ((md->ctypes[c] & ctype_digit) != 0)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

    case OP_DIGIT:
      if (eptr >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      GETCHARINCTEST(c, eptr);
      if ((md->ctypes[c] & ctype_digit) == 0)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

    case OP_NOT_WHITESPACE:
      if (eptr >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      GETCHARINCTEST(c, eptr);
      if ((md->ctypes[c] & ctype_space) != 0)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

    case OP_WHITESPACE:
      if (eptr >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      GETCHARINCTEST(c, eptr);
      if ((md->ctypes[c] & ctype_space) == 0)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

    case OP_NOT_WORDCHAR:
      if (eptr >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      GETCHARINCTEST(c, eptr);
      if ((md->ctypes[c] & ctype_word) != 0)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

    case OP_WORDCHAR:
      if (eptr >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      GETCHARINCTEST(c, eptr);
      if ((md->ctypes[c] & ctype_word) == 0)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      break;

#ifdef SUPPORT_UCP
      /* Check the next character by Unicode property. We will get here only
         if the support is in the binary; otherwise a compile-time error occurs. */

    case OP_PROP:
    case OP_NOTPROP:
      if (eptr >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      GETCHARINCTEST(c, eptr);
      {
        int chartype, rqdtype;
        int othercase;
        int category = _pcre_ucp_findchar(c, &chartype, &othercase);

        rqdtype = *(++ecode);
        ecode++;

        if (rqdtype >= 128) {
          if ((rqdtype - 128 != category) == (op == OP_PROP))
            RRETURN(MATCH_NOMATCH);
        } else {
          if ((rqdtype != chartype) == (op == OP_PROP))
            RRETURN(MATCH_NOMATCH);
        }
      }
      break;

      /* Match an extended Unicode sequence. We will get here only if the support
         is in the binary; otherwise a compile-time error occurs. */

    case OP_EXTUNI:
      if (eptr >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      GETCHARINCTEST(c, eptr);
      {
        int chartype;
        int othercase;
        int category = _pcre_ucp_findchar(c, &chartype, &othercase);
        if (category == ucp_M)
          RRETURN(MATCH_NOMATCH);
        while (eptr < md->end_subject) {
          int len = 1;
          if (!utf8)
            c = *eptr;
          else {
            GETCHARLEN(c, eptr, len);
          }
          category = _pcre_ucp_findchar(c, &chartype, &othercase);
          if (category != ucp_M)
            break;
          eptr += len;
        }
      }
      ecode++;
      break;
#endif


      /* Match a back reference, possibly repeatedly. Look past the end of the
         item to see if there is repeat information following. The code is similar
         to that for character classes, but repeated for efficiency. Then obey
         similar code to character type repeats - written out again for speed.
         However, if the referenced string is the empty string, always treat
         it as matched, any number of times (otherwise there could be infinite
         loops). */

    case OP_REF:
      {
        offset = GET2(ecode, 1) << 1;   /* Doubled ref number */
        ecode += 3;             /* Advance past item */

        /* If the reference is unset, set the length to be longer than the amount
           of subject left; this ensures that every attempt at a match fails. We
           can't just fail here, because of the possibility of quantifiers with zero
           minima. */

        length = (offset >= offset_top || md->offset_vector[offset] < 0) ?
          md->end_subject - eptr + 1 :
          md->offset_vector[offset + 1] - md->offset_vector[offset];

        /* Set up for repetition, or handle the non-repeated case */

        switch (*ecode) {
        case OP_CRSTAR:
        case OP_CRMINSTAR:
        case OP_CRPLUS:
        case OP_CRMINPLUS:
        case OP_CRQUERY:
        case OP_CRMINQUERY:
          c = *ecode++ - OP_CRSTAR;
          minimize = (c & 1) != 0;
          min = rep_min[c];     /* Pick up values from tables; */
          max = rep_max[c];     /* zero for max => infinity */
          if (max == 0)
            max = INT_MAX;
          break;

        case OP_CRRANGE:
        case OP_CRMINRANGE:
          minimize = (*ecode == OP_CRMINRANGE);
          min = GET2(ecode, 1);
          max = GET2(ecode, 3);
          if (max == 0)
            max = INT_MAX;
          ecode += 5;
          break;

        default:               /* No repeat follows */
          if (!match_ref(offset, eptr, length, md, ims))
            RRETURN(MATCH_NOMATCH);
          eptr += length;
          continue;             /* With the main loop */
        }

        /* If the length of the reference is zero, just continue with the
           main loop. */

        if (length == 0)
          continue;

        /* First, ensure the minimum number of matches are present. We get back
           the length of the reference string explicitly rather than passing the
           address of eptr, so that eptr can be a register variable. */

        for (i = 1; i <= min; i++) {
          if (!match_ref(offset, eptr, length, md, ims))
            RRETURN(MATCH_NOMATCH);
          eptr += length;
        }

        /* If min = max, continue at the same level without recursion.
           They are not both allowed to be zero. */

        if (min == max)
          continue;

        /* If minimizing, keep trying and advancing the pointer */

        if (minimize) {
          for (fi = min;; fi++) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
            if (fi >= max || !match_ref(offset, eptr, length, md, ims))
              RRETURN(MATCH_NOMATCH);
            eptr += length;
          }
          /* Control never gets here */
        }

        /* If maximizing, find the longest string and work backwards */

        else {
          pp = eptr;
          for (i = min; i < max; i++) {
            if (!match_ref(offset, eptr, length, md, ims))
              break;
            eptr += length;
          }
          while (eptr >= pp) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
            eptr -= length;
          }
          RRETURN(MATCH_NOMATCH);
        }
      }
      /* Control never gets here */



      /* Match a bit-mapped character class, possibly repeatedly. This op code is
         used when all the characters in the class have values in the range 0-255,
         and either the matching is caseful, or the characters are in the range
         0-127 when UTF-8 processing is enabled. The only difference between
         OP_CLASS and OP_NCLASS occurs when a data character outside the range is
         encountered.

         First, look past the end of the item to see if there is repeat information
         following. Then obey similar code to character type repeats - written out
         again for speed. */

    case OP_NCLASS:
    case OP_CLASS:
      {
        data = ecode + 1;       /* Save for matching */
        ecode += 33;            /* Advance past the item */

        switch (*ecode) {
        case OP_CRSTAR:
        case OP_CRMINSTAR:
        case OP_CRPLUS:
        case OP_CRMINPLUS:
        case OP_CRQUERY:
        case OP_CRMINQUERY:
          c = *ecode++ - OP_CRSTAR;
          minimize = (c & 1) != 0;
          min = rep_min[c];     /* Pick up values from tables; */
          max = rep_max[c];     /* zero for max => infinity */
          if (max == 0)
            max = INT_MAX;
          break;

        case OP_CRRANGE:
        case OP_CRMINRANGE:
          minimize = (*ecode == OP_CRMINRANGE);
          min = GET2(ecode, 1);
          max = GET2(ecode, 3);
          if (max == 0)
            max = INT_MAX;
          ecode += 5;
          break;

        default:               /* No repeat follows */
          min = max = 1;
          break;
        }

        /* First, ensure the minimum number of matches are present. */

        /* Not UTF-8 mode */
        {
          for (i = 1; i <= min; i++) {
            if (eptr >= md->end_subject)
              RRETURN(MATCH_NOMATCH);
            c = *eptr++;
            if ((data[c / 8] & (1 << (c & 7))) == 0)
              RRETURN(MATCH_NOMATCH);
          }
        }

        /* If max == min we can continue with the main loop without the
           need to recurse. */

        if (min == max)
          continue;

        /* If minimizing, keep testing the rest of the expression and advancing
           the pointer while it matches the class. */

        if (minimize) {
          /* Not UTF-8 mode */
          {
            for (fi = min;; fi++) {
              RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
              if (rrc != MATCH_NOMATCH)
                RRETURN(rrc);
              if (fi >= max || eptr >= md->end_subject)
                RRETURN(MATCH_NOMATCH);
              c = *eptr++;
              if ((data[c / 8] & (1 << (c & 7))) == 0)
                RRETURN(MATCH_NOMATCH);
            }
          }
          /* Control never gets here */
        }

        /* If maximizing, find the longest possible run, then work backwards. */

        else {
          pp = eptr;

          /* Not UTF-8 mode */
          {
            for (i = min; i < max; i++) {
              if (eptr >= md->end_subject)
                break;
              c = *eptr;
              if ((data[c / 8] & (1 << (c & 7))) == 0)
                break;
              eptr++;
            }
            while (eptr >= pp) {
              RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
              eptr--;
              if (rrc != MATCH_NOMATCH)
                RRETURN(rrc);
            }
          }

          RRETURN(MATCH_NOMATCH);
        }
      }
      /* Control never gets here */


      /* Match an extended character class. This opcode is encountered only
         in UTF-8 mode, because that's the only time it is compiled. */


      /* Match a single character, casefully */

    case OP_CHAR:

      /* Non-UTF-8 mode */
      {
        if (md->end_subject - eptr < 1)
          RRETURN(MATCH_NOMATCH);
        if (ecode[1] != *eptr++)
          RRETURN(MATCH_NOMATCH);
        ecode += 2;
      }
      break;

      /* Match a single character, caselessly */

    case OP_CHARNC:

      /* Non-UTF-8 mode */
      {
        if (md->end_subject - eptr < 1)
          RRETURN(MATCH_NOMATCH);
        if (md->lcc[ecode[1]] != md->lcc[*eptr++])
          RRETURN(MATCH_NOMATCH);
        ecode += 2;
      }
      break;

      /* Match a single character repeatedly; different opcodes share code. */

    case OP_EXACT:
      min = max = GET2(ecode, 1);
      ecode += 3;
      goto REPEATCHAR;

    case OP_UPTO:
    case OP_MINUPTO:
      min = 0;
      max = GET2(ecode, 1);
      minimize = *ecode == OP_MINUPTO;
      ecode += 3;
      goto REPEATCHAR;

    case OP_STAR:
    case OP_MINSTAR:
    case OP_PLUS:
    case OP_MINPLUS:
    case OP_QUERY:
    case OP_MINQUERY:
      c = *ecode++ - OP_STAR;
      minimize = (c & 1) != 0;
      min = rep_min[c];         /* Pick up values from tables; */
      max = rep_max[c];         /* zero for max => infinity */
      if (max == 0)
        max = INT_MAX;

      /* Common code for all repeated single-character matches. We can give
         up quickly if there are fewer than the minimum number of characters left in
         the subject. */

    REPEATCHAR:

      /* When not in UTF-8 mode, load a single-byte character. */
      {
        if (min > md->end_subject - eptr)
          RRETURN(MATCH_NOMATCH);
        fc = *ecode++;
      }

      /* The value of fc at this point is always less than 256, though we may or
         may not be in UTF-8 mode. The code is duplicated for the caseless and
         caseful cases, for speed, since matching characters is likely to be quite
         common. First, ensure the minimum number of matches are present. If min =
         max, continue at the same level without recursing. Otherwise, if
         minimizing, keep trying the rest of the expression and advancing one
         matching character if failing, up to the maximum. Alternatively, if
         maximizing, find the maximum number of characters and work backwards. */


      if ((ims & PCRE_CASELESS) != 0) {
        fc = md->lcc[fc];
        for (i = 1; i <= min; i++)
          if (fc != md->lcc[*eptr++])
            RRETURN(MATCH_NOMATCH);
        if (min == max)
          continue;
        if (minimize) {
          for (fi = min;; fi++) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
            if (fi >= max || eptr >= md->end_subject || fc != md->lcc[*eptr++])
              RRETURN(MATCH_NOMATCH);
          }
          /* Control never gets here */
        } else {
          pp = eptr;
          for (i = min; i < max; i++) {
            if (eptr >= md->end_subject || fc != md->lcc[*eptr])
              break;
            eptr++;
          }
          while (eptr >= pp) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            eptr--;
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
          }
          RRETURN(MATCH_NOMATCH);
        }
        /* Control never gets here */
      }

      /* Caseful comparisons (includes all multi-byte characters) */

      else {
        for (i = 1; i <= min; i++)
          if (fc != *eptr++)
            RRETURN(MATCH_NOMATCH);
        if (min == max)
          continue;
        if (minimize) {
          for (fi = min;; fi++) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
            if (fi >= max || eptr >= md->end_subject || fc != *eptr++)
              RRETURN(MATCH_NOMATCH);
          }
          /* Control never gets here */
        } else {
          pp = eptr;
          for (i = min; i < max; i++) {
            if (eptr >= md->end_subject || fc != *eptr)
              break;
            eptr++;
          }
          while (eptr >= pp) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            eptr--;
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
          }
          RRETURN(MATCH_NOMATCH);
        }
      }
      /* Control never gets here */

      /* Match a negated single one-byte character. The character we are
         checking can be multibyte. */

    case OP_NOT:
      if (eptr >= md->end_subject)
        RRETURN(MATCH_NOMATCH);
      ecode++;
      GETCHARINCTEST(c, eptr);
      if ((ims & PCRE_CASELESS) != 0) {
        c = md->lcc[c];
        if (md->lcc[*ecode++] == c)
          RRETURN(MATCH_NOMATCH);
      } else {
        if (*ecode++ == c)
          RRETURN(MATCH_NOMATCH);
      }
      break;

      /* Match a negated single one-byte character repeatedly. This is almost a
         repeat of the code for a repeated single character, but I haven't found a
         nice way of commoning these up that doesn't require a test of the
         positive/negative option for each character match. Maybe that wouldn't add
         very much to the time taken, but character matching *is* what this is all
         about... */

    case OP_NOTEXACT:
      min = max = GET2(ecode, 1);
      ecode += 3;
      goto REPEATNOTCHAR;

    case OP_NOTUPTO:
    case OP_NOTMINUPTO:
      min = 0;
      max = GET2(ecode, 1);
      minimize = *ecode == OP_NOTMINUPTO;
      ecode += 3;
      goto REPEATNOTCHAR;

    case OP_NOTSTAR:
    case OP_NOTMINSTAR:
    case OP_NOTPLUS:
    case OP_NOTMINPLUS:
    case OP_NOTQUERY:
    case OP_NOTMINQUERY:
      c = *ecode++ - OP_NOTSTAR;
      minimize = (c & 1) != 0;
      min = rep_min[c];         /* Pick up values from tables; */
      max = rep_max[c];         /* zero for max => infinity */
      if (max == 0)
        max = INT_MAX;

      /* Common code for all repeated single-byte matches. We can give up quickly
         if there are fewer than the minimum number of bytes left in the
         subject. */

    REPEATNOTCHAR:
      if (min > md->end_subject - eptr)
        RRETURN(MATCH_NOMATCH);
      fc = *ecode++;

      /* The code is duplicated for the caseless and caseful cases, for speed,
         since matching characters is likely to be quite common. First, ensure the
         minimum number of matches are present. If min = max, continue at the same
         level without recursing. Otherwise, if minimizing, keep trying the rest of
         the expression and advancing one matching character if failing, up to the
         maximum. Alternatively, if maximizing, find the maximum number of
         characters and work backwards. */

      if ((ims & PCRE_CASELESS) != 0) {
        fc = md->lcc[fc];


        /* Not UTF-8 mode */
        {
          for (i = 1; i <= min; i++)
            if (fc == md->lcc[*eptr++])
              RRETURN(MATCH_NOMATCH);
        }

        if (min == max)
          continue;

        if (minimize) {
          /* Not UTF-8 mode */
          {
            for (fi = min;; fi++) {
              RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
              if (rrc != MATCH_NOMATCH)
                RRETURN(rrc);
              if (fi >= max || eptr >= md->end_subject
                  || fc == md->lcc[*eptr++])
                RRETURN(MATCH_NOMATCH);
            }
          }
          /* Control never gets here */
        }

        /* Maximize case */

        else {
          pp = eptr;

          /* Not UTF-8 mode */
          {
            for (i = min; i < max; i++) {
              if (eptr >= md->end_subject || fc == md->lcc[*eptr])
                break;
              eptr++;
            }
            while (eptr >= pp) {
              RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
              if (rrc != MATCH_NOMATCH)
                RRETURN(rrc);
              eptr--;
            }
          }

          RRETURN(MATCH_NOMATCH);
        }
        /* Control never gets here */
      }

      /* Caseful comparisons */

      else {
        /* Not UTF-8 mode */
        {
          for (i = 1; i <= min; i++)
            if (fc == *eptr++)
              RRETURN(MATCH_NOMATCH);
        }

        if (min == max)
          continue;

        if (minimize) {
          /* Not UTF-8 mode */
          {
            for (fi = min;; fi++) {
              RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
              if (rrc != MATCH_NOMATCH)
                RRETURN(rrc);
              if (fi >= max || eptr >= md->end_subject || fc == *eptr++)
                RRETURN(MATCH_NOMATCH);
            }
          }
          /* Control never gets here */
        }

        /* Maximize case */

        else {
          pp = eptr;

          /* Not UTF-8 mode */
          {
            for (i = min; i < max; i++) {
              if (eptr >= md->end_subject || fc == *eptr)
                break;
              eptr++;
            }
            while (eptr >= pp) {
              RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
              if (rrc != MATCH_NOMATCH)
                RRETURN(rrc);
              eptr--;
            }
          }

          RRETURN(MATCH_NOMATCH);
        }
      }
      /* Control never gets here */

      /* Match a single character type repeatedly; several different opcodes
         share code. This is very similar to the code for single characters, but we
         repeat it in the interests of efficiency. */

    case OP_TYPEEXACT:
      min = max = GET2(ecode, 1);
      minimize = TRUE;
      ecode += 3;
      goto REPEATTYPE;

    case OP_TYPEUPTO:
    case OP_TYPEMINUPTO:
      min = 0;
      max = GET2(ecode, 1);
      minimize = *ecode == OP_TYPEMINUPTO;
      ecode += 3;
      goto REPEATTYPE;

    case OP_TYPESTAR:
    case OP_TYPEMINSTAR:
    case OP_TYPEPLUS:
    case OP_TYPEMINPLUS:
    case OP_TYPEQUERY:
    case OP_TYPEMINQUERY:
      c = *ecode++ - OP_TYPESTAR;
      minimize = (c & 1) != 0;
      min = rep_min[c];         /* Pick up values from tables; */
      max = rep_max[c];         /* zero for max => infinity */
      if (max == 0)
        max = INT_MAX;

      /* Common code for all repeated single character type matches. Note that
         in UTF-8 mode, '.' matches a character of any length, but for the other
         character types, the valid characters are all one-byte long. */

    REPEATTYPE:
      ctype = *ecode++;         /* Code for the character type */

#ifdef SUPPORT_UCP
      if (ctype == OP_PROP || ctype == OP_NOTPROP) {
        prop_fail_result = ctype == OP_NOTPROP;
        prop_type = *ecode++;
        if (prop_type >= 128) {
          prop_test_against = prop_type - 128;
          prop_test_variable = &prop_category;
        } else {
          prop_test_against = prop_type;
          prop_test_variable = &prop_chartype;
        }
      } else
        prop_type = -1;
#endif

      /* First, ensure the minimum number of matches are present. Use inline
         code for maximizing the speed, and do the type test once at the start
         (i.e. keep it out of the loop). Also we can test that there are at least
         the minimum number of bytes before we start. This isn't as effective in
         UTF-8 mode, but it does no harm. Separate the UTF-8 code completely as that
         is tidier. Also separate the UCP code, which can be the same for both UTF-8
         and single-bytes. */

      if (min > md->end_subject - eptr)
        RRETURN(MATCH_NOMATCH);
      if (min > 0) {
#ifdef SUPPORT_UCP
        if (prop_type > 0) {
          for (i = 1; i <= min; i++) {
            GETCHARINC(c, eptr);
            prop_category =
              _pcre_ucp_findchar(c, &prop_chartype, &prop_othercase);
            if ((*prop_test_variable == prop_test_against) == prop_fail_result)
              RRETURN(MATCH_NOMATCH);
          }
        }

        /* Match extended Unicode sequences. We will get here only if the
           support is in the binary; otherwise a compile-time error occurs. */

        else if (ctype == OP_EXTUNI) {
          for (i = 1; i <= min; i++) {
            GETCHARINCTEST(c, eptr);
            prop_category =
              _pcre_ucp_findchar(c, &prop_chartype, &prop_othercase);
            if (prop_category == ucp_M)
              RRETURN(MATCH_NOMATCH);
            while (eptr < md->end_subject) {
              int len = 1;
              if (!utf8)
                c = *eptr;
              else {
                GETCHARLEN(c, eptr, len);
              }
              prop_category =
                _pcre_ucp_findchar(c, &prop_chartype, &prop_othercase);
              if (prop_category != ucp_M)
                break;
              eptr += len;
            }
          }
        }

        else
#endif                          /* SUPPORT_UCP */

/* Handle all other cases when the coding is UTF-8 */


          /* Code for the non-UTF-8 case for minimum matching of operators other
             than OP_PROP and OP_NOTPROP. */

          switch (ctype) {
          case OP_ANY:
            if ((ims & PCRE_DOTALL) == 0) {
              for (i = 1; i <= min; i++)
                if (*eptr++ == NEWLINE)
                  RRETURN(MATCH_NOMATCH);
            } else
              eptr += min;
            break;

          case OP_ANYBYTE:
            eptr += min;
            break;

          case OP_NOT_DIGIT:
            for (i = 1; i <= min; i++)
              if ((md->ctypes[*eptr++] & ctype_digit) != 0)
                RRETURN(MATCH_NOMATCH);
            break;

          case OP_DIGIT:
            for (i = 1; i <= min; i++)
              if ((md->ctypes[*eptr++] & ctype_digit) == 0)
                RRETURN(MATCH_NOMATCH);
            break;

          case OP_NOT_WHITESPACE:
            for (i = 1; i <= min; i++)
              if ((md->ctypes[*eptr++] & ctype_space) != 0)
                RRETURN(MATCH_NOMATCH);
            break;

          case OP_WHITESPACE:
            for (i = 1; i <= min; i++)
              if ((md->ctypes[*eptr++] & ctype_space) == 0)
                RRETURN(MATCH_NOMATCH);
            break;

          case OP_NOT_WORDCHAR:
            for (i = 1; i <= min; i++)
              if ((md->ctypes[*eptr++] & ctype_word) != 0)
                RRETURN(MATCH_NOMATCH);
            break;

          case OP_WORDCHAR:
            for (i = 1; i <= min; i++)
              if ((md->ctypes[*eptr++] & ctype_word) == 0)
                RRETURN(MATCH_NOMATCH);
            break;

          default:
            RRETURN(PCRE_ERROR_INTERNAL);
          }
      }

      /* If min = max, continue at the same level without recursing */

      if (min == max)
        continue;

      /* If minimizing, we have to test the rest of the pattern before each
         subsequent match. Again, separate the UTF-8 case for speed, and also
         separate the UCP cases. */

      if (minimize) {
#ifdef SUPPORT_UCP
        if (prop_type > 0) {
          for (fi = min;; fi++) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
            if (fi >= max || eptr >= md->end_subject)
              RRETURN(MATCH_NOMATCH);
            GETCHARINC(c, eptr);
            prop_category =
              _pcre_ucp_findchar(c, &prop_chartype, &prop_othercase);
            if ((*prop_test_variable == prop_test_against) == prop_fail_result)
              RRETURN(MATCH_NOMATCH);
          }
        }

        /* Match extended Unicode sequences. We will get here only if the
           support is in the binary; otherwise a compile-time error occurs. */

        else if (ctype == OP_EXTUNI) {
          for (fi = min;; fi++) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
            if (fi >= max || eptr >= md->end_subject)
              RRETURN(MATCH_NOMATCH);
            GETCHARINCTEST(c, eptr);
            prop_category =
              _pcre_ucp_findchar(c, &prop_chartype, &prop_othercase);
            if (prop_category == ucp_M)
              RRETURN(MATCH_NOMATCH);
            while (eptr < md->end_subject) {
              int len = 1;
              if (!utf8)
                c = *eptr;
              else {
                GETCHARLEN(c, eptr, len);
              }
              prop_category =
                _pcre_ucp_findchar(c, &prop_chartype, &prop_othercase);
              if (prop_category != ucp_M)
                break;
              eptr += len;
            }
          }
        }

        else
#endif                          /* SUPPORT_UCP */

          /* Not UTF-8 mode */
        {
          for (fi = min;; fi++) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
            if (fi >= max || eptr >= md->end_subject)
              RRETURN(MATCH_NOMATCH);
            c = *eptr++;
            switch (ctype) {
            case OP_ANY:
              if ((ims & PCRE_DOTALL) == 0 && c == NEWLINE)
                RRETURN(MATCH_NOMATCH);
              break;

            case OP_ANYBYTE:
              break;

            case OP_NOT_DIGIT:
              if ((md->ctypes[c] & ctype_digit) != 0)
                RRETURN(MATCH_NOMATCH);
              break;

            case OP_DIGIT:
              if ((md->ctypes[c] & ctype_digit) == 0)
                RRETURN(MATCH_NOMATCH);
              break;

            case OP_NOT_WHITESPACE:
              if ((md->ctypes[c] & ctype_space) != 0)
                RRETURN(MATCH_NOMATCH);
              break;

            case OP_WHITESPACE:
              if ((md->ctypes[c] & ctype_space) == 0)
                RRETURN(MATCH_NOMATCH);
              break;

            case OP_NOT_WORDCHAR:
              if ((md->ctypes[c] & ctype_word) != 0)
                RRETURN(MATCH_NOMATCH);
              break;

            case OP_WORDCHAR:
              if ((md->ctypes[c] & ctype_word) == 0)
                RRETURN(MATCH_NOMATCH);
              break;

            default:
              RRETURN(PCRE_ERROR_INTERNAL);
            }
          }
        }
        /* Control never gets here */
      }

      /* If maximizing it is worth using inline code for speed, doing the type
         test once at the start (i.e. keep it out of the loop). Again, keep the
         UTF-8 and UCP stuff separate. */

      else {
        pp = eptr;              /* Remember where we started */

#ifdef SUPPORT_UCP
        if (prop_type > 0) {
          for (i = min; i < max; i++) {
            int len = 1;
            if (eptr >= md->end_subject)
              break;
            GETCHARLEN(c, eptr, len);
            prop_category =
              _pcre_ucp_findchar(c, &prop_chartype, &prop_othercase);
            if ((*prop_test_variable == prop_test_against) == prop_fail_result)
              break;
            eptr += len;
          }

          /* eptr is now past the end of the maximum run */

          for (;;) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
            if (eptr-- == pp)
              break;            /* Stop if tried at original pos */
            BACKCHAR(eptr);
          }
        }

        /* Match extended Unicode sequences. We will get here only if the
           support is in the binary; otherwise a compile-time error occurs. */

        else if (ctype == OP_EXTUNI) {
          for (i = min; i < max; i++) {
            if (eptr >= md->end_subject)
              break;
            GETCHARINCTEST(c, eptr);
            prop_category =
              _pcre_ucp_findchar(c, &prop_chartype, &prop_othercase);
            if (prop_category == ucp_M)
              break;
            while (eptr < md->end_subject) {
              int len = 1;
              if (!utf8)
                c = *eptr;
              else {
                GETCHARLEN(c, eptr, len);
              }
              prop_category =
                _pcre_ucp_findchar(c, &prop_chartype, &prop_othercase);
              if (prop_category != ucp_M)
                break;
              eptr += len;
            }
          }

          /* eptr is now past the end of the maximum run */

          for (;;) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
            if (eptr-- == pp)
              break;            /* Stop if tried at original pos */
            for (;;) {          /* Move back over one extended */
              int len = 1;
              BACKCHAR(eptr);
              if (!utf8)
                c = *eptr;
              else {
                GETCHARLEN(c, eptr, len);
              }
              prop_category =
                _pcre_ucp_findchar(c, &prop_chartype, &prop_othercase);
              if (prop_category != ucp_M)
                break;
              eptr--;
            }
          }
        }

        else
#endif                          /* SUPPORT_UCP */


          /* Not UTF-8 mode */
        {
          switch (ctype) {
          case OP_ANY:
            if ((ims & PCRE_DOTALL) == 0) {
              for (i = min; i < max; i++) {
                if (eptr >= md->end_subject || *eptr == NEWLINE)
                  break;
                eptr++;
              }
              break;
            }
            /* For DOTALL case, fall through and treat as \C */

          case OP_ANYBYTE:
            c = max - min;
            if (c > md->end_subject - eptr)
              c = md->end_subject - eptr;
            eptr += c;
            break;

          case OP_NOT_DIGIT:
            for (i = min; i < max; i++) {
              if (eptr >= md->end_subject
                  || (md->ctypes[*eptr] & ctype_digit) != 0)
                break;
              eptr++;
            }
            break;

          case OP_DIGIT:
            for (i = min; i < max; i++) {
              if (eptr >= md->end_subject
                  || (md->ctypes[*eptr] & ctype_digit) == 0)
                break;
              eptr++;
            }
            break;

          case OP_NOT_WHITESPACE:
            for (i = min; i < max; i++) {
              if (eptr >= md->end_subject
                  || (md->ctypes[*eptr] & ctype_space) != 0)
                break;
              eptr++;
            }
            break;

          case OP_WHITESPACE:
            for (i = min; i < max; i++) {
              if (eptr >= md->end_subject
                  || (md->ctypes[*eptr] & ctype_space) == 0)
                break;
              eptr++;
            }
            break;

          case OP_NOT_WORDCHAR:
            for (i = min; i < max; i++) {
              if (eptr >= md->end_subject
                  || (md->ctypes[*eptr] & ctype_word) != 0)
                break;
              eptr++;
            }
            break;

          case OP_WORDCHAR:
            for (i = min; i < max; i++) {
              if (eptr >= md->end_subject
                  || (md->ctypes[*eptr] & ctype_word) == 0)
                break;
              eptr++;
            }
            break;

          default:
            RRETURN(PCRE_ERROR_INTERNAL);
          }

          /* eptr is now past the end of the maximum run */

          while (eptr >= pp) {
            RMATCH(rrc, eptr, ecode, offset_top, md, ims, eptrb, 0);
            eptr--;
            if (rrc != MATCH_NOMATCH)
              RRETURN(rrc);
          }
        }

        /* Get here if we can't make it match with any permitted repetitions */

        RRETURN(MATCH_NOMATCH);
      }
      /* Control never gets here */

      /* There's been some horrible disaster. Since all codes > OP_BRA are
         for capturing brackets, and there shouldn't be any gaps between 0 and
         OP_BRA, arrival here can only mean there is something seriously wrong
         in the code above or the OP_xxx definitions. */

    default:
      RRETURN(PCRE_ERROR_UNKNOWN_NODE);
    }

    /* Do not stick any code in here without much thought; it is assumed
       that "continue" in the code above comes out to here to repeat the main
       loop. */

  }                             /* End of main loop */
/* Control never reaches here */
}


/***************************************************************************
****************************************************************************
                   RECURSION IN THE match() FUNCTION

Undefine all the macros that were defined above to handle this. */

#ifdef NO_RECURSE
#undef eptr
#undef ecode
#undef offset_top
#undef ims
#undef eptrb
#undef flags

#undef callpat
#undef charptr
#undef data
#undef next
#undef pp
#undef prev
#undef saved_eptr

#undef new_recursive

#undef cur_is_word
#undef condition
#undef minimize
#undef prev_is_word

#undef original_ims

#undef ctype
#undef length
#undef max
#undef min
#undef number
#undef offset
#undef op
#undef save_capture_last
#undef save_offset1
#undef save_offset2
#undef save_offset3
#undef stacksave

#undef newptrb

#endif

/* These two are defined as macros in both cases */

#undef fc
#undef fi

/***************************************************************************
***************************************************************************/



/*************************************************
*         Execute a Regular Expression           *
*************************************************/

/* This function applies a compiled re to a subject string and picks out
portions of the string if it matches. Two elements in the vector are set for
each substring: the offsets to the start and end of the substring.

Arguments:
  argument_re     points to the compiled expression
  extra_data      points to extra data or is NULL
  subject         points to the subject string
  length          length of subject string (may contain binary zeros)
  start_offset    where to start in the subject string
  options         option bits
  offsets         points to a vector of ints to be filled in with offsets
  offsetcount     the number of elements in the vector

Returns:          > 0 => success; value is the number of elements filled in
                  = 0 => success, but offsets is not big enough
                   -1 => failed to match
                 < -1 => some kind of unexpected problem
*/

int
pcre_exec(const pcre * argument_re, const pcre_extra * extra_data,
          const char *subject, int length, int start_offset, int options,
          int *offsets, int offsetcount)
{
  int rc, resetcount, ocount;
  int first_byte = -1;
  int req_byte = -1;
  int req_byte2 = -1;
  unsigned long int ims = 0;
  BOOL using_temporary_offsets = FALSE;
  BOOL anchored;
  BOOL startline;
  BOOL firstline;
  BOOL first_byte_caseless = FALSE;
  BOOL req_byte_caseless = FALSE;
  match_data match_block;
  const uschar *tables;
  const uschar *start_bits = NULL;
  const uschar *start_match = (const uschar *) subject + start_offset;
  const uschar *end_subject;
  const uschar *req_byte_ptr = start_match - 1;

  pcre_study_data internal_study;
  const pcre_study_data *study;

  real_pcre internal_re;
  const real_pcre *external_re = (const real_pcre *) argument_re;
  const real_pcre *re = external_re;

/* Plausibility checks */

  if ((options & ~PUBLIC_EXEC_OPTIONS) != 0)
    return PCRE_ERROR_BADOPTION;
  if (re == NULL || subject == NULL || (offsets == NULL && offsetcount > 0))
    return PCRE_ERROR_NULL;
  if (offsetcount < 0)
    return PCRE_ERROR_BADCOUNT;

/* Fish out the optional data from the extra_data structure, first setting
the default values. */

  study = NULL;
  match_block.match_limit = MATCH_LIMIT;
  match_block.callout_data = NULL;

/* The table pointer is always in native byte order. */

  tables = external_re->tables;

  if (extra_data != NULL) {
    register unsigned int flags = extra_data->flags;
    if ((flags & PCRE_EXTRA_STUDY_DATA) != 0)
      study = (const pcre_study_data *) extra_data->study_data;
    if ((flags & PCRE_EXTRA_MATCH_LIMIT) != 0)
      match_block.match_limit = extra_data->match_limit;
    if ((flags & PCRE_EXTRA_CALLOUT_DATA) != 0)
      match_block.callout_data = extra_data->callout_data;
    if ((flags & PCRE_EXTRA_TABLES) != 0)
      tables = extra_data->tables;
  }

/* If the exec call supplied NULL for tables, use the inbuilt ones. This
is a feature that makes it possible to save compiled regex and re-use them
in other programs later. */

  if (tables == NULL)
    tables = _pcre_default_tables;

/* Check that the first field in the block is the magic number. If it is not,
test for a regex that was compiled on a host of opposite endianness. If this is
the case, flipped values are put in internal_re and internal_study if there was
study data too. */

  if (re->magic_number != MAGIC_NUMBER) {
    re = _pcre_try_flipped(re, &internal_re, study, &internal_study);
    if (re == NULL)
      return PCRE_ERROR_BADMAGIC;
    if (study != NULL)
      study = &internal_study;
  }

/* Set up other data */

  anchored = ((re->options | options) & PCRE_ANCHORED) != 0;
  startline = (re->options & PCRE_STARTLINE) != 0;
  firstline = (re->options & PCRE_FIRSTLINE) != 0;

/* The code starts after the real_pcre block and the capture name table. */

  match_block.start_code =
    (const uschar *) external_re + re->name_table_offset +
    re->name_count * re->name_entry_size;

  match_block.start_subject = (const uschar *) subject;
  match_block.start_offset = start_offset;
  match_block.end_subject = match_block.start_subject + length;
  end_subject = match_block.end_subject;

  match_block.endonly = (re->options & PCRE_DOLLAR_ENDONLY) != 0;
  match_block.utf8 = (re->options & PCRE_UTF8) != 0;

  match_block.notbol = (options & PCRE_NOTBOL) != 0;
  match_block.noteol = (options & PCRE_NOTEOL) != 0;
  match_block.notempty = (options & PCRE_NOTEMPTY) != 0;
  match_block.partial = (options & PCRE_PARTIAL) != 0;
  match_block.hitend = FALSE;

  match_block.recursive = NULL; /* No recursion at top level */

  match_block.lcc = tables + lcc_offset;
  match_block.ctypes = tables + ctypes_offset;

/* Partial matching is supported only for a restricted set of regexes at the
moment. */

  if (match_block.partial && (re->options & PCRE_NOPARTIAL) != 0)
    return PCRE_ERROR_BADPARTIAL;

/* Check a UTF-8 string if required. Unfortunately there's no way of passing
back the character offset. */


/* The ims options can vary during the matching as a result of the presence
of (?ims) items in the pattern. They are kept in a local variable so that
restoring at the exit of a group is easy. */

  ims = re->options & (PCRE_CASELESS | PCRE_MULTILINE | PCRE_DOTALL);

/* If the expression has got more back references than the offsets supplied can
hold, we get a temporary chunk of working store to use during the matching.
Otherwise, we can use the vector supplied, rounding down its size to a multiple
of 3. */

  ocount = offsetcount - (offsetcount % 3);

  if (re->top_backref > 0 && re->top_backref >= ocount / 3) {
    ocount = re->top_backref * 3 + 3;
    match_block.offset_vector = (int *) malloc(ocount * sizeof(int));
    if (match_block.offset_vector == NULL)
      return PCRE_ERROR_NOMEMORY;
    using_temporary_offsets = TRUE;
  } else
    match_block.offset_vector = offsets;

  match_block.offset_end = ocount;
  match_block.offset_max = (2 * ocount) / 3;
  match_block.offset_overflow = FALSE;
  match_block.capture_last = -1;

/* Compute the minimum number of offsets that we need to reset each time. Doing
this makes a huge difference to execution time when there aren't many brackets
in the pattern. */

  resetcount = 2 + re->top_bracket * 2;
  if (resetcount > offsetcount)
    resetcount = ocount;

/* Reset the working variable associated with each extraction. These should
never be used unless previously set, but they get saved and restored, and so we
initialize them to avoid reading uninitialized locations. */

  if (match_block.offset_vector != NULL) {
    register int *iptr = match_block.offset_vector + ocount;
    register int *iend = iptr - resetcount / 2 + 1;
    while (--iptr >= iend)
      *iptr = -1;
  }

/* Set up the first character to match, if available. The first_byte value is
never set for an anchored regular expression, but the anchoring may be forced
at run time, so we have to test for anchoring. The first char may be unset for
an unanchored pattern, of course. If there's no first char and the pattern was
studied, there may be a bitmap of possible first characters. */

  if (!anchored) {
    if ((re->options & PCRE_FIRSTSET) != 0) {
      first_byte = re->first_byte & 255;
      if ((first_byte_caseless =
           ((re->first_byte & REQ_CASELESS) != 0)) == TRUE)
        first_byte = match_block.lcc[first_byte];
    } else
      if (!startline && study != NULL &&
          (study->options & PCRE_STUDY_MAPPED) != 0)
      start_bits = study->start_bits;
  }

/* For anchored or unanchored matches, there may be a "last known required
character" set. */

  if ((re->options & PCRE_REQCHSET) != 0) {
    req_byte = re->req_byte & 255;
    req_byte_caseless = (re->req_byte & REQ_CASELESS) != 0;
    req_byte2 = (tables + fcc_offset)[req_byte];        /* case flipped */
  }

/* Loop for handling unanchored repeated matching attempts; for anchored regexs
the loop runs just once. */

  do {
    const uschar *save_end_subject = end_subject;

    /* Reset the maximum number of extractions we might see. */

    if (match_block.offset_vector != NULL) {
      register int *iptr = match_block.offset_vector;
      register int *iend = iptr + resetcount;
      while (iptr < iend)
        *iptr++ = -1;
    }

    /* Advance to a unique first char if possible. If firstline is TRUE, the
       start of the match is constrained to the first line of a multiline string.
       Implement this by temporarily adjusting end_subject so that we stop scanning
       at a newline. If the match fails at the newline, later code breaks this loop.
     */

    if (firstline) {
      const uschar *t = start_match;
      while (t < save_end_subject && *t != '\n')
        t++;
      end_subject = t;
    }

    /* Now test for a unique first byte */

    if (first_byte >= 0) {
      if (first_byte_caseless)
        while (start_match < end_subject &&
               match_block.lcc[*start_match] != first_byte)
          start_match++;
      else
        while (start_match < end_subject && *start_match != first_byte)
          start_match++;
    }

    /* Or to just after \n for a multiline match if possible */

    else if (startline) {
      if (start_match > match_block.start_subject + start_offset) {
        while (start_match < end_subject && start_match[-1] != NEWLINE)
          start_match++;
      }
    }

    /* Or to a non-unique first char after study */

    else if (start_bits != NULL) {
      while (start_match < end_subject) {
        register unsigned int c = *start_match;
        if ((start_bits[c / 8] & (1 << (c & 7))) == 0)
          start_match++;
        else
          break;
      }
    }

    /* Restore fudged end_subject */

    end_subject = save_end_subject;

#ifdef DEBUG                    /* Sigh. Some compilers never learn. */
    printf(">>>> Match against: ");
    pchars(start_match, end_subject - start_match, TRUE, &match_block);
    printf("\n");
#endif

    /* If req_byte is set, we know that that character must appear in the subject
       for the match to succeed. If the first character is set, req_byte must be
       later in the subject; otherwise the test starts at the match point. This
       optimization can save a huge amount of backtracking in patterns with nested
       unlimited repeats that aren't going to match. Writing separate code for
       cased/caseless versions makes it go faster, as does using an autoincrement
       and backing off on a match.

       HOWEVER: when the subject string is very, very long, searching to its end can
       take a long time, and give bad performance on quite ordinary patterns. This
       showed up when somebody was matching /^C/ on a 32-megabyte string... so we
       don't do this when the string is sufficiently long.

       ALSO: this processing is disabled when partial matching is requested.
     */

    if (req_byte >= 0 &&
        end_subject - start_match < REQ_BYTE_MAX && !match_block.partial) {
      register const uschar *p = start_match + ((first_byte >= 0) ? 1 : 0);

      /* We don't need to repeat the search if we haven't yet reached the
         place we found it at last time. */

      if (p > req_byte_ptr) {
        if (req_byte_caseless) {
          while (p < end_subject) {
            register int pp = *p++;
            if (pp == req_byte || pp == req_byte2) {
              p--;
              break;
            }
          }
        } else {
          while (p < end_subject) {
            if (*p++ == req_byte) {
              p--;
              break;
            }
          }
        }

        /* If we can't find the required character, break the matching loop */

        if (p >= end_subject)
          break;

        /* If we have found the required character, save the point where we
           found it, so that we don't search again next time round the loop if
           the start hasn't passed this character yet. */

        req_byte_ptr = p;
      }
    }

    /* When a match occurs, substrings will be set for all internal extractions;
       we just need to set up the whole thing as substring 0 before returning. If
       there were too many extractions, set the return code to zero. In the case
       where we had to get some local store to hold offsets for backreferences, copy
       those back references that we can. In this case there need not be overflow
       if certain parts of the pattern were not used. */

    match_block.start_match = start_match;
    match_block.match_call_count = 0;

    rc = match(start_match, match_block.start_code, 2, &match_block, ims, NULL,
               match_isgroup);

    /* When the result is no match, if the subject's first character was a
       newline and the PCRE_FIRSTLINE option is set, break (which will return
       PCRE_ERROR_NOMATCH). The option requests that a match occur before the first
       newline in the subject. Otherwise, advance the pointer to the next character
       and continue - but the continuation will actually happen only when the
       pattern is not anchored. */

    if (rc == MATCH_NOMATCH) {
      if (firstline && *start_match == NEWLINE)
        break;
      start_match++;
      continue;
    }

    if (rc != MATCH_MATCH) {
      return rc;
    }

    /* We have a match! Copy the offset information from temporary store if
       necessary */

    if (using_temporary_offsets) {
      if (offsetcount >= 4) {
        memcpy(offsets + 2, match_block.offset_vector + 2,
               (offsetcount - 2) * sizeof(int));
      }
      if (match_block.end_offset_top > offsetcount)
        match_block.offset_overflow = TRUE;

      free(match_block.offset_vector);
    }

    rc = match_block.offset_overflow ? 0 : match_block.end_offset_top / 2;

    if (offsetcount < 2)
      rc = 0;
    else {
      offsets[0] = start_match - match_block.start_subject;
      offsets[1] = match_block.end_match_ptr - match_block.start_subject;
    }

    return rc;
  }

/* This "while" is the end of the "do" above */

  while (!anchored && start_match <= end_subject);

  if (using_temporary_offsets) {
    free(match_block.offset_vector);
  }

  if (match_block.partial && match_block.hitend) {
    return PCRE_ERROR_PARTIAL;
  } else {
    return PCRE_ERROR_NOMATCH;
  }
}

/* End of pcre_exec.c */

#endif                          /* !HAVE_PCRE */

/** Return a default pcre_extra pointer pointing to a static region
    set up to use a fairly low match-limit setting.
*/
struct pcre_extra *
default_match_limit(void)
{
  static struct pcre_extra ex;
  memset(&ex, 0, sizeof ex);
  set_match_limit(&ex);
  return &ex;
}


/** Set a low match-limit setting in an existing pcre_extra struct. */
void
set_match_limit(struct pcre_extra *ex)
{
  if (!ex)
    return;
  ex->flags |= PCRE_EXTRA_MATCH_LIMIT;
  ex->match_limit = PENN_MATCH_LIMIT;
}
