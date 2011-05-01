/* ansi.h */

/* ANSI control codes for various neat-o terminal effects

 * Some older versions of Ultrix don't appear to be able to
 * handle these escape sequences. If lowercase 'a's are being
 * stripped from @doings, and/or the output of the ANSI flag
 * is screwed up, you have the Ultrix problem.
 *
 * To fix the ANSI problem, try replacing the '\x1B' with '\033'.
 * To fix the problem with 'a's, replace all occurrences of '\a'
 * in the code with '\07'.
 *
 */

#ifndef __ANSI_H
#define __ANSI_H

#include "mushtype.h"
#include "mypcre.h"

#define BEEP_CHAR     '\a'
#define ESC_CHAR      '\x1B'

#define ANSI_RAW_NORMAL "\x1B[0m"

#define TAG_START     '\002'
#define TAG_END       '\003'
#define MARKUP_START     "\002"
#define MARKUP_END       "\003"

#define ANSI_HILITE      MARKUP_START "ch" MARKUP_END
#define ANSI_INVERSE     MARKUP_START "ci" MARKUP_END
#define ANSI_BLINK       MARKUP_START "cf" MARKUP_END
#define ANSI_UNDERSCORE  MARKUP_START "cu" MARKUP_END

#define ANSI_INV_BLINK         MARKUP_START "cfi" MARKUP_END
#define ANSI_INV_HILITE        MARKUP_START "chi" MARKUP_END
#define ANSI_BLINK_HILITE      MARKUP_START "cfh" MARKUP_END
#define ANSI_INV_BLINK_HILITE  MARKUP_START "cifh" MARKUP_END

/* Foreground colors */

#define ANSI_PLAIN      MARKUP_START "n" MARKUP_END
#define ANSI_BLACK      MARKUP_START "cx" MARKUP_END
#define ANSI_RED        MARKUP_START "cr" MARKUP_END
#define ANSI_GREEN      MARKUP_START "cg" MARKUP_END
#define ANSI_YELLOW     MARKUP_START "cy" MARKUP_END
#define ANSI_BLUE       MARKUP_START "cb" MARKUP_END
#define ANSI_MAGENTA    MARKUP_START "cm" MARKUP_END
#define ANSI_CYAN       MARKUP_START "cc" MARKUP_END
#define ANSI_WHITE      MARKUP_START "cw" MARKUP_END

#define ANSI_HIBLACK      MARKUP_START "chx" MARKUP_END
#define ANSI_HIRED        MARKUP_START "chr" MARKUP_END
#define ANSI_HIGREEN      MARKUP_START "chg" MARKUP_END
#define ANSI_HIYELLOW     MARKUP_START "chy" MARKUP_END
#define ANSI_HIBLUE       MARKUP_START "chb" MARKUP_END
#define ANSI_HIMAGENTA    MARKUP_START "chm" MARKUP_END
#define ANSI_HICYAN       MARKUP_START "chc" MARKUP_END
#define ANSI_HIWHITE      MARKUP_START "chw" MARKUP_END

/* Background colors */

#define ANSI_BBLACK     MARKUP_START "cX" MARKUP_END
#define ANSI_BRED       MARKUP_START "cR" MARKUP_END
#define ANSI_BGREEN     MARKUP_START "cG" MARKUP_END
#define ANSI_BYELLOW    MARKUP_START "cY" MARKUP_END
#define ANSI_BBLUE      MARKUP_START "cB" MARKUP_END
#define ANSI_BMAGENTA   MARKUP_START "cM" MARKUP_END
#define ANSI_BCYAN      MARKUP_START "cC" MARKUP_END
#define ANSI_BWHITE     MARKUP_START "cW" MARKUP_END

#define ANSI_END        MARKUP_START "c/" MARKUP_END
#define ANSI_ENDALL     MARKUP_START "c/a" MARKUP_END

#define ANSI_NORMAL     ANSI_ENDALL

void init_ansi_codes(void);

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

typedef struct _ansi_data {
  uint8_t bits;
  uint8_t offbits;
  char fore;
  char back;
} ansi_data;

