/*
 * Requires a C99ish compiler. gcc 3 works. gcc 2.95 works. Earlier
 * versions might.
 *
 * The arrays below use designated initializers to make it very explicit
 * which elements are being set to what. The standard says that any elements
 * without an initalizer in these starts out like it would if static - in
 * other words, zero'ed out. That's usually what we wanted.
 *
 * However, since most people compiling Penn probably aren't going to be
 * using a C99 compiler for some time to come, this program will translate
 * from the DI form to the fully-initialized form that all C and C++ compilers
 * understand.
 *
 * Example Usage:
 * % cd pennmush
 * % gcc -o gentables utils/gentables.c
 * % ./gentables > src/tables.c
 * % make
 */

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

/* Offsets (+1) for q-register lookup. */
char q_offsets[UCHAR_MAX + 1] = {
  ['0'] = 1, ['1'] = 2, ['2'] = 3, ['3'] = 4, ['4'] = 5,
  ['5'] = 6, ['6'] = 7, ['7'] = 8, ['8'] = 9, ['9'] = 10,
  ['A'] = 11, ['a'] = 11,
  ['B'] = 12, ['b'] = 12,
  ['C'] = 13, ['c'] = 13,
  ['D'] = 14, ['d'] = 14,
  ['E'] = 15, ['e'] = 15,
  ['F'] = 16, ['f'] = 16,
  ['G'] = 17, ['g'] = 17,
  ['H'] = 18, ['h'] = 18,
  ['I'] = 19, ['i'] = 19,
  ['J'] = 20, ['j'] = 20,
  ['K'] = 21, ['k'] = 21,
  ['L'] = 22, ['l'] = 22,
  ['M'] = 23, ['m'] = 23,
  ['N'] = 24, ['n'] = 24,
  ['O'] = 25, ['o'] = 25,
  ['P'] = 26, ['p'] = 26,
  ['Q'] = 27, ['q'] = 27,
  ['R'] = 28, ['r'] = 28,
  ['S'] = 29, ['s'] = 29,
  ['T'] = 30, ['t'] = 30,
  ['U'] = 31, ['u'] = 31,
  ['V'] = 32, ['v'] = 32,
  ['W'] = 33, ['w'] = 33,
  ['X'] = 34, ['x'] = 34,
  ['Y'] = 35, ['y'] = 35,
  ['Z'] = 36, ['z'] = 36
};

/* What characters the parser looks for. */
char parse_interesting[UCHAR_MAX + 1] = {
  ['\0'] = 1,
  ['%'] = 1,
  ['{'] = 1,
  ['['] = 1,
  ['('] = 1,
  ['\\'] = 1,
  [' '] = 1,
  ['}'] = 1,
  ['>'] = 1,
  [']'] = 1,
  [')'] = 1,
  [','] = 1,
  [';'] = 1,
  ['='] = 1,
  ['$'] = 1,
  [0x1B] = 1
};

