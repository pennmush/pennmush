/**
 * \file ansi.h
 *
 * \brief ANSI control codes for various neat-o terminal effects
 *
 * \verbatim
 * Routines for dealing with ANSI and Pueblo, and the internal
 * markup system Penn uses to handle them.
 *
 * Some older versions of Ultrix don't appear to be able to
 * handle these escape sequences. If lowercase 'a's are being
 * stripped from @doings, and/or the output of the ANSI flag
 * is screwed up, you have the Ultrix problem.
 *
 * To fix the ANSI problem, try replacing the '\x1B' with '\033'.
 * To fix the problem with 'a's, replace all occurrences of '\a'
 * in the code with '\07'.
 *
 * \endverbatim
 */

#ifndef __ANSI_H
#define __ANSI_H

/* Doxygen does not like this */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
/* If we want to debug ansi stuff. */
/* #define ANSI_DEBUG /**/
#endif

#include "compile.h"
#include "mushtype.h"
#include "mypcre.h"
#include "strtree.h"

#define BEEP_CHAR '\a'
#define ESC_CHAR '\x1B'

#define ANSI_RAW_NORMAL "\x1B[0m"

#define TAG_START '\002'
#define TAG_END '\003'
#define MARKUP_START "\002"
#define MARKUP_END "\003"

#define ANSI_HILITE MARKUP_START "ch" MARKUP_END
#define ANSI_INVERSE MARKUP_START "ci" MARKUP_END
#define ANSI_BLINK MARKUP_START "cf" MARKUP_END
#define ANSI_UNDERSCORE MARKUP_START "cu" MARKUP_END

#define ANSI_INV_BLINK MARKUP_START "cfi" MARKUP_END
#define ANSI_INV_HILITE MARKUP_START "chi" MARKUP_END
#define ANSI_BLINK_HILITE MARKUP_START "cfh" MARKUP_END
#define ANSI_INV_BLINK_HILITE MARKUP_START "cifh" MARKUP_END

/* Foreground colors */

#define ANSI_PLAIN MARKUP_START "n" MARKUP_END
#define ANSI_BLACK MARKUP_START "cx" MARKUP_END
#define ANSI_RED MARKUP_START "cr" MARKUP_END
#define ANSI_GREEN MARKUP_START "cg" MARKUP_END
#define ANSI_YELLOW MARKUP_START "cy" MARKUP_END
#define ANSI_BLUE MARKUP_START "cb" MARKUP_END
#define ANSI_MAGENTA MARKUP_START "cm" MARKUP_END
#define ANSI_CYAN MARKUP_START "cc" MARKUP_END
#define ANSI_WHITE MARKUP_START "cw" MARKUP_END

#define ANSI_HIBLACK MARKUP_START "chx" MARKUP_END
#define ANSI_HIRED MARKUP_START "chr" MARKUP_END
#define ANSI_HIGREEN MARKUP_START "chg" MARKUP_END
#define ANSI_HIYELLOW MARKUP_START "chy" MARKUP_END
#define ANSI_HIBLUE MARKUP_START "chb" MARKUP_END
#define ANSI_HIMAGENTA MARKUP_START "chm" MARKUP_END
#define ANSI_HICYAN MARKUP_START "chc" MARKUP_END
#define ANSI_HIWHITE MARKUP_START "chw" MARKUP_END

/* Background colors */

#define ANSI_BBLACK MARKUP_START "cX" MARKUP_END
#define ANSI_BRED MARKUP_START "cR" MARKUP_END
#define ANSI_BGREEN MARKUP_START "cG" MARKUP_END
#define ANSI_BYELLOW MARKUP_START "cY" MARKUP_END
#define ANSI_BBLUE MARKUP_START "cB" MARKUP_END
#define ANSI_BMAGENTA MARKUP_START "cM" MARKUP_END
#define ANSI_BCYAN MARKUP_START "cC" MARKUP_END
#define ANSI_BWHITE MARKUP_START "cW" MARKUP_END

