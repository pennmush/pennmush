/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: gperf -C --output-file rgbtab.c rgbtab.gperf  */
/* Computed positions: -k'1,3,5-8,12-13,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 24 "rgbtab.gperf"
struct RGB_COLORMAP;
/* maximum key range = 4354, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
  static unsigned int
colorname_hash(register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] = {
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 680, 0,
    5, 930, 910, 90, 70, 20, 300, 280, 500, 787,
    742, 92, 486, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 705, 855, 848,
    185, 0, 550, 15, 635, 285, 751, 240, 5, 0,
    105, 35, 245, 260, 125, 0, 60, 700, 763, 10,
    0, 75, 5, 0, 20, 4360, 115, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360, 4360,
    4360
  };
  register int hval = len;

  switch (hval) {
  default:
    hval += asso_values[(unsigned char) str[12]];
   /*FALLTHROUGH*/ case 12:
    hval += asso_values[(unsigned char) str[11]];
   /*FALLTHROUGH*/ case 11:
  case 10:
  case 9:
  case 8:
    hval += asso_values[(unsigned char) str[7]];
   /*FALLTHROUGH*/ case 7:
    hval += asso_values[(unsigned char) str[6] + 5];
   /*FALLTHROUGH*/ case 6:
    hval += asso_values[(unsigned char) str[5]];
   /*FALLTHROUGH*/ case 5:
    hval += asso_values[(unsigned char) str[4]];
   /*FALLTHROUGH*/ case 4:
  case 3:
    hval += asso_values[(unsigned char) str[2]];
   /*FALLTHROUGH*/ case 2:
  case 1:
    hval += asso_values[(unsigned char) str[0]];
    break;
  }
  return hval + asso_values[(unsigned char) str[len - 1]];
}

#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
  __attribute__ ((__gnu_inline__))