#define HAS_ANSI(adata) (adata.bits || adata.offbits || adata.fore || adata.back)
int read_raw_ansi_data(ansi_data *store, const char *codes);
int write_raw_ansi_data(ansi_data *old, ansi_data *cur, char *buff, char **bp);

void define_ansi_data(ansi_data *store, const char *str);
int write_ansi_data(ansi_data *cur, char *buff, char **bp);
int write_ansi_close(char *buff, char **bp);

void nest_ansi_data(ansi_data *old, ansi_data *cur);

#define MARKUP_COLOR 'c'
#define MARKUP_COLOR_STR "c"
#define MARKUP_COLOR_OLD 'a'
#define MARKUP_HTML 'p'
#define MARKUP_HTML_STR "p"

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

typedef struct _markup_information {
  char *start_code;
  char *stop_code;
  char type;
  int start;
  int end;
  int priority;
} markup_information;

/** A string, with ansi attributes broken out from the text */
typedef struct _ansi_string {
  char text[BUFFER_LEN];        /**< Text of the string */
  ansi_data ansi[BUFFER_LEN];   /**< ANSI of the string */
  markup_information markup[BUFFER_LEN]; /**< The markup_information list */
  int nmarkups;         /**< Number of Pueblo markups */
  int len;              /**< Length of text */
  int optimized;              /**< Has this ansi_string been optimized? */
} ansi_string;

int ansi_strcmp(const char *astr, const char *bstr);
extern char *remove_markup(const char *orig, size_t * stripped_len);
extern char *skip_leading_ansi(const char *p);

int has_markup(const char *test);
extern ansi_string *
parse_ansi_string(const char *src)
  __attribute_malloc__;
    extern void flip_ansi_string(ansi_string *as);
    extern void free_ansi_string(ansi_string *as);

/* Append X characters to the end of a string, taking ansi and html codes into
   account. */
    extern int safe_ansi_string(ansi_string *as, int start, int len,
                                char *buff, char **bp);

/* Modifying ansi strings */
    ansi_string *real_parse_ansi_string(const char *src)
 __attribute_malloc__;
    int ansi_string_delete(ansi_string *as, int start, int count);
    int ansi_string_insert(ansi_string *dst, int loc, ansi_string *src);
    int ansi_string_replace(ansi_string *dst, int loc, int size,
                            ansi_string *src);
    ansi_string *scramble_ansi_string(ansi_string *as);
    void optimize_ansi_string(ansi_string *as);

/* Dump the penn code required to recreate the ansi_string */
    extern int dump_ansi_string(ansi_string *as, char *buff, char **bp);

    int ansi_pcre_copy_substring(ansi_string *as, int *ovector, int stringcount,
                                 int stringnumber, int nonempty, char *buffer,
                                 char **bp);

    int ansi_pcre_copy_named_substring(const pcre * code, ansi_string *as,
                                       int *ovector, int stringcount,
                                       const char *stringname, int nonempty,
                                       char *buffer, char **bp);

/* Pueblo stuff */
#define open_tag(x) tprintf("%c%c%s%c",TAG_START,MARKUP_HTML,x,TAG_END)
#define close_tag(x) tprintf("%c%c/%s%c",TAG_START,MARKUP_HTML,x,TAG_END)
#define wrap_tag(x,y) tprintf("%c%c%s%c%s%c%c/%s%c", \
                              TAG_START,MARKUP_HTML,x,TAG_END, \
                              y, TAG_START,MARKUP_HTML,x,TAG_END)
    int safe_tag(char const *a_tag, char *buf, char **bp);
    int safe_tag_cancel(char const *a_tag, char *buf, char **bp);
    int safe_tag_wrap(char const *a_tag, char const *params,
                      char const *data, char *buf, char **bp, dbref player);

/* Walk through a string containing markup, skipping over the markup (ansi/pueblo) codes */
#define WALK_ANSI_STRING(p) while ((p = skip_leading_ansi(p)) && *p)

#endif                          /* __ANSI_H */