#define ANSI_END MARKUP_START "c/" MARKUP_END
#define ANSI_ENDALL MARKUP_START "c/a" MARKUP_END

#define ANSI_NORMAL ANSI_ENDALL

void init_ansi_codes(void);

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/** Maximum length of a color name (lightgoldenrodyellow) + '+' prefix and
 * trailing nul  */
#define COLOR_NAME_LEN 22
/** ANSI color data */
typedef struct _ansi_data {
  uint8_t bits;    /**< Bitwise CBIT_* flags which are explicitly on (underline,
                      flash, etc) */
  uint8_t offbits; /**< Bitwise CBIT_* flags which are explicitly off
                      (underline, flash, etc) */
  char fg[COLOR_NAME_LEN]; /**< FG color. May be a single character old-style
                              ANSI code or a modern color (+name, \#hex, etc) */
  char bg[COLOR_NAME_LEN]; /**< BG color. May be a single character old-style
                              ANSI code or a modern color (+name, \#hex, etc) */
} ansi_data;

#define NULL_ANSI                                                              \
  {                                                                            \
    0, 0, "", ""                                                               \
  }
#define HAS_ANSI(adata)                                                        \
  (adata.bits || adata.offbits || (adata.fg[0]) || (adata.bg[0]))
int read_raw_ansi_data(ansi_data *store, const char *codes);
int write_raw_ansi_data(ansi_data *old, ansi_data *cur, int ansi_format,
                        char *buff, char **bp);

/* Different ways of handling ANSI colors */
#define ANSI_FORMAT_NONE 0 /**< Strip all colors */
#define ANSI_FORMAT_HILITE                                                     \
  1 /**< Only show ANSI highlight, no colors/underline/etc */
#define ANSI_FORMAT_16COLOR                                                    \
  2 /**< Show the full basic ANSI palette, including highlight, underline, etc \
     */
#define ANSI_FORMAT_XTERM256 3 /**< Use the 256 color XTERM palette */
#define ANSI_FORMAT_HTML                                                       \
  4 /**< Show colors as HTML tags. Not currently used.                         \
     */

int define_ansi_data(ansi_data *store, const char *str);
int write_ansi_data(ansi_data *cur, char *buff, char **bp);
int write_ansi_close(char *buff, char **bp);

void nest_ansi_data(ansi_data *old, ansi_data *cur);

#define MARKUP_COLOR 'c'
#define MARKUP_COLOR_STR "c"
#define MARKUP_HTML 'p'
#define MARKUP_HTML_STR "p"
#define MARKUP_OLDANSI 'o'
#define MARKUP_OLDANSI_STR "o"

#define MARKUP_WS 'w'
#define MARKUP_WS_ALT 'W'
#define MARKUP_WS_ALT_END 'M'

/* Markup information necessary for ansi_string */

/* Miscellaneous notes on markup_information:
 * If "start" is negative, there are two cases:
 * end >= 0  :: A stand-alone tag, starting at "end".
 * end <  0  :: A tag set for removal.
 * If start is non-negative while end is negative, something's broken.
 *
 * Markup surrounding a character ends to the right of that character:
 * In the string "abc", if 'b' has a markup assigned to only itself,
 * start = 1, end = 2. (Instead of end = 1)
 */

/** Holds the markup information for an ansi_string struct */
typedef struct _new_markup_information {
  int parentIdx;                     /**< If this is nested, its parent */
  char type;                         /**< MARKUP_foo type. */
  char standalone;                   /**< If this is a standalone tag. */
  int start;                         /**< The start position of this tag.
                                      **  Only relevant for standalone tags. */
  const char *start_code, *end_code; /**< Markup start and end codes. */
  uint16_t idx; /**< For parse_ansi_string: Index of this mi */
} new_markup_information;

#define NOMARKUP (-1) /**< Character has no markup */