#endif
#endif
    const struct RGB_COLORMAP *colorname_lookup(register const char *str,
                                                register unsigned int len)
{
  enum {
    TOTAL_KEYWORDS = 921,
    MIN_WORD_LENGTH = 3,
    MAX_WORD_LENGTH = 20,
    MIN_HASH_VALUE = 6,
    MAX_HASH_VALUE = 4359
  };

  static const struct RGB_COLORMAP wordlist[] = {
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 27 "rgbtab.gperf"
    {"xterm1", 0x800000, 1, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 28 "rgbtab.gperf"
    {"xterm2", 0x008000, 2, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 349 "rgbtab.gperf"
    {"grey1", 0x030303, 16, 0},
#line 746 "rgbtab.gperf"
    {"grey11", 0x1c1c1c, 234, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 735 "rgbtab.gperf"
    {"gold1", 0xffd700, 220, 259},
#line 713 "rgbtab.gperf"
    {"grey21", 0x363636, 237, 256},
    {"", 0, 0, 0},
#line 147 "rgbtab.gperf"
    {"xterm121", 0x87ffaf, 121, 258},
    {"", 0, 0, 0},
#line 393 "rgbtab.gperf"
    {"grey2", 0x050505, 232, 0},
#line 357 "rgbtab.gperf"
    {"grey12", 0x1f1f1f, 234, 256},
#line 38 "rgbtab.gperf"
    {"xterm12", 0x0000ff, 12, 260},
#line 247 "rgbtab.gperf"
    {"xterm221", 0xffd75f, 221, 259},
    {"", 0, 0, 0},
#line 642 "rgbtab.gperf"
    {"gold2", 0xeec900, 220, 259},
#line 945 "rgbtab.gperf"
    {"grey22", 0x383838, 237, 256},
#line 48 "rgbtab.gperf"
    {"xterm22", 0x005f00, 22, 2},
#line 148 "rgbtab.gperf"
    {"xterm122", 0x87ffd7, 122, 263},
    {"", 0, 0, 0},
#line 431 "rgbtab.gperf"
    {"snow1", 0xfffafa, 231, 263},
#line 756 "rgbtab.gperf"
    {"grey71", 0xb5b5b5, 249, 263},
    {"", 0, 0, 0},
#line 248 "rgbtab.gperf"
    {"xterm222", 0xffd787, 222, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 33 "rgbtab.gperf"
    {"xterm7", 0xc0c0c0, 7, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 942 "rgbtab.gperf"
    {"snow", 0xfffafa, 231, 263},
#line 813 "rgbtab.gperf"
    {"snow2", 0xeee9e9, 255, 263},
#line 521 "rgbtab.gperf"
    {"grey72", 0xb8b8b8, 250, 263},
#line 98 "rgbtab.gperf"
    {"xterm72", 0x5faf87, 72, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 748 "rgbtab.gperf"
    {"grey7", 0x121212, 233, 0},
#line 671 "rgbtab.gperf"
    {"grey17", 0x2b2b2b, 235, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 705 "rgbtab.gperf"
    {"grey27", 0x454545, 238, 256},
    {"", 0, 0, 0},
#line 153 "rgbtab.gperf"
    {"xterm127", 0xaf00af, 127, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 253 "rgbtab.gperf"
    {"xterm227", 0xffff5f, 227, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 458 "rgbtab.gperf"
    {"wheat1", 0xffe7ba, 223, 263},
#line 37 "rgbtab.gperf"
    {"xterm11", 0xffff00, 11, 259},
#line 137 "rgbtab.gperf"
    {"xterm111", 0x87afff, 111, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 362 "rgbtab.gperf"
    {"grey77", 0xc4c4c4, 251, 263},
#line 47 "rgbtab.gperf"
    {"xterm21", 0x0000ff, 21, 260},
#line 237 "rgbtab.gperf"
    {"xterm211", 0xff87af, 211, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 806 "rgbtab.gperf"
    {"wheat2", 0xeed8ae, 223, 263},
    {"", 0, 0, 0},
#line 138 "rgbtab.gperf"
    {"xterm112", 0x87d700, 112, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 656 "rgbtab.gperf"
    {"grey61", 0x9c9c9c, 247, 7},
    {"", 0, 0, 0},
#line 238 "rgbtab.gperf"
    {"xterm212", 0xff87d7, 212, 261},
#line 834 "rgbtab.gperf"
    {"grey", 0xbebebe, 250, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 97 "rgbtab.gperf"
    {"xterm71", 0x5faf5f, 71, 2},
#line 127 "rgbtab.gperf"
    {"xterm101", 0x87875f, 101, 2},
    {"", 0, 0, 0},
#line 207 "rgbtab.gperf"
    {"xterm181", 0xd7afaf, 181, 261},
#line 693 "rgbtab.gperf"
    {"grey62", 0x9e9e9e, 247, 7},
#line 88 "rgbtab.gperf"
    {"xterm62", 0x5f5fd7, 62, 260},
#line 227 "rgbtab.gperf"
    {"xterm201", 0xff00ff, 201, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 128 "rgbtab.gperf"
    {"xterm102", 0x878787, 102, 2},
    {"", 0, 0, 0},
#line 208 "rgbtab.gperf"
    {"xterm182", 0xd7afd7, 182, 261},
#line 770 "rgbtab.gperf"
    {"grey51", 0x828282, 244, 7},
    {"", 0, 0, 0},
#line 228 "rgbtab.gperf"
    {"xterm202", 0xff5f00, 202, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 143 "rgbtab.gperf"
    {"xterm117", 0x87d7ff, 117, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 401 "rgbtab.gperf"
    {"grey52", 0x858585, 102, 2},
#line 78 "rgbtab.gperf"
    {"xterm52", 0x5f0000, 52, 1},
#line 243 "rgbtab.gperf"
    {"xterm217", 0xffafaf, 217, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 709 "rgbtab.gperf"
    {"green1", 0x00ff00, 46, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 914 "rgbtab.gperf"
    {"grey67", 0xababab, 248, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 363 "rgbtab.gperf"
    {"wheat", 0xf5deb3, 223, 263},
#line 593 "rgbtab.gperf"
    {"green2", 0x00ee00, 46, 258},
    {"", 0, 0, 0},
#line 133 "rgbtab.gperf"
    {"xterm107", 0x87af5f, 107, 2},
    {"", 0, 0, 0},
#line 213 "rgbtab.gperf"
    {"xterm187", 0xd7d7af, 187, 263},
#line 535 "rgbtab.gperf"
    {"yellow", 0xffff00, 226, 259},
    {"", 0, 0, 0},
#line 233 "rgbtab.gperf"
    {"xterm207", 0xff5fff, 207, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 32 "rgbtab.gperf"
    {"xterm6", 0x008080, 6, 6},
#line 87 "rgbtab.gperf"
    {"xterm61", 0x5f5faf, 61, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 604 "rgbtab.gperf"
    {"grey57", 0x919191, 246, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 441 "rgbtab.gperf"
    {"yellow2", 0xeeee00, 226, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 850 "rgbtab.gperf"
    {"grey6", 0x0f0f0f, 233, 0},
#line 758 "rgbtab.gperf"
    {"grey16", 0x292929, 235, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 594 "rgbtab.gperf"
    {"grey26", 0x424242, 238, 256},
#line 77 "rgbtab.gperf"
    {"xterm51", 0x00ffff, 51, 262},
#line 152 "rgbtab.gperf"
    {"xterm126", 0xaf0087, 126, 5},
#line 678 "rgbtab.gperf"
    {"tan1", 0xffa54f, 215, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 252 "rgbtab.gperf"
    {"xterm226", 0xffff00, 226, 259},
#line 795 "rgbtab.gperf"
    {"tan2", 0xee9a49, 209, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 419 "rgbtab.gperf"
    {"salmon2", 0xee8262, 209, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 287 "rgbtab.gperf"
    {"goldenrod1", 0xffc125, 214, 259},
#line 302 "rgbtab.gperf"
    {"grey76", 0xc2c2c2, 251, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 461 "rgbtab.gperf"
    {"goldenrod2", 0xeeb422, 214, 259},
#line 31 "rgbtab.gperf"
    {"xterm5", 0x800080, 5, 5},
#line 773 "rgbtab.gperf"
    {"tomato2", 0xee5c42, 203, 257},
    {"", 0, 0, 0},
#line 346 "rgbtab.gperf"
    {"lightgrey", 0xd3d3d3, 252, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 669 "rgbtab.gperf"
    {"tomato", 0xff6347, 203, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 449 "rgbtab.gperf"
    {"grey5", 0x0d0d0d, 232, 0},
#line 566 "rgbtab.gperf"
    {"grey15", 0x262626, 235, 256},
#line 716 "rgbtab.gperf"
    {"yellow1", 0xffff00, 226, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 452 "rgbtab.gperf"
    {"grey25", 0x404040, 238, 256},
    {"", 0, 0, 0},
#line 151 "rgbtab.gperf"
    {"xterm125", 0xaf005f, 125, 5},
#line 759 "rgbtab.gperf"
    {"gold", 0xffd700, 220, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 251 "rgbtab.gperf"
    {"xterm225", 0xffd7ff, 225, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 142 "rgbtab.gperf"
    {"xterm116", 0x87d7d7, 116, 262},
    {"", 0, 0, 0},
#line 848 "rgbtab.gperf"
    {"lightgreen", 0x90ee90, 120, 258},
#line 776 "rgbtab.gperf"
    {"grey75", 0xbfbfbf, 250, 263},
#line 883 "rgbtab.gperf"
    {"salmon1", 0xff8c69, 209, 257},
#line 242 "rgbtab.gperf"
    {"xterm216", 0xffaf87, 216, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 532 "rgbtab.gperf"
    {"green", 0x00ff00, 46, 258},
#line 379 "rgbtab.gperf"
    {"grey66", 0xa8a8a8, 248, 263},
#line 332 "rgbtab.gperf"
    {"tomato1", 0xff6347, 203, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 132 "rgbtab.gperf"
    {"xterm106", 0x87af00, 106, 2},
    {"", 0, 0, 0},
#line 212 "rgbtab.gperf"
    {"xterm186", 0xd7d787, 186, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 232 "rgbtab.gperf"
    {"xterm206", 0xff5fd7, 206, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 665 "rgbtab.gperf"
    {"grey56", 0x8f8f8f, 245, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 823 "rgbtab.gperf"
    {"salmon", 0xfa8072, 209, 257},
    {"", 0, 0, 0},
#line 141 "rgbtab.gperf"
    {"xterm115", 0x87d7af, 115, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 241 "rgbtab.gperf"
    {"xterm215", 0xffaf5f, 215, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 467 "rgbtab.gperf"
    {"mistyrose", 0xffe4e1, 224, 263},
#line 644 "rgbtab.gperf"
    {"mistyrose1", 0xffe4e1, 224, 263},
#line 308 "rgbtab.gperf"
    {"grey65", 0xa6a6a6, 248, 263},
    {"", 0, 0, 0},
#line 666 "rgbtab.gperf"
    {"tan", 0xd2b48c, 180, 259},
    {"", 0, 0, 0},
#line 715 "rgbtab.gperf"
    {"mistyrose2", 0xeed5d2, 224, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 131 "rgbtab.gperf"
    {"xterm105", 0x8787ff, 105, 260},
    {"", 0, 0, 0},
#line 211 "rgbtab.gperf"
    {"xterm185", 0xd7d75f, 185, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 231 "rgbtab.gperf"
    {"xterm205", 0xff5faf, 205, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 167 "rgbtab.gperf"
    {"xterm141", 0xaf87ff, 141, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 385 "rgbtab.gperf"
    {"grey55", 0x8c8c8c, 245, 7},
    {"", 0, 0, 0},
#line 267 "rgbtab.gperf"
    {"xterm241", 0x626262, 241, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 836 "rgbtab.gperf"
    {"maroon2", 0xee30a7, 205, 261},
#line 168 "rgbtab.gperf"
    {"xterm142", 0xafaf00, 142, 3},
    {"", 0, 0, 0},
#line 578 "rgbtab.gperf"
    {"white", 0xffffff, 231, 263},
#line 745 "rgbtab.gperf"
    {"grey91", 0xe8e8e8, 254, 263},
    {"", 0, 0, 0},
#line 268 "rgbtab.gperf"
    {"xterm242", 0x6c6c6c, 242, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 157 "rgbtab.gperf"
    {"xterm131", 0xaf5f5f, 131, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 285 "rgbtab.gperf"
    {"grey92", 0xebebeb, 255, 263},
#line 118 "rgbtab.gperf"
    {"xterm92", 0x8700d7, 92, 261},
#line 257 "rgbtab.gperf"
    {"xterm231", 0xffffff, 231, 263},
#line 640 "rgbtab.gperf"
    {"red1", 0xff0000, 196, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 158 "rgbtab.gperf"
    {"xterm132", 0xaf5f87, 132, 257},
#line 727 "rgbtab.gperf"
    {"red2", 0xee0000, 196, 257},
    {"", 0, 0, 0},
#line 348 "rgbtab.gperf"
    {"grey81", 0xcfcfcf, 252, 263},
    {"", 0, 0, 0},
#line 258 "rgbtab.gperf"
    {"xterm232", 0x080808, 232, 0},
    {"", 0, 0, 0},
#line 475 "rgbtab.gperf"
    {"linen", 0xfaf0e6, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 173 "rgbtab.gperf"
    {"xterm147", 0xafafff, 147, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 808 "rgbtab.gperf"
    {"grey82", 0xd1d1d1, 252, 263},
#line 108 "rgbtab.gperf"
    {"xterm82", 0x5fff00, 82, 258},
#line 273 "rgbtab.gperf"
    {"xterm247", 0x9e9e9e, 247, 7},
    {"", 0, 0, 0},
#line 314 "rgbtab.gperf"
    {"lightgoldenrod1", 0xffec8b, 228, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 494 "rgbtab.gperf"
    {"lightgoldenrod2", 0xeedc82, 222, 259},
#line 547 "rgbtab.gperf"
    {"grey97", 0xf7f7f7, 231, 263},
#line 373 "rgbtab.gperf"
    {"maroon1", 0xff34b3, 205, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 163 "rgbtab.gperf"
    {"xterm137", 0xaf875f, 137, 3},
    {"", 0, 0, 0},
#line 478 "rgbtab.gperf"
    {"lightgoldenrodyellow", 0xfafad2, 230, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 263 "rgbtab.gperf"
    {"xterm237", 0x3a3a3a, 237, 256},
    {"", 0, 0, 0},
#line 420 "rgbtab.gperf"
    {"pink1", 0xffb5c5, 218, 263},
    {"", 0, 0, 0},
#line 117 "rgbtab.gperf"
    {"xterm91", 0x8700af, 91, 5},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 502 "rgbtab.gperf"
    {"grey87", 0xdedede, 253, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 714 "rgbtab.gperf"
    {"goldenrod", 0xdaa520, 178, 3},
#line 575 "rgbtab.gperf"
    {"pink2", 0xeea9b8, 217, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 699 "rgbtab.gperf"
    {"yellowgreen", 0x9acd32, 113, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 455 "rgbtab.gperf"
    {"maroon", 0xb03060, 131, 257},
#line 107 "rgbtab.gperf"
    {"xterm81", 0x5fd7ff, 81, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 463 "rgbtab.gperf"
    {"purple", 0xa020f0, 129, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 736 "rgbtab.gperf"
    {"springgreen1", 0x00ff7f, 48, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 805 "rgbtab.gperf"
    {"springgreen2", 0x00ee76, 48, 258},
    {"", 0, 0, 0},
#line 44 "rgbtab.gperf"
    {"xterm18", 0x000087, 18, 4},
    {"", 0, 0, 0},
#line 922 "rgbtab.gperf"
    {"ivory1", 0xfffff0, 231, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 54 "rgbtab.gperf"
    {"xterm28", 0x008700, 28, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 613 "rgbtab.gperf"
    {"purple2", 0x912cee, 93, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 724 "rgbtab.gperf"
    {"powderblue", 0xb0e0e6, 152, 263},
#line 618 "rgbtab.gperf"
    {"ivory2", 0xeeeee0, 255, 263},
#line 846 "rgbtab.gperf"
    {"lightskyblue", 0x87cefa, 117, 262},
#line 851 "rgbtab.gperf"
    {"lightskyblue1", 0xb0e2ff, 153, 263},
    {"", 0, 0, 0},
#line 495 "rgbtab.gperf"
    {"ghostwhite", 0xf8f8ff, 231, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 104 "rgbtab.gperf"
    {"xterm78", 0x5fd787, 78, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 769 "rgbtab.gperf"
    {"lightskyblue2", 0xa4d3ee, 153, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 172 "rgbtab.gperf"
    {"xterm146", 0xafafd7, 146, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 272 "rgbtab.gperf"
    {"xterm246", 0x949494, 246, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 661 "rgbtab.gperf"
    {"grey96", 0xf5f5f5, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 859 "rgbtab.gperf"
    {"saddlebrown", 0x8b4513, 94, 2},
    {"", 0, 0, 0},
#line 162 "rgbtab.gperf"
    {"xterm136", 0xaf8700, 136, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 597 "rgbtab.gperf"
    {"purple1", 0x9b30ff, 99, 261},
#line 262 "rgbtab.gperf"
    {"xterm236", 0x303030, 236, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 479 "rgbtab.gperf"
    {"grey86", 0xdbdbdb, 253, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 625 "rgbtab.gperf"
    {"whitesmoke", 0xf5f5f5, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 171 "rgbtab.gperf"
    {"xterm145", 0xafafaf, 145, 261},
#line 94 "rgbtab.gperf"
    {"xterm68", 0x5f87d7, 68, 6},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 271 "rgbtab.gperf"
    {"xterm245", 0x8a8a8a, 245, 7},
    {"", 0, 0, 0},
#line 539 "rgbtab.gperf"
    {"ivory", 0xfffff0, 231, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 364 "rgbtab.gperf"
    {"grey95", 0xf2f2f2, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 161 "rgbtab.gperf"
    {"xterm135", 0xaf5fff, 135, 261},
#line 84 "rgbtab.gperf"
    {"xterm58", 0x5f5f00, 58, 2},
    {"", 0, 0, 0},
#line 444 "rgbtab.gperf"
    {"springgreen", 0x00ff7f, 48, 258},
    {"", 0, 0, 0},
#line 261 "rgbtab.gperf"
    {"xterm235", 0x262626, 235, 256},
#line 217 "rgbtab.gperf"
    {"xterm191", 0xd7ff5f, 191, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 655 "rgbtab.gperf"
    {"red", 0xff0000, 196, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 616 "rgbtab.gperf"
    {"grey85", 0xd9d9d9, 253, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 218 "rgbtab.gperf"
    {"xterm192", 0xd7ff87, 192, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 943 "rgbtab.gperf"
    {"dimgrey", 0x696969, 242, 7},
#line 177 "rgbtab.gperf"
    {"xterm151", 0xafd7af, 151, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 277 "rgbtab.gperf"
    {"xterm251", 0xc6c6c6, 251, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 178 "rgbtab.gperf"
    {"xterm152", 0xafd7d7, 152, 263},
#line 445 "rgbtab.gperf"
    {"lightgoldenrod", 0xeedd82, 222, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 278 "rgbtab.gperf"
    {"xterm252", 0xd0d0d0, 252, 263},
    {"", 0, 0, 0},
#line 696 "rgbtab.gperf"
    {"dodgerblue", 0x1e90ff, 33, 260},
#line 902 "rgbtab.gperf"
    {"dodgerblue1", 0x1e90ff, 33, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 809 "rgbtab.gperf"
    {"dodgerblue2", 0x1c86ee, 33, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 223 "rgbtab.gperf"
    {"xterm197", 0xff005f, 197, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 359 "rgbtab.gperf"
    {"lightpink1", 0xffaeb9, 217, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 183 "rgbtab.gperf"
    {"xterm157", 0xafffaf, 157, 259},
    {"", 0, 0, 0},
#line 488 "rgbtab.gperf"
    {"lightpink2", 0xeea2ad, 217, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 886 "rgbtab.gperf"
    {"midnightblue", 0x191970, 17, 4},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 35 "rgbtab.gperf"
    {"xterm9", 0xff0000, 9, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 286 "rgbtab.gperf"
    {"grey9", 0x171717, 233, 0},
#line 899 "rgbtab.gperf"
    {"grey19", 0x303030, 236, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 641 "rgbtab.gperf"
    {"grey29", 0x4a4a4a, 239, 256},
    {"", 0, 0, 0},
#line 155 "rgbtab.gperf"
    {"xterm129", 0xaf00ff, 129, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 255 "rgbtab.gperf"
    {"xterm229", 0xffffaf, 229, 259},
#line 511 "rgbtab.gperf"
    {"pink", 0xffc0cb, 218, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 41 "rgbtab.gperf"
    {"xterm15", 0xffffff, 15, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 606 "rgbtab.gperf"
    {"grey79", 0xc9c9c9, 251, 263},
#line 51 "rgbtab.gperf"
    {"xterm25", 0x005faf, 25, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 34 "rgbtab.gperf"
    {"xterm8", 0x808080, 8, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 101 "rgbtab.gperf"
    {"xterm75", 0x5fafff, 75, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 369 "rgbtab.gperf"
    {"grey8", 0x141414, 233, 0},
#line 632 "rgbtab.gperf"
    {"grey18", 0x2e2e2e, 236, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 622 "rgbtab.gperf"
    {"grey28", 0x474747, 238, 256},
    {"", 0, 0, 0},
#line 154 "rgbtab.gperf"
    {"xterm128", 0xaf00d7, 128, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 254 "rgbtab.gperf"
    {"xterm228", 0xffff87, 228, 259},
#line 222 "rgbtab.gperf"
    {"xterm196", 0xff0000, 196, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 145 "rgbtab.gperf"
    {"xterm119", 0x87ff5f, 119, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 503 "rgbtab.gperf"
    {"grey78", 0xc7c7c7, 251, 263},
    {"", 0, 0, 0},
#line 245 "rgbtab.gperf"
    {"xterm219", 0xffafff, 219, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 683 "rgbtab.gperf"
    {"lightsalmon1", 0xffa07a, 216, 263},
#line 182 "rgbtab.gperf"
    {"xterm156", 0xafff87, 156, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 630 "rgbtab.gperf"
    {"grey69", 0xb0b0b0, 145, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 561 "rgbtab.gperf"
    {"lightsalmon2", 0xee9572, 209, 257},
#line 135 "rgbtab.gperf"
    {"xterm109", 0x87afaf, 109, 6},
    {"", 0, 0, 0},
#line 215 "rgbtab.gperf"
    {"xterm189", 0xd7d7ff, 189, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 235 "rgbtab.gperf"
    {"xterm209", 0xff875f, 209, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 91 "rgbtab.gperf"
    {"xterm65", 0x5f875f, 65, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 638 "rgbtab.gperf"
    {"grey59", 0x969696, 246, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 221 "rgbtab.gperf"
    {"xterm195", 0xd7ffff, 195, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 144 "rgbtab.gperf"
    {"xterm118", 0x87ff00, 118, 258},
#line 124 "rgbtab.gperf"
    {"xterm98", 0x875fd7, 98, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 244 "rgbtab.gperf"
    {"xterm218", 0xffafd7, 218, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 81 "rgbtab.gperf"
    {"xterm55", 0x5f00af, 55, 260},
#line 181 "rgbtab.gperf"
    {"xterm155", 0xafff5f, 155, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 658 "rgbtab.gperf"
    {"grey68", 0xadadad, 145, 261},
    {"", 0, 0, 0},
#line 281 "rgbtab.gperf"
    {"xterm255", 0xeeeeee, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 134 "rgbtab.gperf"
    {"xterm108", 0x87af87, 108, 2},
#line 114 "rgbtab.gperf"
    {"xterm88", 0x870000, 88, 1},
#line 214 "rgbtab.gperf"
    {"xterm188", 0xd7d7d7, 188, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 234 "rgbtab.gperf"
    {"xterm208", 0xff8700, 208, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 723 "rgbtab.gperf"
    {"grey58", 0x949494, 246, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 645 "rgbtab.gperf"
    {"gray1", 0x030303, 16, 0},
#line 932 "rgbtab.gperf"
    {"gray11", 0x1c1c1c, 234, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 654 "rgbtab.gperf"
    {"gray21", 0x363636, 237, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 804 "rgbtab.gperf"
    {"gray2", 0x050505, 232, 0},
#line 793 "rgbtab.gperf"
    {"gray12", 0x1f1f1f, 234, 256},
    {"", 0, 0, 0},
#line 311 "rgbtab.gperf"
    {"magenta1", 0xff00ff, 201, 261},
    {"", 0, 0, 0},
#line 292 "rgbtab.gperf"
    {"slategrey1", 0xc6e2ff, 189, 263},
#line 774 "rgbtab.gperf"
    {"gray22", 0x383838, 237, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 407 "rgbtab.gperf"
    {"slategrey2", 0xb9d3ee, 153, 263},
#line 648 "rgbtab.gperf"
    {"gray71", 0xb5b5b5, 249, 263},
    {"", 0, 0, 0},
#line 791 "rgbtab.gperf"
    {"magenta2", 0xee00ee, 201, 261},
    {"", 0, 0, 0},
#line 197 "rgbtab.gperf"
    {"xterm171", 0xd75fff, 171, 261},
#line 856 "rgbtab.gperf"
    {"lightsalmon", 0xffa07a, 216, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 447 "rgbtab.gperf"
    {"gray72", 0xb8b8b8, 250, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 198 "rgbtab.gperf"
    {"xterm172", 0xd78700, 172, 3},
#line 291 "rgbtab.gperf"
    {"orange", 0xffa500, 214, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 453 "rgbtab.gperf"
    {"gray7", 0x121212, 233, 0},
#line 327 "rgbtab.gperf"
    {"gray17", 0x2b2b2b, 235, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 43 "rgbtab.gperf"
    {"xterm17", 0x00005f, 17, 4},
    {"", 0, 0, 0},
#line 631 "rgbtab.gperf"
    {"gray27", 0x454545, 238, 256},
    {"", 0, 0, 0},
#line 45 "rgbtab.gperf"
    {"xterm19", 0x0000af, 19, 4},
#line 53 "rgbtab.gperf"
    {"xterm27", 0x005fff, 27, 260},
#line 920 "rgbtab.gperf"
    {"orangered1", 0xff4500, 202, 257},
    {"", 0, 0, 0},
#line 36 "rgbtab.gperf"
    {"xterm10", 0x00ff00, 10, 258},
#line 55 "rgbtab.gperf"
    {"xterm29", 0x00875f, 29, 2},
    {"", 0, 0, 0},
#line 366 "rgbtab.gperf"
    {"orangered2", 0xee4000, 202, 257},
    {"", 0, 0, 0},
#line 46 "rgbtab.gperf"
    {"xterm20", 0x0000d7, 20, 260},
    {"", 0, 0, 0},
#line 504 "rgbtab.gperf"
    {"lightpink", 0xffb6c1, 217, 263},
    {"", 0, 0, 0},
#line 620 "rgbtab.gperf"
    {"gray77", 0xc4c4c4, 251, 263},
#line 761 "rgbtab.gperf"
    {"orange2", 0xee9a00, 208, 257},
    {"", 0, 0, 0},
#line 103 "rgbtab.gperf"
    {"xterm77", 0x5fd75f, 77, 258},
#line 203 "rgbtab.gperf"
    {"xterm177", 0xd787ff, 177, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 105 "rgbtab.gperf"
    {"xterm79", 0x5fd7af, 79, 262},
    {"", 0, 0, 0},
#line 187 "rgbtab.gperf"
    {"xterm161", 0xd7005f, 161, 257},
#line 587 "rgbtab.gperf"
    {"gray61", 0x9c9c9c, 247, 7},
#line 96 "rgbtab.gperf"
    {"xterm70", 0x5faf00, 70, 258},
    {"", 0, 0, 0},
#line 294 "rgbtab.gperf"
    {"gray", 0xbebebe, 250, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 188 "rgbtab.gperf"
    {"xterm162", 0xd70087, 162, 261},
#line 499 "rgbtab.gperf"
    {"gray62", 0x9e9e9e, 247, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 526 "rgbtab.gperf"
    {"slategrey", 0x708090, 66, 2},
    {"", 0, 0, 0},
#line 623 "rgbtab.gperf"
    {"gray51", 0x828282, 244, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 935 "rgbtab.gperf"
    {"gray52", 0x858585, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 528 "rgbtab.gperf"
    {"orange1", 0xffa500, 214, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 193 "rgbtab.gperf"
    {"xterm167", 0xd75f5f, 167, 257},
#line 933 "rgbtab.gperf"
    {"gray67", 0xababab, 248, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 93 "rgbtab.gperf"
    {"xterm67", 0x5f87af, 67, 6},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 721 "rgbtab.gperf"
    {"sienna2", 0xee7942, 209, 257},
#line 95 "rgbtab.gperf"
    {"xterm69", 0x5f87ff, 69, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 86 "rgbtab.gperf"
    {"xterm60", 0x5f5f87, 60, 6},
#line 175 "rgbtab.gperf"
    {"xterm149", 0xafd75f, 149, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 275 "rgbtab.gperf"
    {"xterm249", 0xb2b2b2, 249, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 299 "rgbtab.gperf"
    {"gray57", 0x919191, 246, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 83 "rgbtab.gperf"
    {"xterm57", 0x5f00ff, 57, 260},
    {"", 0, 0, 0},
#line 762 "rgbtab.gperf"
    {"grey99", 0xfcfcfc, 231, 263},
    {"", 0, 0, 0},
#line 85 "rgbtab.gperf"
    {"xterm59", 0x5f5f5f, 59, 2},
#line 42 "rgbtab.gperf"
    {"xterm16", 0x000000, 16, 0},
#line 711 "rgbtab.gperf"
    {"gray6", 0x0f0f0f, 233, 0},
#line 700 "rgbtab.gperf"
    {"gray16", 0x292929, 235, 256},
#line 76 "rgbtab.gperf"
    {"xterm50", 0x00ffd7, 50, 262},
#line 165 "rgbtab.gperf"
    {"xterm139", 0xaf87af, 139, 261},
#line 52 "rgbtab.gperf"
    {"xterm26", 0x005fd7, 26, 260},
    {"", 0, 0, 0},
#line 313 "rgbtab.gperf"
    {"gray26", 0x424242, 238, 256},
    {"", 0, 0, 0},
#line 265 "rgbtab.gperf"
    {"xterm239", 0x4e4e4e, 239, 256},
#line 509 "rgbtab.gperf"
    {"lightsteelblue", 0xb0c4de, 152, 263},
#line 530 "rgbtab.gperf"
    {"lightsteelblue1", 0xcae1ff, 189, 263},
    {"", 0, 0, 0},
#line 121 "rgbtab.gperf"
    {"xterm95", 0x875f5f, 95, 1},
#line 829 "rgbtab.gperf"
    {"violetred1", 0xff3e96, 204, 257},
    {"", 0, 0, 0},
#line 472 "rgbtab.gperf"
    {"lightsteelblue2", 0xbcd2ee, 153, 263},
#line 316 "rgbtab.gperf"
    {"grey89", 0xe3e3e3, 254, 263},
    {"", 0, 0, 0},
#line 427 "rgbtab.gperf"
    {"violetred2", 0xee3a8c, 204, 257},
#line 102 "rgbtab.gperf"
    {"xterm76", 0x5fd700, 76, 258},
    {"", 0, 0, 0},
#line 752 "rgbtab.gperf"
    {"gray76", 0xc2c2c2, 251, 263},
#line 553 "rgbtab.gperf"
    {"sienna1", 0xff8247, 209, 257},
#line 174 "rgbtab.gperf"
    {"xterm148", 0xafd700, 148, 259},
    {"", 0, 0, 0},
#line 202 "rgbtab.gperf"
    {"xterm176", 0xd787d7, 176, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 274 "rgbtab.gperf"
    {"xterm248", 0xa8a8a8, 248, 263},
#line 392 "rgbtab.gperf"
    {"lightgray", 0xd3d3d3, 252, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 111 "rgbtab.gperf"
    {"xterm85", 0x5fffaf, 85, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 847 "rgbtab.gperf"
    {"grey98", 0xfafafa, 231, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 304 "rgbtab.gperf"
    {"gray5", 0x0d0d0d, 232, 0},
#line 457 "rgbtab.gperf"
    {"gray15", 0x262626, 235, 256},
    {"", 0, 0, 0},
#line 164 "rgbtab.gperf"
    {"xterm138", 0xaf8787, 138, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 819 "rgbtab.gperf"
    {"gray25", 0x404040, 238, 256},
#line 912 "rgbtab.gperf"
    {"darkred", 0x8b0000, 88, 1},
#line 264 "rgbtab.gperf"
    {"xterm238", 0x444444, 238, 256},
    {"", 0, 0, 0},
#line 689 "rgbtab.gperf"
    {"mediumblue", 0x0000cd, 20, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 930 "rgbtab.gperf"
    {"grey88", 0xe0e0e0, 254, 263},
    {"", 0, 0, 0},
#line 910 "rgbtab.gperf"
    {"lightyellow1", 0xffffe0, 230, 263},
#line 585 "rgbtab.gperf"
    {"violet", 0xee82ee, 213, 261},
    {"", 0, 0, 0},
#line 824 "rgbtab.gperf"
    {"gray75", 0xbfbfbf, 250, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 201 "rgbtab.gperf"
    {"xterm175", 0xd787af, 175, 261},
#line 650 "rgbtab.gperf"
    {"grey41", 0x696969, 242, 7},
#line 514 "rgbtab.gperf"
    {"lightyellow", 0xffffe0, 230, 263},
#line 917 "rgbtab.gperf"
    {"lightyellow2", 0xeeeed1, 254, 263},
#line 92 "rgbtab.gperf"
    {"xterm66", 0x5f8787, 66, 2},
#line 192 "rgbtab.gperf"
    {"xterm166", 0xd75f00, 166, 257},
#line 854 "rgbtab.gperf"
    {"gray66", 0xa8a8a8, 248, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 802 "rgbtab.gperf"
    {"grey42", 0x6b6b6b, 242, 7},
#line 68 "rgbtab.gperf"
    {"xterm42", 0x00d787, 42, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 698 "rgbtab.gperf"
    {"navy", 0x000080, 18, 4},
    {"", 0, 0, 0},
#line 489 "rgbtab.gperf"
    {"plum", 0xdda0dd, 182, 261},
#line 454 "rgbtab.gperf"
    {"plum1", 0xffbbff, 219, 263},
#line 529 "rgbtab.gperf"
    {"grey31", 0x4f4f4f, 239, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 82 "rgbtab.gperf"
    {"xterm56", 0x5f00d7, 56, 260},
    {"", 0, 0, 0},
#line 704 "rgbtab.gperf"
    {"gray56", 0x8f8f8f, 245, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 538 "rgbtab.gperf"
    {"orangered", 0xff4500, 202, 257},
#line 352 "rgbtab.gperf"
    {"plum2", 0xeeaeee, 219, 263},
#line 320 "rgbtab.gperf"
    {"grey32", 0x525252, 239, 256},
#line 58 "rgbtab.gperf"
    {"xterm32", 0x0087d7, 32, 260},
    {"", 0, 0, 0},
#line 462 "rgbtab.gperf"
    {"deeppink1", 0xff1493, 198, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 551 "rgbtab.gperf"
    {"deeppink2", 0xee1289, 198, 257},
    {"", 0, 0, 0},
#line 465 "rgbtab.gperf"
    {"grey47", 0x787878, 243, 7},
#line 904 "rgbtab.gperf"
    {"greenyellow", 0xadff2f, 154, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 191 "rgbtab.gperf"
    {"xterm165", 0xd700ff, 165, 261},
#line 416 "rgbtab.gperf"
    {"gray65", 0xa6a6a6, 248, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 326 "rgbtab.gperf"
    {"forestgreen", 0x228b22, 28, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 634 "rgbtab.gperf"
    {"coral1", 0xff7256, 203, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 67 "rgbtab.gperf"
    {"xterm41", 0x00d75f, 41, 258},
#line 408 "rgbtab.gperf"
    {"coral", 0xff7f50, 209, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 662 "rgbtab.gperf"
    {"grey37", 0x5e5e5e, 59, 2},
    {"", 0, 0, 0},
#line 772 "rgbtab.gperf"
    {"darkseagreen1", 0xc1ffc1, 157, 259},
#line 938 "rgbtab.gperf"
    {"coral2", 0xee6a50, 203, 257},
    {"", 0, 0, 0},
#line 483 "rgbtab.gperf"
    {"gray55", 0x8c8c8c, 245, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 436 "rgbtab.gperf"
    {"brown1", 0xff4040, 203, 257},
    {"", 0, 0, 0},
#line 890 "rgbtab.gperf"
    {"darkseagreen2", 0xb4eeb4, 157, 259},
#line 771 "rgbtab.gperf"
    {"mediumseagreen", 0x3cb371, 71, 2},
    {"", 0, 0, 0},
#line 830 "rgbtab.gperf"
    {"gray91", 0xe8e8e8, 254, 263},
#line 57 "rgbtab.gperf"
    {"xterm31", 0x0087af, 31, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 599 "rgbtab.gperf"
    {"limegreen", 0x32cd32, 77, 258},
#line 365 "rgbtab.gperf"
    {"brown2", 0xee3b3b, 203, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 667 "rgbtab.gperf"
    {"darkslategrey1", 0x97ffff, 123, 263},
    {"", 0, 0, 0},
#line 913 "rgbtab.gperf"
    {"gray92", 0xebebeb, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 425 "rgbtab.gperf"
    {"darkslategrey2", 0x8deeee, 123, 263},
#line 868 "rgbtab.gperf"
    {"lawngreen", 0x7cfc00, 118, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 742 "rgbtab.gperf"
    {"gray81", 0xcfcfcf, 252, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 812 "rgbtab.gperf"
    {"turquoise", 0x40e0d0, 80, 262},
#line 916 "rgbtab.gperf"
    {"turquoise1", 0x00f5ff, 51, 262},
#line 668 "rgbtab.gperf"
    {"gray82", 0xd1d1d1, 252, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 390 "rgbtab.gperf"
    {"darkgoldenrod1", 0xffb90f, 214, 259},
#line 682 "rgbtab.gperf"
    {"turquoise2", 0x00e5ee, 45, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 464 "rgbtab.gperf"
    {"darkgoldenrod2", 0xeead0e, 214, 259},
    {"", 0, 0, 0},
#line 459 "rgbtab.gperf"
    {"gray97", 0xf7f7f7, 231, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 123 "rgbtab.gperf"
    {"xterm97", 0x875faf, 97, 5},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 125 "rgbtab.gperf"
    {"xterm99", 0x875fff, 99, 261},
#line 225 "rgbtab.gperf"
    {"xterm199", 0xff00af, 199, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 116 "rgbtab.gperf"
    {"xterm90", 0x870087, 90, 5},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 796 "rgbtab.gperf"
    {"violetred", 0xd02090, 162, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 298 "rgbtab.gperf"
    {"gray87", 0xdedede, 253, 263},
    {"", 0, 0, 0},
#line 185 "rgbtab.gperf"
    {"xterm159", 0xafffff, 159, 263},
#line 113 "rgbtab.gperf"
    {"xterm87", 0x5fffff, 87, 262},
    {"", 0, 0, 0},
#line 480 "rgbtab.gperf"
    {"grey46", 0x757575, 243, 7},
    {"", 0, 0, 0},
#line 115 "rgbtab.gperf"
    {"xterm89", 0x87005f, 89, 1},
#line 611 "rgbtab.gperf"
    {"peru", 0xcd853f, 173, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 106 "rgbtab.gperf"
    {"xterm80", 0x5fd7d7, 80, 262},
    {"", 0, 0, 0},
#line 801 "rgbtab.gperf"
    {"tan4", 0x8b5a2b, 94, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 672 "rgbtab.gperf"
    {"darkslategrey", 0x2f4f4f, 238, 256},
    {"", 0, 0, 0},
#line 429 "rgbtab.gperf"
    {"goldenrod4", 0x8b6914, 94, 2},
#line 607 "rgbtab.gperf"
    {"grey36", 0x5c5c5c, 59, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 224 "rgbtab.gperf"
    {"xterm198", 0xff0087, 198, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 889 "rgbtab.gperf"
    {"darkseagreen", 0x8fbc8f, 108, 2},
    {"", 0, 0, 0},
#line 907 "rgbtab.gperf"
    {"tan3", 0xcd853f, 173, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 863 "rgbtab.gperf"
    {"brown", 0xa52a2a, 124, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 184 "rgbtab.gperf"
    {"xterm158", 0xafffd7, 158, 263},
    {"", 0, 0, 0},
#line 335 "rgbtab.gperf"
    {"goldenrod3", 0xcd9b1d, 172, 3},
#line 906 "rgbtab.gperf"
    {"grey45", 0x737373, 243, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 747 "rgbtab.gperf"
    {"lightcoral", 0xf08080, 210, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 790 "rgbtab.gperf"
    {"grey35", 0x595959, 240, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 787 "rgbtab.gperf"
    {"darkorange", 0xff8c00, 208, 257},
#line 340 "rgbtab.gperf"
    {"darkorange1", 0xff7f00, 208, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 911 "rgbtab.gperf"
    {"darkorange2", 0xee7600, 208, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 122 "rgbtab.gperf"
    {"xterm96", 0x875f87, 96, 5},
#line 589 "rgbtab.gperf"
    {"beige", 0xf5f5dc, 230, 263},
#line 337 "rgbtab.gperf"
    {"gray96", 0xf5f5f5, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 424 "rgbtab.gperf"
    {"palegreen1", 0x9aff9a, 120, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 446 "rgbtab.gperf"
    {"palegreen2", 0x90ee90, 120, 258},
    {"", 0, 0, 0},
#line 874 "rgbtab.gperf"
    {"darkgray", 0xa9a9a9, 248, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 652 "rgbtab.gperf"
    {"palegoldenrod", 0xeee8aa, 223, 263},
#line 112 "rgbtab.gperf"
    {"xterm86", 0x5fffd7, 86, 262},
    {"", 0, 0, 0},
#line 684 "rgbtab.gperf"
    {"gray86", 0xdbdbdb, 253, 263},
    {"", 0, 0, 0},
#line 559 "rgbtab.gperf"
    {"thistle", 0xd8bfd8, 182, 261},
#line 295 "rgbtab.gperf"
    {"thistle1", 0xffe1ff, 225, 263},
#line 344 "rgbtab.gperf"
    {"burlywood1", 0xffd39b, 222, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 415 "rgbtab.gperf"
    {"burlywood2", 0xeec591, 222, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 832 "rgbtab.gperf"
    {"thistle2", 0xeed2ee, 254, 263},
#line 880 "rgbtab.gperf"
    {"mistyrose4", 0x8b7d7b, 244, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 718 "rgbtab.gperf"
    {"darkblue", 0x00008b, 18, 4},
    {"", 0, 0, 0},
#line 779 "rgbtab.gperf"
    {"rosybrown1", 0xffc1c1, 217, 263},
#line 562 "rgbtab.gperf"
    {"gray95", 0xf2f2f2, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 934 "rgbtab.gperf"
    {"rosybrown2", 0xeeb4b4, 217, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 831 "rgbtab.gperf"
    {"sandybrown", 0xf4a460, 215, 259},
    {"", 0, 0, 0},
#line 40 "rgbtab.gperf"
    {"xterm14", 0x00ffff, 14, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 612 "rgbtab.gperf"
    {"mistyrose3", 0xcdb7b5, 181, 261},
    {"", 0, 0, 0},
#line 50 "rgbtab.gperf"
    {"xterm24", 0x005f87, 24, 4},
#line 882 "rgbtab.gperf"
    {"deeppink", 0xff1493, 198, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 780 "rgbtab.gperf"
    {"gray85", 0xd9d9d9, 253, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 849 "rgbtab.gperf"
    {"dimgray", 0x696969, 242, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 100 "rgbtab.gperf"
    {"xterm74", 0x5fafd7, 74, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 305 "rgbtab.gperf"
    {"darkgoldenrod", 0xb8860b, 136, 3},
#line 839 "rgbtab.gperf"
    {"red4", 0x8b0000, 88, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 372 "rgbtab.gperf"
    {"olivedrab1", 0xc0ff3e, 155, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 545 "rgbtab.gperf"
    {"darkolivegreen1", 0xcaff70, 191, 259},
    {"", 0, 0, 0},
#line 635 "rgbtab.gperf"
    {"olivedrab2", 0xb3ee3a, 155, 259},
#line 820 "rgbtab.gperf"
    {"khaki1", 0xfff68f, 228, 259},
#line 39 "rgbtab.gperf"
    {"xterm13", 0xff00ff, 13, 261},
#line 330 "rgbtab.gperf"
    {"darkolivegreen2", 0xbcee68, 155, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 49 "rgbtab.gperf"
    {"xterm23", 0x005f5f, 23, 6},
    {"", 0, 0, 0},
#line 614 "rgbtab.gperf"
    {"red3", 0xcd0000, 160, 1},
#line 615 "rgbtab.gperf"
    {"lightgoldenrod4", 0x8b814c, 101, 2},
#line 788 "rgbtab.gperf"
    {"khaki2", 0xeee685, 222, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 542 "rgbtab.gperf"
    {"palegreen", 0x98fb98, 120, 258},
    {"", 0, 0, 0},
#line 99 "rgbtab.gperf"
    {"xterm73", 0x5fafaf, 73, 6},
    {"", 0, 0, 0},
#line 751 "rgbtab.gperf"
    {"lightslategrey", 0x778899, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 765 "rgbtab.gperf"
    {"lightgoldenrod3", 0xcdbe70, 179, 3},
    {"", 0, 0, 0},
#line 90 "rgbtab.gperf"
    {"xterm64", 0x5f8700, 64, 2},
#line 520 "rgbtab.gperf"
    {"mediumorchid1", 0xe066ff, 171, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 750 "rgbtab.gperf"
    {"mediumorchid2", 0xd15fee, 171, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 368 "rgbtab.gperf"
    {"mediumspringgreen", 0x00fa9a, 48, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 609 "rgbtab.gperf"
    {"gray9", 0x171717, 233, 0},
#line 360 "rgbtab.gperf"
    {"gray19", 0x303030, 236, 256},
#line 80 "rgbtab.gperf"
    {"xterm54", 0x5f0087, 54, 5},
    {"", 0, 0, 0},
#line 486 "rgbtab.gperf"
    {"rosybrown", 0xbc8f8f, 138, 261},
    {"", 0, 0, 0},
#line 515 "rgbtab.gperf"
    {"gray29", 0x4a4a4a, 239, 256},
    {"", 0, 0, 0},
#line 573 "rgbtab.gperf"
    {"paleturquoise", 0xafeeee, 159, 263},
#line 484 "rgbtab.gperf"
    {"paleturquoise1", 0xbbffff, 159, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 879 "rgbtab.gperf"
    {"paleturquoise2", 0xaeeeee, 159, 263},
#line 688 "rgbtab.gperf"
    {"indianred1", 0xff6a6a, 203, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 549 "rgbtab.gperf"
    {"indianred2", 0xee6363, 203, 257},
#line 583 "rgbtab.gperf"
    {"gray79", 0xc9c9c9, 251, 263},
#line 89 "rgbtab.gperf"
    {"xterm63", 0x5f5fff, 63, 260},
    {"", 0, 0, 0},
#line 74 "rgbtab.gperf"
    {"xterm48", 0x00ff87, 48, 258},
#line 205 "rgbtab.gperf"
    {"xterm179", 0xd7af5f, 179, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 518 "rgbtab.gperf"
    {"darkgreen", 0x006400, 22, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 763 "rgbtab.gperf"
    {"yellow4", 0x8b8b00, 100, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 940 "rgbtab.gperf"
    {"gray8", 0x141414, 233, 0},
#line 605 "rgbtab.gperf"
    {"gray18", 0x2e2e2e, 236, 256},
#line 79 "rgbtab.gperf"
    {"xterm53", 0x5f005f, 53, 5},
    {"", 0, 0, 0},
#line 64 "rgbtab.gperf"
    {"xterm38", 0x00afd7, 38, 260},
    {"", 0, 0, 0},
#line 600 "rgbtab.gperf"
    {"gray28", 0x474747, 238, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 822 "rgbtab.gperf"
    {"darkolivegreen", 0x556b2f, 239, 256},
#line 544 "rgbtab.gperf"
    {"hotpink1", 0xff6eb4, 205, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 783 "rgbtab.gperf"
    {"salmon4", 0x8b4c39, 95, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 548 "rgbtab.gperf"
    {"gray78", 0xc7c7c7, 251, 263},
    {"", 0, 0, 0},
#line 928 "rgbtab.gperf"
    {"hotpink2", 0xee6aa7, 205, 261},
    {"", 0, 0, 0},
#line 204 "rgbtab.gperf"
    {"xterm178", 0xd7af00, 178, 3},
    {"", 0, 0, 0},
#line 643 "rgbtab.gperf"
    {"tomato4", 0x8b3626, 94, 2},
#line 893 "rgbtab.gperf"
    {"darkturquoise", 0x00ced1, 44, 262},
#line 864 "rgbtab.gperf"
    {"burlywood", 0xdeb887, 180, 259},
#line 195 "rgbtab.gperf"
    {"xterm169", 0xd75faf, 169, 261},
#line 719 "rgbtab.gperf"
    {"gray69", 0xb0b0b0, 145, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 888 "rgbtab.gperf"
    {"darkgrey", 0xa9a9a9, 248, 263},
    {"", 0, 0, 0},
#line 657 "rgbtab.gperf"
    {"lemonchiffon1", 0xfffacd, 230, 263},
#line 651 "rgbtab.gperf"
    {"yellow3", 0xcdcd00, 184, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 26 "rgbtab.gperf"
    {"xterm0", 0x000000, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 306 "rgbtab.gperf"
    {"lemonchiffon2", 0xeee9bf, 223, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 937 "rgbtab.gperf"
    {"gray59", 0x969696, 246, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 946 "rgbtab.gperf"
    {"grey0", 0x000000, 16, 0},
#line 675 "rgbtab.gperf"
    {"grey10", 0x1a1a1a, 234, 256},
#line 703 "rgbtab.gperf"
    {"salmon3", 0xcd7054, 167, 257},
    {"", 0, 0, 0},
#line 505 "rgbtab.gperf"
    {"gainsboro", 0xdcdcdc, 253, 263},
#line 555 "rgbtab.gperf"
    {"orchid2", 0xee7ae9, 212, 261},
#line 894 "rgbtab.gperf"
    {"grey20", 0x333333, 236, 256},
    {"", 0, 0, 0},
#line 146 "rgbtab.gperf"
    {"xterm120", 0x87ff87, 120, 258},
    {"", 0, 0, 0},
#line 370 "rgbtab.gperf"
    {"darksalmon", 0xe9967a, 174, 3},
    {"", 0, 0, 0},
#line 821 "rgbtab.gperf"
    {"tomato3", 0xcd4f39, 167, 257},
#line 246 "rgbtab.gperf"
    {"xterm220", 0xffd700, 220, 259},
    {"", 0, 0, 0},
#line 194 "rgbtab.gperf"
    {"xterm168", 0xd75f87, 168, 257},
#line 702 "rgbtab.gperf"
    {"gray68", 0xadadad, 145, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 858 "rgbtab.gperf"
    {"grey70", 0xb3b3b3, 249, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 797 "rgbtab.gperf"
    {"deepskyblue", 0x00bfff, 39, 260},
#line 435 "rgbtab.gperf"
    {"deepskyblue1", 0x00bfff, 39, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 510 "rgbtab.gperf"
    {"azure", 0xf0ffff, 231, 263},
#line 649 "rgbtab.gperf"
    {"azure1", 0xf0ffff, 231, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 534 "rgbtab.gperf"
    {"gray58", 0x949494, 246, 7},
#line 356 "rgbtab.gperf"
    {"deepskyblue2", 0x00b2ee, 39, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 588 "rgbtab.gperf"
    {"azure2", 0xe0eeee, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 350 "rgbtab.gperf"
    {"orchid1", 0xff83fa, 213, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 380 "rgbtab.gperf"
    {"dodgerblue4", 0x104e8b, 24, 4},
    {"", 0, 0, 0},
#line 136 "rgbtab.gperf"
    {"xterm110", 0x87afd7, 110, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 428 "rgbtab.gperf"
    {"magenta", 0xff00ff, 201, 261},
#line 236 "rgbtab.gperf"
    {"xterm210", 0xff8787, 210, 261},
    {"", 0, 0, 0},
#line 482 "rgbtab.gperf"
    {"slategray1", 0xc6e2ff, 189, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 775 "rgbtab.gperf"
    {"slategray2", 0xb9d3ee, 153, 263},
#line 876 "rgbtab.gperf"
    {"grey60", 0x999999, 246, 7},
#line 695 "rgbtab.gperf"
    {"mediumorchid", 0xba55d3, 134, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 878 "rgbtab.gperf"
    {"lightpink4", 0x8b5f65, 95, 1},
#line 799 "rgbtab.gperf"
    {"dodgerblue3", 0x1874cd, 32, 260},
    {"", 0, 0, 0},
#line 126 "rgbtab.gperf"
    {"xterm100", 0x878700, 100, 2},
    {"", 0, 0, 0},
#line 206 "rgbtab.gperf"
    {"xterm180", 0xd7af87, 180, 259},
    {"", 0, 0, 0},
#line 647 "rgbtab.gperf"
    {"maroon4", 0x8b1c62, 89, 1},
#line 226 "rgbtab.gperf"
    {"xterm200", 0xff00d7, 200, 261},
    {"", 0, 0, 0},
#line 493 "rgbtab.gperf"
    {"lemonchiffon", 0xfffacd, 230, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 506 "rgbtab.gperf"
    {"grey50", 0x7f7f7f, 244, 7},
#line 527 "rgbtab.gperf"
    {"grey100", 0xffffff, 231, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 558 "rgbtab.gperf"
    {"lightpink3", 0xcd8c95, 174, 3},
    {"", 0, 0, 0},
#line 120 "rgbtab.gperf"
    {"xterm94", 0x875f00, 94, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 414 "rgbtab.gperf"
    {"indianred", 0xcd5c5c, 167, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 610 "rgbtab.gperf"
    {"grey49", 0x7d7d7d, 244, 7},
    {"", 0, 0, 0},
#line 423 "rgbtab.gperf"
    {"darkviolet", 0x9400d3, 92, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 110 "rgbtab.gperf"
    {"xterm84", 0x5fff87, 84, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 939 "rgbtab.gperf"
    {"maroon3", 0xcd2990, 162, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 71 "rgbtab.gperf"
    {"xterm45", 0x00d7ff, 45, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 590 "rgbtab.gperf"
    {"grey39", 0x636363, 241, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 119 "rgbtab.gperf"
    {"xterm93", 0x8700ff, 93, 261},
    {"", 0, 0, 0},
#line 543 "rgbtab.gperf"
    {"slategray", 0x708090, 66, 2},
#line 442 "rgbtab.gperf"
    {"khaki", 0xf0e68c, 222, 259},
#line 577 "rgbtab.gperf"
    {"sienna", 0xa0522d, 130, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 61 "rgbtab.gperf"
    {"xterm35", 0x00af5f, 35, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 725 "rgbtab.gperf"
    {"grey48", 0x7a7a7a, 243, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 109 "rgbtab.gperf"
    {"xterm83", 0x5fff5f, 83, 258},
#line 731 "rgbtab.gperf"
    {"skyblue", 0x87ceeb, 116, 262},
#line 729 "rgbtab.gperf"
    {"skyblue1", 0x87ceff, 117, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 624 "rgbtab.gperf"
    {"chocolate", 0xd2691e, 166, 257},
#line 853 "rgbtab.gperf"
    {"chocolate1", 0xff7f24, 208, 257},
#line 697 "rgbtab.gperf"
    {"orchid", 0xda70d6, 170, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 371 "rgbtab.gperf"
    {"chocolate2", 0xee7621, 208, 257},
#line 828 "rgbtab.gperf"
    {"skyblue2", 0x7ec0ee, 111, 262},
    {"", 0, 0, 0},
#line 388 "rgbtab.gperf"
    {"grey38", 0x616161, 241, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 336 "rgbtab.gperf"
    {"mediumvioletred", 0xc71585, 162, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 522 "rgbtab.gperf"
    {"cyan1", 0x00ffff, 51, 262},
#line 931 "rgbtab.gperf"
    {"blue", 0x0000ff, 21, 260},
#line 432 "rgbtab.gperf"
    {"blue1", 0x0000ff, 21, 260},
#line 754 "rgbtab.gperf"
    {"bisque", 0xffe4c4, 224, 263},
#line 567 "rgbtab.gperf"
    {"palevioletred1", 0xff82ab, 211, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 929 "rgbtab.gperf"
    {"gray99", 0xfcfcfc, 231, 263},
#line 557 "rgbtab.gperf"
    {"palevioletred2", 0xee799f, 211, 261},
#line 580 "rgbtab.gperf"
    {"cyan2", 0x00eeee, 51, 262},
    {"", 0, 0, 0},
#line 525 "rgbtab.gperf"
    {"blue2", 0x0000ee, 21, 260},
    {"", 0, 0, 0},
#line 421 "rgbtab.gperf"
    {"purple4", 0x551a8b, 54, 5},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 418 "rgbtab.gperf"
    {"hotpink", 0xff69b4, 205, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 896 "rgbtab.gperf"
    {"gray89", 0xe3e3e3, 254, 263},
#line 507 "rgbtab.gperf"
    {"bisque2", 0xeed5b7, 223, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 576 "rgbtab.gperf"
    {"gray98", 0xfafafa, 231, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 692 "rgbtab.gperf"
    {"purple3", 0x7d26cd, 92, 261},
    {"", 0, 0, 0},
#line 546 "rgbtab.gperf"
    {"seashell1", 0xfff5ee, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 814 "rgbtab.gperf"
    {"seashell", 0xfff5ee, 255, 263},
#line 760 "rgbtab.gperf"
    {"seashell2", 0xeee5de, 254, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 884 "rgbtab.gperf"
    {"gray88", 0xe0e0e0, 254, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 744 "rgbtab.gperf"
    {"bisque1", 0xffe4c4, 224, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 732 "rgbtab.gperf"
    {"gray41", 0x696969, 242, 7},
    {"", 0, 0, 0},
#line 469 "rgbtab.gperf"
    {"darkslateblue", 0x483d8b, 60, 6},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 852 "rgbtab.gperf"
    {"gray42", 0x6b6b6b, 242, 7},
    {"", 0, 0, 0},
#line 166 "rgbtab.gperf"
    {"xterm140", 0xaf87d7, 140, 261},
    {"", 0, 0, 0},
#line 569 "rgbtab.gperf"
    {"slategrey4", 0x6c7b8b, 66, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 266 "rgbtab.gperf"
    {"xterm240", 0x585858, 240, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 568 "rgbtab.gperf"
    {"gray31", 0x4f4f4f, 239, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 833 "rgbtab.gperf"
    {"grey90", 0xe5e5e5, 254, 263},
#line 782 "rgbtab.gperf"
    {"cyan", 0x00ffff, 51, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 501 "rgbtab.gperf"
    {"gray32", 0x525252, 239, 256},
    {"", 0, 0, 0},
#line 156 "rgbtab.gperf"
    {"xterm130", 0xaf5f00, 130, 257},
    {"", 0, 0, 0},
#line 842 "rgbtab.gperf"
    {"slategrey3", 0x9fb6cd, 146, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 256 "rgbtab.gperf"
    {"xterm230", 0xffffd7, 230, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 582 "rgbtab.gperf"
    {"gray47", 0x787878, 243, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 73 "rgbtab.gperf"
    {"xterm47", 0x00ff5f, 47, 258},
    {"", 0, 0, 0},
#line 923 "rgbtab.gperf"
    {"grey80", 0xcccccc, 252, 263},
    {"", 0, 0, 0},
#line 75 "rgbtab.gperf"
    {"xterm49", 0x00ffaf, 49, 262},
    {"", 0, 0, 0},
#line 927 "rgbtab.gperf"
    {"orangered4", 0x8b2500, 88, 1},
    {"", 0, 0, 0},
#line 66 "rgbtab.gperf"
    {"xterm40", 0x00d700, 40, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 755 "rgbtab.gperf"
    {"seagreen1", 0x54ff9f, 85, 258},
#line 386 "rgbtab.gperf"
    {"gray37", 0x5e5e5e, 59, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 63 "rgbtab.gperf"
    {"xterm37", 0x00afaf, 37, 262},
#line 794 "rgbtab.gperf"
    {"seagreen2", 0x4eee94, 84, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 65 "rgbtab.gperf"
    {"xterm39", 0x00afff, 39, 260},
    {"", 0, 0, 0},
#line 283 "rgbtab.gperf"
    {"orangered3", 0xcd3700, 166, 257},
    {"", 0, 0, 0},
#line 56 "rgbtab.gperf"
    {"xterm30", 0x008787, 30, 6},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 739 "rgbtab.gperf"
    {"darkslategray1", 0x97ffff, 123, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 485 "rgbtab.gperf"
    {"darkslategray2", 0x8deeee, 123, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 707 "rgbtab.gperf"
    {"darkmagenta", 0x8b008b, 90, 5},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 861 "rgbtab.gperf"
    {"navyblue", 0x000080, 18, 4},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 867 "rgbtab.gperf"
    {"palevioletred", 0xdb7093, 168, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 918 "rgbtab.gperf"
    {"darkorchid1", 0xbf3eff, 135, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 602 "rgbtab.gperf"
    {"darkorchid2", 0xb23aee, 135, 261},
    {"", 0, 0, 0},
#line 394 "rgbtab.gperf"
    {"lightcyan1", 0xe0ffff, 195, 263},
#line 898 "rgbtab.gperf"
    {"lightseagreen", 0x20b2aa, 37, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 497 "rgbtab.gperf"
    {"lightcyan2", 0xd1eeee, 254, 263},
#line 490 "rgbtab.gperf"
    {"lightslateblue", 0x8470ff, 99, 261},
#line 354 "rgbtab.gperf"
    {"mediumslateblue", 0x7b68ee, 99, 261},
    {"", 0, 0, 0},
#line 339 "rgbtab.gperf"
    {"cornsilk1", 0xfff8dc, 230, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 798 "rgbtab.gperf"
    {"honeydew1", 0xf0fff0, 255, 263},
    {"", 0, 0, 0},
#line 297 "rgbtab.gperf"
    {"cornsilk2", 0xeee8cd, 254, 263},
    {"", 0, 0, 0},
#line 72 "rgbtab.gperf"
    {"xterm46", 0x00ff00, 46, 258},
#line 592 "rgbtab.gperf"
    {"honeydew2", 0xe0eee0, 254, 263},
#line 865 "rgbtab.gperf"
    {"gray46", 0x757575, 243, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 778 "rgbtab.gperf"
    {"honeydew", 0xf0fff0, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 296 "rgbtab.gperf"
    {"lightsteelblue4", 0x6e7b8b, 66, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 284 "rgbtab.gperf"
    {"violetred4", 0x8b2252, 89, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 816 "rgbtab.gperf"
    {"darkslategray", 0x2f4f4f, 238, 256},
#line 62 "rgbtab.gperf"
    {"xterm36", 0x00af87, 36, 262},
    {"", 0, 0, 0},
#line 685 "rgbtab.gperf"
    {"gray36", 0x5c5c5c, 59, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 777 "rgbtab.gperf"
    {"seagreen", 0x2e8b57, 29, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 862 "rgbtab.gperf"
    {"lightsteelblue3", 0xa2b5cd, 146, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 351 "rgbtab.gperf"
    {"violetred3", 0xcd3278, 168, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 301 "rgbtab.gperf"
    {"gray45", 0x737373, 243, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 30 "rgbtab.gperf"
    {"xterm4", 0x000080, 4, 4},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 637 "rgbtab.gperf"
    {"steelblue", 0x4682b4, 67, 6},
#line 843 "rgbtab.gperf"
    {"steelblue1", 0x63b8ff, 75, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 389 "rgbtab.gperf"
    {"steelblue2", 0x5cacee, 75, 262},
#line 768 "rgbtab.gperf"
    {"gray35", 0x595959, 240, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 531 "rgbtab.gperf"
    {"grey4", 0x0a0a0a, 232, 0},
#line 303 "rgbtab.gperf"
    {"grey14", 0x242424, 235, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 841 "rgbtab.gperf"
    {"gold4", 0x8b7500, 100, 2},
#line 397 "rgbtab.gperf"
    {"grey24", 0x3d3d3d, 237, 256},
    {"", 0, 0, 0},
#line 150 "rgbtab.gperf"
    {"xterm124", 0xaf0000, 124, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 250 "rgbtab.gperf"
    {"xterm224", 0xffd7d7, 224, 263},
#line 216 "rgbtab.gperf"
    {"xterm190", 0xd7ff00, 190, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 564 "rgbtab.gperf"
    {"snow4", 0x8b8989, 245, 7},
#line 722 "rgbtab.gperf"
    {"grey74", 0xbdbdbd, 250, 263},
#line 487 "rgbtab.gperf"
    {"lightcyan", 0xe0ffff, 195, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 29 "rgbtab.gperf"
    {"xterm3", 0x808000, 3, 3},
    {"", 0, 0, 0},
#line 176 "rgbtab.gperf"
    {"xterm150", 0xafd787, 150, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 276 "rgbtab.gperf"
    {"xterm250", 0xbcbcbc, 250, 263},
#line 466 "rgbtab.gperf"
    {"deeppink4", 0x8b0a50, 89, 1},
    {"", 0, 0, 0},
#line 450 "rgbtab.gperf"
    {"moccasin", 0xffe4b5, 223, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 496 "rgbtab.gperf"
    {"grey3", 0x080808, 232, 0},
#line 785 "rgbtab.gperf"
    {"grey13", 0x212121, 234, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 926 "rgbtab.gperf"
    {"gold3", 0xcdad00, 178, 3},
#line 334 "rgbtab.gperf"
    {"grey23", 0x3b3b3b, 237, 256},
    {"", 0, 0, 0},
#line 149 "rgbtab.gperf"
    {"xterm123", 0x87ffff, 123, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 249 "rgbtab.gperf"
    {"xterm223", 0xffd7af, 223, 263},
#line 552 "rgbtab.gperf"
    {"deeppink3", 0xcd1076, 162, 261},
    {"", 0, 0, 0},
#line 300 "rgbtab.gperf"
    {"wheat4", 0x8b7e66, 101, 2},
    {"", 0, 0, 0},
#line 140 "rgbtab.gperf"
    {"xterm114", 0x87d787, 114, 258},
    {"", 0, 0, 0},
#line 422 "rgbtab.gperf"
    {"snow3", 0xcdc9c9, 251, 263},
#line 619 "rgbtab.gperf"
    {"grey73", 0xbababa, 250, 263},
    {"", 0, 0, 0},
#line 240 "rgbtab.gperf"
    {"xterm214", 0xffaf00, 214, 259},
#line 312 "rgbtab.gperf"
    {"lightblue", 0xadd8e6, 152, 263},
#line 437 "rgbtab.gperf"
    {"lightblue1", 0xbfefff, 159, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 900 "rgbtab.gperf"
    {"lightblue2", 0xb2dfee, 153, 263},
#line 333 "rgbtab.gperf"
    {"grey64", 0xa3a3a3, 247, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 130 "rgbtab.gperf"
    {"xterm104", 0x8787d7, 104, 260},
    {"", 0, 0, 0},
#line 210 "rgbtab.gperf"
    {"xterm184", 0xd7d700, 184, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 230 "rgbtab.gperf"
    {"xterm204", 0xff5f87, 204, 257},
#line 601 "rgbtab.gperf"
    {"darkslategrey4", 0x528b8b, 66, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 516 "rgbtab.gperf"
    {"grey54", 0x8a8a8a, 245, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 807 "rgbtab.gperf"
    {"darkorchid", 0x9932cc, 98, 261},
#line 383 "rgbtab.gperf"
    {"wheat3", 0xcdba96, 180, 259},
    {"", 0, 0, 0},
#line 139 "rgbtab.gperf"
    {"xterm113", 0x87d75f, 113, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 239 "rgbtab.gperf"
    {"xterm213", 0xff87ff, 213, 261},
#line 872 "rgbtab.gperf"
    {"darkslategrey3", 0x79cdcd, 116, 262},
#line 508 "rgbtab.gperf"
    {"turquoise4", 0x00868b, 30, 6},
#line 417 "rgbtab.gperf"
    {"green4", 0x008b00, 28, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 345 "rgbtab.gperf"
    {"darkgoldenrod4", 0x8b6508, 94, 2},
    {"", 0, 0, 0},
#line 474 "rgbtab.gperf"
    {"grey63", 0xa1a1a1, 247, 7},
#line 887 "rgbtab.gperf"
    {"orange4", 0x8b5a00, 94, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 129 "rgbtab.gperf"
    {"xterm103", 0x8787af, 103, 6},
    {"", 0, 0, 0},
#line 209 "rgbtab.gperf"
    {"xterm183", 0xd7afff, 183, 261},
#line 925 "rgbtab.gperf"
    {"floralwhite", 0xfffaf0, 231, 263},
    {"", 0, 0, 0},
#line 229 "rgbtab.gperf"
    {"xterm203", 0xff5f5f, 203, 257},
#line 653 "rgbtab.gperf"
    {"lightslategray", 0x778899, 102, 2},
#line 753 "rgbtab.gperf"
    {"turquoise3", 0x00c5cd, 44, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 375 "rgbtab.gperf"
    {"darkgoldenrod3", 0xcd950c, 172, 3},
    {"", 0, 0, 0},
#line 574 "rgbtab.gperf"
    {"grey53", 0x878787, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 523 "rgbtab.gperf"
    {"green3", 0x00cd00, 40, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 646 "rgbtab.gperf"
    {"orange3", 0xcd8500, 172, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 789 "rgbtab.gperf"
    {"mediumturquoise", 0x48d1cc, 80, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 331 "rgbtab.gperf"
    {"darkcyan", 0x008b8b, 30, 6},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 591 "rgbtab.gperf"
    {"cornsilk", 0xfff8dc, 230, 263},
#line 870 "rgbtab.gperf"
    {"sienna4", 0x8b4726, 94, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 381 "rgbtab.gperf"
    {"royalblue", 0x4169e1, 62, 260},
#line 438 "rgbtab.gperf"
    {"royalblue1", 0x4876ff, 69, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 936 "rgbtab.gperf"
    {"royalblue2", 0x436eee, 63, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 710 "rgbtab.gperf"
    {"black", 0x000000, 16, 0},
#line 439 "rgbtab.gperf"
    {"darkorange4", 0x8b4500, 94, 2},
#line 628 "rgbtab.gperf"
    {"sienna3", 0xcd6839, 167, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 500 "rgbtab.gperf"
    {"palegreen4", 0x548b54, 65, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 512 "rgbtab.gperf"
    {"darkorange3", 0xcd6600, 166, 257},
#line 550 "rgbtab.gperf"
    {"lavender", 0xe6e6fa, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 289 "rgbtab.gperf"
    {"burlywood4", 0x8b7355, 95, 1},
#line 536 "rgbtab.gperf"
    {"palegreen3", 0x7ccd7c, 114, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 617 "rgbtab.gperf"
    {"olivedrab", 0x6b8e23, 64, 2},
#line 674 "rgbtab.gperf"
    {"gray0", 0x000000, 16, 0},
#line 293 "rgbtab.gperf"
    {"gray10", 0x1a1a1a, 234, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 571 "rgbtab.gperf"
    {"gray20", 0x333333, 236, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 720 "rgbtab.gperf"
    {"rosybrown4", 0x8b6969, 95, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 355 "rgbtab.gperf"
    {"burlywood3", 0xcdaa7d, 180, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 764 "rgbtab.gperf"
    {"gray70", 0xb3b3b3, 249, 263},
#line 70 "rgbtab.gperf"
    {"xterm44", 0x00d7d7, 44, 262},
#line 170 "rgbtab.gperf"
    {"xterm144", 0xafaf87, 144, 3},
    {"", 0, 0, 0},
#line 196 "rgbtab.gperf"
    {"xterm170", 0xd75fd7, 170, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 270 "rgbtab.gperf"
    {"xterm244", 0x808080, 244, 7},
    {"", 0, 0, 0},
#line 670 "rgbtab.gperf"
    {"rosybrown3", 0xcd9b9b, 174, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 434 "rgbtab.gperf"
    {"grey94", 0xf0f0f0, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 60 "rgbtab.gperf"
    {"xterm34", 0x00af00, 34, 258},
#line 160 "rgbtab.gperf"
    {"xterm134", 0xaf5fd7, 134, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 260 "rgbtab.gperf"
    {"xterm234", 0x1c1c1c, 234, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 513 "rgbtab.gperf"
    {"olivedrab4", 0x698b22, 64, 2},
#line 537 "rgbtab.gperf"
    {"grey84", 0xd6d6d6, 188, 263},
    {"", 0, 0, 0},
#line 786 "rgbtab.gperf"
    {"darkolivegreen4", 0x6e8b3d, 65, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 69 "rgbtab.gperf"
    {"xterm43", 0x00d7af, 43, 262},
#line 169 "rgbtab.gperf"
    {"xterm143", 0xafaf5f, 143, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 269 "rgbtab.gperf"
    {"xterm243", 0x767676, 243, 7},
    {"", 0, 0, 0},
#line 186 "rgbtab.gperf"
    {"xterm160", 0xd70000, 160, 1},
#line 477 "rgbtab.gperf"
    {"gray60", 0x999999, 246, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 584 "rgbtab.gperf"
    {"olivedrab3", 0x9acd32, 113, 258},
#line 875 "rgbtab.gperf"
    {"grey93", 0xededed, 255, 263},
    {"", 0, 0, 0},
#line 376 "rgbtab.gperf"
    {"darkolivegreen3", 0xa2cd5a, 149, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 59 "rgbtab.gperf"
    {"xterm33", 0x0087ff, 33, 260},
#line 159 "rgbtab.gperf"
    {"xterm133", 0xaf5faf, 133, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 259 "rgbtab.gperf"
    {"xterm233", 0x121212, 233, 0},
    {"", 0, 0, 0},
#line 410 "rgbtab.gperf"
    {"pink4", 0x8b636c, 95, 1},
#line 377 "rgbtab.gperf"
    {"gray50", 0x7f7f7f, 244, 7},
#line 677 "rgbtab.gperf"
    {"gray100", 0xffffff, 231, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 728 "rgbtab.gperf"
    {"papayawhip", 0xffefd5, 230, 263},
#line 845 "rgbtab.gperf"
    {"grey83", 0xd4d4d4, 188, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 686 "rgbtab.gperf"
    {"gray49", 0x7d7d7d, 244, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 749 "rgbtab.gperf"
    {"paleturquoise4", 0x668b8b, 66, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 323 "rgbtab.gperf"
    {"springgreen4", 0x008b45, 29, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 378 "rgbtab.gperf"
    {"indianred4", 0x8b3a3a, 95, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 310 "rgbtab.gperf"
    {"pink3", 0xcd919e, 175, 261},
#line 541 "rgbtab.gperf"
    {"gray39", 0x636363, 241, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 921 "rgbtab.gperf"
    {"ivory4", 0x8b8b83, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 343 "rgbtab.gperf"
    {"paleturquoise3", 0x96cdcd, 116, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 440 "rgbtab.gperf"
    {"indianred3", 0xcd5555, 167, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 855 "rgbtab.gperf"
    {"lightskyblue4", 0x607b8b, 66, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 395 "rgbtab.gperf"
    {"gray48", 0x7a7a7a, 243, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 892 "rgbtab.gperf"
    {"springgreen3", 0x00cd66, 41, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 827 "rgbtab.gperf"
    {"gray38", 0x616161, 241, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 694 "rgbtab.gperf"
    {"ivory3", 0xcdcdc1, 251, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 738 "rgbtab.gperf"
    {"darkkhaki", 0xbdb76b, 143, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 627 "rgbtab.gperf"
    {"lightskyblue3", 0x8db6cd, 110, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 860 "rgbtab.gperf"
    {"grey40", 0x666666, 241, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 328 "rgbtab.gperf"
    {"mediumpurple", 0x9370db, 98, 261},
#line 712 "rgbtab.gperf"
    {"mediumpurple1", 0xab82ff, 141, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 792 "rgbtab.gperf"
    {"mediumpurple2", 0x9f79ee, 141, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 681 "rgbtab.gperf"
    {"grey30", 0x4d4d4d, 239, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 220 "rgbtab.gperf"
    {"xterm194", 0xd7ffd7, 194, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 734 "rgbtab.gperf"
    {"cornflowerblue", 0x6495ed, 69, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 318 "rgbtab.gperf"
    {"navajowhite", 0xffdead, 223, 263},
#line 766 "rgbtab.gperf"
    {"navajowhite1", 0xffdead, 223, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 180 "rgbtab.gperf"
    {"xterm154", 0xafff00, 154, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 358 "rgbtab.gperf"
    {"navajowhite2", 0xeecfa1, 223, 263},
    {"", 0, 0, 0},
#line 280 "rgbtab.gperf"
    {"xterm254", 0xe4e4e4, 254, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 219 "rgbtab.gperf"
    {"xterm193", 0xd7ffaf, 193, 263},
#line 413 "rgbtab.gperf"
    {"slategray4", 0x6c7b8b, 66, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 740 "rgbtab.gperf"
    {"gray90", 0xe5e5e5, 254, 263},
    {"", 0, 0, 0},
#line 179 "rgbtab.gperf"
    {"xterm153", 0xafd7ff, 153, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 279 "rgbtab.gperf"
    {"xterm253", 0xdadada, 253, 263},
    {"", 0, 0, 0},
#line 691 "rgbtab.gperf"
    {"slategray3", 0x9fb6cd, 146, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 338 "rgbtab.gperf"
    {"gray80", 0xcccccc, 252, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 837 "rgbtab.gperf"
    {"peachpuff1", 0xffdab9, 223, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 626 "rgbtab.gperf"
    {"peachpuff2", 0xeecbad, 223, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 433 "rgbtab.gperf"
    {"aquamarine", 0x7fffd4, 122, 263},
#line 639 "rgbtab.gperf"
    {"aquamarine1", 0x7fffd4, 122, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 857 "rgbtab.gperf"
    {"aquamarine2", 0x76eec6, 122, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 572 "rgbtab.gperf"
    {"chocolate4", 0x8b4513, 94, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 919 "rgbtab.gperf"
    {"lightsalmon4", 0x8b5742, 95, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 374 "rgbtab.gperf"
    {"palevioletred4", 0x8b475d, 95, 1},
#line 319 "rgbtab.gperf"
    {"chocolate3", 0xcd661d, 166, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 288 "rgbtab.gperf"
    {"palevioletred3", 0xcd6889, 168, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 451 "rgbtab.gperf"
    {"antiquewhite", 0xfaebd7, 224, 263},
#line 708 "rgbtab.gperf"
    {"antiquewhite1", 0xffefdb, 230, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 825 "rgbtab.gperf"
    {"lightsalmon3", 0xcd8162, 173, 3},
    {"", 0, 0, 0},
#line 673 "rgbtab.gperf"
    {"antiquewhite2", 0xeedfcc, 224, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 687 "rgbtab.gperf"
    {"seashell4", 0x8b8682, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 840 "rgbtab.gperf"
    {"slateblue", 0x6a5acd, 62, 260},
#line 767 "rgbtab.gperf"
    {"slateblue1", 0x836fff, 99, 261},
#line 491 "rgbtab.gperf"
    {"oldlace", 0xfdf5e6, 230, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 621 "rgbtab.gperf"
    {"slateblue2", 0x7a67ee, 99, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 329 "rgbtab.gperf"
    {"mediumaquamarine", 0x66cdaa, 79, 262},
    {"", 0, 0, 0},
#line 361 "rgbtab.gperf"
    {"mintcream", 0xf5fffa, 231, 263},
#line 470 "rgbtab.gperf"
    {"seashell3", 0xcdc5bf, 251, 263},
#line 342 "rgbtab.gperf"
    {"gray4", 0x0a0a0a, 232, 0},
#line 891 "rgbtab.gperf"
    {"gray14", 0x242424, 235, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 733 "rgbtab.gperf"
    {"orchid4", 0x8b4789, 96, 5},
#line 897 "rgbtab.gperf"
    {"gray24", 0x3d3d3d, 237, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 570 "rgbtab.gperf"
    {"magenta4", 0x8b008b, 90, 5},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 895 "rgbtab.gperf"
    {"gray74", 0xbdbdbd, 250, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 200 "rgbtab.gperf"
    {"xterm174", 0xd78787, 174, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 412 "rgbtab.gperf"
    {"lavenderblush1", 0xfff0f5, 231, 263},
    {"", 0, 0, 0},
#line 367 "rgbtab.gperf"
    {"gray3", 0x080808, 232, 0},
#line 915 "rgbtab.gperf"
    {"gray13", 0x212121, 234, 256},
    {"", 0, 0, 0},
#line 818 "rgbtab.gperf"
    {"lavenderblush2", 0xeee0e5, 254, 263},
    {"", 0, 0, 0},
#line 456 "rgbtab.gperf"
    {"orchid3", 0xcd69c9, 170, 261},
#line 811 "rgbtab.gperf"
    {"gray23", 0x3b3b3b, 237, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 563 "rgbtab.gperf"
    {"magenta3", 0xcd00cd, 164, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 321 "rgbtab.gperf"
    {"seagreen4", 0x2e8b57, 29, 2},
#line 282 "rgbtab.gperf"
    {"gray73", 0xbababa, 250, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 199 "rgbtab.gperf"
    {"xterm173", 0xd7875f, 173, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 190 "rgbtab.gperf"
    {"xterm164", 0xd700d7, 164, 261},
#line 347 "rgbtab.gperf"
    {"gray64", 0xa3a3a3, 247, 7},
    {"", 0, 0, 0},
#line 835 "rgbtab.gperf"
    {"firebrick1", 0xff3030, 203, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 426 "rgbtab.gperf"
    {"firebrick2", 0xee2c2c, 196, 257},
    {"", 0, 0, 0},
#line 341 "rgbtab.gperf"
    {"seagreen3", 0x43cd80, 78, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 384 "rgbtab.gperf"
    {"darkslategray4", 0x528b8b, 66, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 944 "rgbtab.gperf"
    {"gray54", 0x8a8a8a, 245, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 443 "rgbtab.gperf"
    {"darkslategray3", 0x79cdcd, 116, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 189 "rgbtab.gperf"
    {"xterm163", 0xd700af, 163, 261},
#line 398 "rgbtab.gperf"
    {"gray63", 0xa1a1a1, 247, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 608 "rgbtab.gperf"
    {"darkorchid4", 0x68228b, 54, 5},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 901 "rgbtab.gperf"
    {"lightcyan4", 0x7a8b8b, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 636 "rgbtab.gperf"
    {"gray53", 0x878787, 102, 2},
#line 595 "rgbtab.gperf"
    {"cornsilk4", 0x8b8878, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 399 "rgbtab.gperf"
    {"honeydew4", 0x838b83, 102, 2},
#line 315 "rgbtab.gperf"
    {"darkorchid3", 0x9a32cd, 98, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 800 "rgbtab.gperf"
    {"lightcyan3", 0xb4cdcd, 152, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 877 "rgbtab.gperf"
    {"cornsilk3", 0xcdc8b1, 187, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 690 "rgbtab.gperf"
    {"honeydew3", 0xc1cdc1, 251, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 554 "rgbtab.gperf"
    {"blueviolet", 0x8a2be2, 92, 261},
    {"", 0, 0, 0},
#line 556 "rgbtab.gperf"
    {"steelblue4", 0x36648b, 60, 6},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 403 "rgbtab.gperf"
    {"lightyellow4", 0x8b8b7a, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 307 "rgbtab.gperf"
    {"grey44", 0x707070, 242, 7},
#line 726 "rgbtab.gperf"
    {"bisque4", 0x8b7d6b, 101, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 404 "rgbtab.gperf"
    {"steelblue3", 0x4f94cd, 68, 6},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 680 "rgbtab.gperf"
    {"plum4", 0x8b668b, 96, 5},
#line 717 "rgbtab.gperf"
    {"grey34", 0x575757, 240, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 317 "rgbtab.gperf"
    {"lightyellow3", 0xcdcdb4, 187, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 757 "rgbtab.gperf"
    {"grey43", 0x6e6e6e, 242, 7},
#line 784 "rgbtab.gperf"
    {"bisque3", 0xcdb79e, 181, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 411 "rgbtab.gperf"
    {"coral4", 0x8b3e2f, 94, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 908 "rgbtab.gperf"
    {"plum3", 0xcd96cd, 176, 261},
#line 468 "rgbtab.gperf"
    {"grey33", 0x545454, 240, 256},
    {"", 0, 0, 0},
#line 409 "rgbtab.gperf"
    {"darkseagreen4", 0x698b69, 65, 2},
#line 603 "rgbtab.gperf"
    {"aliceblue", 0xf0f8ff, 231, 263},
#line 815 "rgbtab.gperf"
    {"lightblue4", 0x68838b, 66, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 460 "rgbtab.gperf"
    {"brown4", 0x8b2323, 88, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 826 "rgbtab.gperf"
    {"gray94", 0xf0f0f0, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 396 "rgbtab.gperf"
    {"lightblue3", 0x9ac0cd, 110, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 838 "rgbtab.gperf"
    {"coral3", 0xcd5b45, 167, 257},
    {"", 0, 0, 0},
#line 817 "rgbtab.gperf"
    {"gray84", 0xd6d6d6, 188, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 781 "rgbtab.gperf"
    {"darkseagreen3", 0x9bcd9b, 114, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 941 "rgbtab.gperf"
    {"firebrick", 0xb22222, 124, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 560 "rgbtab.gperf"
    {"brown3", 0xcd3333, 167, 257},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 405 "rgbtab.gperf"
    {"gray93", 0xededed, 255, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 903 "rgbtab.gperf"
    {"gray83", 0xd4d4d4, 188, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 387 "rgbtab.gperf"
    {"cadetblue", 0x5f9ea0, 73, 6},
#line 402 "rgbtab.gperf"
    {"cadetblue1", 0x98f5ff, 123, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 473 "rgbtab.gperf"
    {"cadetblue2", 0x8ee5ee, 117, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 524 "rgbtab.gperf"
    {"royalblue4", 0x27408b, 24, 4},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 309 "rgbtab.gperf"
    {"peachpuff", 0xffdab9, 223, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 741 "rgbtab.gperf"
    {"royalblue3", 0x3a5fcd, 62, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 706 "rgbtab.gperf"
    {"thistle4", 0x8b7b8b, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 322 "rgbtab.gperf"
    {"gray40", 0x666666, 241, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 391 "rgbtab.gperf"
    {"gray30", 0x4d4d4d, 239, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 406 "rgbtab.gperf"
    {"thistle3", 0xcdb5cd, 182, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 664 "rgbtab.gperf"
    {"khaki4", 0x8b864e, 101, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 871 "rgbtab.gperf"
    {"mediumorchid4", 0x7a378b, 96, 5},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 743 "rgbtab.gperf"
    {"khaki3", 0xcdc673, 185, 3},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 660 "rgbtab.gperf"
    {"mediumorchid3", 0xb452cd, 134, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 659 "rgbtab.gperf"
    {"hotpink4", 0x8b3a62, 95, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 430 "rgbtab.gperf"
    {"lemonchiffon4", 0x8b8970, 101, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 325 "rgbtab.gperf"
    {"hotpink3", 0xcd6090, 168, 257},
#line 869 "rgbtab.gperf"
    {"chartreuse", 0x7fff00, 118, 258},
#line 481 "rgbtab.gperf"
    {"chartreuse1", 0x7fff00, 118, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 596 "rgbtab.gperf"
    {"chartreuse2", 0x76ee00, 118, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 676 "rgbtab.gperf"
    {"lavenderblush", 0xfff0f5, 231, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 400 "rgbtab.gperf"
    {"lemonchiffon3", 0xcdc9a5, 187, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 519 "rgbtab.gperf"
    {"deepskyblue4", 0x00688b, 24, 4},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 324 "rgbtab.gperf"
    {"azure4", 0x838b8b, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 353 "rgbtab.gperf"
    {"deepskyblue3", 0x009acd, 32, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 471 "rgbtab.gperf"
    {"azure3", 0xc1cdcd, 251, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 448 "rgbtab.gperf"
    {"peachpuff4", 0x8b7765, 101, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 701 "rgbtab.gperf"
    {"peachpuff3", 0xcdaf95, 180, 259},
#line 810 "rgbtab.gperf"
    {"aquamarine4", 0x458b74, 66, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 382 "rgbtab.gperf"
    {"aquamarine3", 0x66cdaa, 79, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 885 "rgbtab.gperf"
    {"skyblue4", 0x4a708b, 60, 6},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 498 "rgbtab.gperf"
    {"cyan4", 0x008b8b, 30, 6},
    {"", 0, 0, 0},
#line 844 "rgbtab.gperf"
    {"blue4", 0x00008b, 18, 4},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 909 "rgbtab.gperf"
    {"skyblue3", 0x6ca6cd, 74, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 476 "rgbtab.gperf"
    {"cyan3", 0x00cdcd, 44, 262},
    {"", 0, 0, 0},
#line 924 "rgbtab.gperf"
    {"blue3", 0x0000cd, 20, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 730 "rgbtab.gperf"
    {"slateblue4", 0x473c8b, 60, 6},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 663 "rgbtab.gperf"
    {"gray44", 0x707070, 242, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 565 "rgbtab.gperf"
    {"slateblue3", 0x6959cd, 62, 260},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 492 "rgbtab.gperf"
    {"gray34", 0x575757, 240, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 629 "rgbtab.gperf"
    {"lavenderblush4", 0x8b8386, 102, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 586 "rgbtab.gperf"
    {"gray43", 0x6e6e6e, 242, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 873 "rgbtab.gperf"
    {"lavenderblush3", 0xcdc1c5, 251, 263},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 579 "rgbtab.gperf"
    {"gray33", 0x545454, 240, 256},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 737 "rgbtab.gperf"
    {"firebrick4", 0x8b1a1a, 88, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 881 "rgbtab.gperf"
    {"firebrick3", 0xcd2626, 160, 1},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 679 "rgbtab.gperf"
    {"cadetblue4", 0x53868b, 66, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 533 "rgbtab.gperf"
    {"cadetblue3", 0x7ac5cd, 116, 262},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0},
#line 581 "rgbtab.gperf"
    {"chartreuse4", 0x458b00, 64, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 633 "rgbtab.gperf"
    {"mediumpurple4", 0x5d478b, 60, 6},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 517 "rgbtab.gperf"
    {"chartreuse3", 0x66cd00, 76, 258},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 905 "rgbtab.gperf"
    {"navajowhite4", 0x8b795e, 101, 2},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 866 "rgbtab.gperf"
    {"mediumpurple3", 0x8968cd, 98, 261},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0},
#line 598 "rgbtab.gperf"
    {"navajowhite3", 0xcdb38b, 180, 259},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 803 "rgbtab.gperf"
    {"blanchedalmond", 0xffebcd, 224, 263},
#line 290 "rgbtab.gperf"
    {"antiquewhite4", 0x8b8378, 244, 7},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
    {"", 0, 0, 0}, {"", 0, 0, 0}, {"", 0, 0, 0},
#line 540 "rgbtab.gperf"
    {"antiquewhite3", 0xcdc0b0, 181, 261}
  };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH) {
    register int key = colorname_hash(str, len);

    if (key <= MAX_HASH_VALUE && key >= 0) {
      register const char *s = wordlist[key].name;

      if (*str == *s && !strcmp(str + 1, s + 1))
        return &wordlist[key];
    }
  }
  return 0;
}

#line 947 "rgbtab.gperf"