/* What characters are allowed in attribute names. */
char attribute_names[UCHAR_MAX + 1] = {
  ['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1,
  ['5'] = 1, ['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,
  ['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1,
  ['F'] = 1, ['G'] = 1, ['H'] = 1, ['I'] = 1, ['J'] = 1,
  ['K'] = 1, ['L'] = 1, ['M'] = 1, ['N'] = 1, ['O'] = 1,
  ['P'] = 1, ['Q'] = 1, ['R'] = 1, ['S'] = 1, ['T'] = 1,
  ['U'] = 1, ['V'] = 1, ['W'] = 1, ['X'] = 1, ['Y'] = 1,
  ['Z'] = 1, ['_'] = 1, ['#'] = 1, ['@'] = 1, ['$'] = 1,
  ['!'] = 1, ['~'] = 1, ['|'] = 1, [';'] = 1, ['`'] = 1,
  ['"'] = 1, ['\''] = 1,['&'] = 1, ['*'] = 1, ['-'] = 1,
  ['+'] = 1, ['='] = 1, ['?'] = 1, ['/'] = 1, ['.'] = 1,
  ['>'] = 1, ['<'] = 1, [','] = 1
};

/* C89 format codes for strftime() */
char valid_timefmt_codes[UCHAR_MAX + 1] = {
  ['a'] = 1, ['A'] = 1, ['b'] = 1, ['B'] = 1, ['c'] = 1,
  ['d'] = 1, ['H'] = 1, ['I'] = 1, ['j'] = 1, ['m'] = 1,
  ['M'] = 1, ['p'] = 1, ['S'] = 1, ['U'] = 1, ['w'] = 1,
  ['W'] = 1, ['x'] = 1, ['X'] = 1, ['y'] = 1, ['Y'] = 1,
  ['Z'] = 1, ['$'] = 1
};

/* Special characters for escape() and secure() */
char escaped_chars[UCHAR_MAX + 1] = {
  ['('] = 1, [')'] = 1, ['['] = 1, [']'] = 1, ['{'] = 1,
  ['}'] = 1, ['$'] = 1, ['^'] = 1, ['%'] = 1, [','] = 1,
  [';'] = 1, ['\\'] = 1
};
  

/* Color codes used in ansi markup */
char ansi_codes[UCHAR_MAX + 1] = {
  ['h'] = 1, ['i'] = 1, ['f'] = 1, ['u'] = 1, ['n'] = 1,
  ['x'] = 1, ['r'] = 1, ['g'] = 1, ['y'] = 1, ['b'] = 1, 
  ['c'] = 1, ['m'] = 1, ['w'] = 1,
  ['X'] = 1, ['R'] = 1, ['G'] = 1, ['Y'] = 1, ['B'] = 1, 
  ['C'] = 1, ['M'] = 1, ['W'] = 1,
  ['/'] = 1, ['a'] = 1
};

/* Values used in soundex hashing */
char soundex_codes[UCHAR_MAX + 1] = {
  ['B'] = 1, ['P'] = 1, ['F'] = 1, ['V'] = 1, ['b'] = 1, ['p'] = 1, ['f'] = 1, ['v'] = 1,
  ['C'] = 2, ['G'] = 2, ['J'] = 2, ['K'] = 2, ['Q'] = 2, ['S'] = 2, ['X'] = 2, ['Z'] = 2,
  ['c'] = 2, ['g'] = 2, ['j'] = 2, ['k'] = 2, ['q'] = 2, ['s'] = 2, ['x'] = 2, ['z'] = 2,
  ['D'] = 3, ['T'] = 3, ['d'] = 3, ['t'] = 3,
  ['L'] = 4, ['l'] = 4,
  ['M'] = 5, ['N'] = 5, ['m'] = 5, ['n'] = 5,
  ['R'] = 6, ['r'] = 6
};

/** Accented characters 
 *
 * The table is for ISO 8859-1 character set.
 *  It should be easy to modify it for other ISO 8859-X sets, or completely
 *  different families.
 */
typedef struct {
  const char *base;	/**< Base character */
  const char *entity;	/**< HTML entity */
} accent_info;
accent_info entity_table[UCHAR_MAX + 1] = {
  // Assorted characters 
  ['<'] = {"<", "&lt;"},
  ['>'] = {">", "&gt;"},
  ['&'] = {"&", "&amp;"},
  ['"'] = {"\\\"", "&quot;"},
  ['\n'] = {"\\n", "<br>\\n"},
  // << and >> quotes
  [171] = {"<<", "&laquo;"},
  [187] = {">>", "&raquo;"},
  // Upside-down punctuation
  [161] = {"!", "&iexcl;"},
  [191] = {"?", "&iquest;"},
  // szlig
  [223] = {"s", "&szlig;"},
  // thorn
  [222] = {"P", "&THORN;"},
  [254] = {"p", "&thorn:"},
  // eth
  [208] = {"D", "&ETH;"},
  [240] = {"o", "&eth;"},
  // Special symbols
  [169] = {"(c)", "&copy;"},
  [174] = {"(r)", "&reg;"},
  [188] = {"1/4", "&frac14;"},
  [189] = {"1/2", "&frac12;"},
  [190] = {"3/4", "&frac34;"},

    // AE ligatures
  [198] = {"AE", "&AElig;"},
  [230] = {"ae", "&aelig;"},

  // Accented a's 
  [192] = {"A", "&Agrave;"},
  [193] = {"A", "&Aacute;"},
  [194] = {"A", "&Acirc;"},
  [195] = {"A", "&Atilde;"},
  [196] = {"A", "&Auml;"},
  [197] = {"A", "&Aring;"},
  [224] = {"a", "&agrave;"},
  [225] = {"a", "&aacute;"},
  [226] = {"a", "&acirc;"},
  [227] = {"a", "&atilde;"},
  [228] = {"a", "&auml;"},
  [229] = {"a", "&aring;"},

  // Accented c's 
  [199] = {"C", "&Ccedil;"},
  [231] = {"c", "&ccedil;"},

  // Accented e's 
  [200] = {"E", "&Egrave;"},
  [201] = {"E", "&Eacute;"},
  [202] = {"E", "&Ecirc;"},
  [203] = {"E", "&Euml;"},
  [232] = {"e", "&egrave;"},
  [233] = {"e", "&eacute;"},
  [234] = {"e", "&ecirc;"},
  [235] = {"e", "&euml;"},

  // Accented i's 
  [204] = {"I", "&Igrave;"},
  [205] = {"I", "&Iacute;"},
  [206] = {"I", "&Icirc;"},
  [207] = {"I", "&Iuml;"},
  [236] = {"i", "&igrave;"},
  [237] = {"i", "&iacute;"},
  [238] = {"i", "&icirc;"},
  [239] = {"i", "&iuml;"},

  // Accented n's 
  [209] = {"N", "&Ntilde;"},
  [241] = {"n", "&ntilde;"},

  // Accented o's 
  [210] = {"O", "&Ograve;"},
  [211] = {"O", "&Oacute;"},
  [212] = {"O", "&Ocirc;"},
  [213] = {"O", "&Otilde;"},
  [214] = {"O", "&Ouml;"},
  [242] = {"o", "&ograve;"},
  [243] = {"o", "&oacute;"},
  [244] = {"o", "&ocirc;"},
  [245] = {"o", "&otilde;"},
  [246] = {"o", "&ouml;"},

  // Accented u's 
  [217] = {"U", "&Ugrave;"},
  [218] = {"U", "&Uacute;"},
  [219] = {"U", "&Ucirc;"},
  [220] = {"U", "&Uuml;"},
  [249] = {"u", "&ugrave;"},
  [250] = {"u", "&uacute;"},
  [251] = {"u", "&ucirc;"},
  [252] = {"u", "&uuml;"},

  // Accented y's 
  [221] = {"Y", "&Yacute;"},
  [253] = {"y", "&yacute;"},
  [255] = {"y", "&yuml;"},
};

/* For tables of char's treated as small numeric values. */
void print_table_bool(const char *type, const char *name,
                      char table[], int delta) {
  int n ;
  printf("%s %s[%d] = {\n", type, name, UCHAR_MAX + 1);
  for (n = 1; n < UCHAR_MAX + 2; n++) {
    printf("%3d", table[n - 1] + delta);
    if (n < UCHAR_MAX + 1)
      putchar(',');
    if (n % 16 == 0)
      putchar('\n');
  }
  fputs("};\n\n", stdout);
}

void print_entity_table(const char *name,
          const accent_info table[]) {
  int n;
  puts("typedef struct {");
  puts("const char *base;");
  puts("const char *entity;");
  puts("} accent_info;");
  printf("accent_info %s[%d] = {\n", name, UCHAR_MAX + 1);
  for (n = 0; n < UCHAR_MAX + 1; n++) {
    if (table[n].entity)
      printf("{\"%s\", \"%s\"}", table[n].base, table[n].entity);
    else
      fputs("{NULL, NULL}", stdout);
    if (n < UCHAR_MAX)
      putchar(',');
    putchar('\n');
  }
  fputs("};\n\n", stdout);
}



int main(int argc, char *argv[]) {
  printf("/* This file was generated by running %s compiled from\n"
        " * %s. Edit that file, not this one, when making changes. */\n"
        "#include <stdlib.h>\n\n",
         argv[0], __FILE__);
  // print_table_bool("signed char", "qreg_indexes", q_offsets, -1);
  print_table_bool("char", "active_table", parse_interesting, 0);
  print_table_bool("char", "atr_name_table", attribute_names, 0);
  print_table_bool("char", "valid_timefmt_codes", valid_timefmt_codes, 0);
  print_table_bool("char", "escaped_chars", escaped_chars, 0);
  print_table_bool("char", "valid_ansi_codes", ansi_codes, 0);
  print_table_bool("char", "soundex_val", soundex_codes, '0');
  print_entity_table("accent_table", entity_table);
  return EXIT_SUCCESS;
}