#define AS_OPTIMIZED 0x01  /**< If the markup has been optimized. */
#define AS_HAS_MARKUP 0x02 /**< If the string has markup or not */
#define AS_HAS_TAGS 0x04   /**< If the string has non-color tags. */
/** \verbatim
 * If the string has standalone tags (<IMG>, etc)
 * \endverbatim
 */
#define AS_HAS_STANDALONE 0x08

/** A string, with ansi attributes broken out from the text */
typedef struct _ansi_string {
  char text[BUFFER_LEN];      /**< Text of the string */
  int len;                    /**< Length of the string */
  char *source;               /**< Original source of string */
  uint32_t flags;             /**< Bitwise or of AS_<foo> flags */
  int16_t *markup;            /**< Indexes of markup, if it has any. */
  new_markup_information *mi; /**< Markup information */
  StrTree *tags;              /**< Tags. */
  int micount;                /**< # of used markup information in ->mi */
  int misize;                 /**< Size of the malloc in ->mi */
} ansi_string;

#define AS_Text(as) (as->text) /**< Raw text in an ansi_string */
#define AS_Len(as) (as->len)   /**< Length of the raw text in an ansi_string */
#define AS_IS(as, flag) (as->flags & flag)
#define AS_HasMarkup(as)                                                       \
  AS_IS(as, AS_HAS_MARKUP) /**< Does the ansi_string have markup */
#define AS_HasTags(as)                                                         \
  AS_IS(as, AS_HAS_TAGS) /**< Does the ansi_string have non-color tags */
#define AS_IsOptimized(as)                                                     \
  AS_IS(as, AS_OPTIMIZED) /**< Has the ansi_string been optimized */

int ansi_strcmp(const char *astr, const char *bstr);
char *remove_markup(const char *orig, size_t *stripped_len);
void sanitize_moniker(char *input, char *buff, char **bp);
char *skip_leading_ansi(const char *p, const char *bound);

int has_markup(const char *test);
ansi_string *parse_ansi_string(const char *src) __attribute_malloc__;
void flip_ansi_string(ansi_string *as);
void free_ansi_string(ansi_string *as);

/* Append X characters to the end of a string, taking ansi and html codes into
   account. */
int safe_ansi_string(ansi_string *as, int start, int len, char *buff,
                     char **bp);

/* Modifying ansi strings */
ansi_string *real_parse_ansi_string(const char *src) __attribute_malloc__;
int ansi_string_delete(ansi_string *as, int start, int count);
int ansi_string_insert(ansi_string *dst, int loc, ansi_string *src);
int ansi_string_replace(ansi_string *dst, int loc, int size, ansi_string *src);
void scramble_ansi_string(ansi_string *as);
void optimize_ansi_string(ansi_string *as);

/* Dump the penn code required to recreate the ansi_string */
extern int dump_ansi_string(ansi_string *as, char *buff, char **bp);

int ansi_pcre_copy_substring(ansi_string *as, pcre2_match_data *md,
                             int stringcount, int stringnumber, int nonempty,
                             char *buffer, char **bp);

int ansi_pcre_copy_named_substring(const pcre2_code *code, ansi_string *as,
                                   pcre2_match_data *md, int stringcount,
                                   const char *stringname, int nonempty,
                                   char *buffer, char **bp);

/* Pueblo stuff */
char *open_tag(const char *x);
char *close_tag(const char *x);
char *wrap_tag(const char *x, const char *y);

int safe_tag(char const *a_tag, char *buf, char **bp);
int safe_tag_cancel(char const *a_tag, char *buf, char **bp);
int safe_tag_wrap(char const *a_tag, char const *params, char const *data,
                  char *buf, char **bp, dbref player);

/* Walk through a string containing markup, skipping over the markup
 * (ansi/pueblo) codes */
#define WALK_ANSI_STRING(p) while ((p = skip_leading_ansi(p, NULL)) && *p)

int valid_color_name(const char *name);
uint32_t color_to_hex(const char *name, bool hilite);
int ansi_map_16(const char *name, bool bg, bool *hilite);
int ansi_map_256(const char *name, bool hilite, bool all);

#endif /* __ANSI_H */
