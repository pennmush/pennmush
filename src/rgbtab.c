/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: gperf --output-file rgbtab.c rgbtab.gperf  */
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

#line 27 "rgbtab.gperf"
struct RGB_COLORMAP;
/* maximum key range = 4677, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
colorname_hash (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,  680,    0,
         5,  930,  910,   90,   70,   20,  300,  280,  501,  262,
       995,  751,  390, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683,  705,  990,  832,
        15,    0,   36,   15,    0,  205, 1374,  810,   15,    0,
        55,   65,  440,  330,  170,   15,   45,  300,  290,   55,
         0,   70,  120,    5,    5, 4683,   35, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683, 4683,
      4683
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[12]];
      /*FALLTHROUGH*/
      case 12:
        hval += asso_values[(unsigned char)str[11]];
      /*FALLTHROUGH*/
      case 11:
      case 10:
      case 9:
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
        hval += asso_values[(unsigned char)str[6]+5];
      /*FALLTHROUGH*/
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct RGB_COLORMAP *
colorname_lookup (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 915,
      MIN_WORD_LENGTH = 3,
      MAX_WORD_LENGTH = 20,
      MIN_HASH_VALUE = 6,
      MAX_HASH_VALUE = 4682
    };

  static const struct RGB_COLORMAP wordlist[] =
    {
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 30 "rgbtab.gperf"
      {"xterm1", 0x800000},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 31 "rgbtab.gperf"
      {"xterm2", 0x008000},
      {"",0}, {"",0}, {"",0},
#line 738 "rgbtab.gperf"
      {"grey1", 0x030303},
#line 758 "rgbtab.gperf"
      {"grey11", 0x1c1c1c},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 778 "rgbtab.gperf"
      {"grey21", 0x363636},
      {"",0},
#line 150 "rgbtab.gperf"
      {"xterm121", 0x87ffaf},
      {"",0},
#line 740 "rgbtab.gperf"
      {"grey2", 0x050505},
#line 760 "rgbtab.gperf"
      {"grey12", 0x1f1f1f},
#line 41 "rgbtab.gperf"
      {"xterm12", 0x0000ff},
#line 250 "rgbtab.gperf"
      {"xterm221", 0xffd75f},
      {"",0},
#line 594 "rgbtab.gperf"
      {"gold1", 0xffd700},
#line 780 "rgbtab.gperf"
      {"grey22", 0x383838},
#line 51 "rgbtab.gperf"
      {"xterm22", 0x005f00},
#line 151 "rgbtab.gperf"
      {"xterm122", 0x87ffd7},
      {"",0}, {"",0},
#line 878 "rgbtab.gperf"
      {"grey71", 0xb5b5b5},
      {"",0},
#line 251 "rgbtab.gperf"
      {"xterm222", 0xffd787},
      {"",0},
#line 595 "rgbtab.gperf"
      {"gold2", 0xeec900},
#line 36 "rgbtab.gperf"
      {"xterm7", 0xc0c0c0},
      {"",0}, {"",0},
#line 376 "rgbtab.gperf"
      {"gold", 0xffd700},
      {"",0},
#line 880 "rgbtab.gperf"
      {"grey72", 0xb8b8b8},
#line 101 "rgbtab.gperf"
      {"xterm72", 0x5faf87},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 750 "rgbtab.gperf"
      {"grey7", 0x121212},
#line 770 "rgbtab.gperf"
      {"grey17", 0x2b2b2b},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 790 "rgbtab.gperf"
      {"grey27", 0x454545},
      {"",0},
#line 156 "rgbtab.gperf"
      {"xterm127", 0xaf00af},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 256 "rgbtab.gperf"
      {"xterm227", 0xffff5f},
      {"",0}, {"",0},
#line 562 "rgbtab.gperf"
      {"green1", 0x00ff00},
#line 40 "rgbtab.gperf"
      {"xterm11", 0xffff00},
#line 140 "rgbtab.gperf"
      {"xterm111", 0x87afff},
      {"",0}, {"",0},
#line 890 "rgbtab.gperf"
      {"grey77", 0xc4c4c4},
#line 50 "rgbtab.gperf"
      {"xterm21", 0x0000ff},
#line 240 "rgbtab.gperf"
      {"xterm211", 0xff87af},
      {"",0},
#line 422 "rgbtab.gperf"
      {"snow1", 0xfffafa},
#line 563 "rgbtab.gperf"
      {"green2", 0x00ee00},
      {"",0},
#line 141 "rgbtab.gperf"
      {"xterm112", 0x87d700},
#line 322 "rgbtab.gperf"
      {"grey", 0xbebebe},
      {"",0},
#line 858 "rgbtab.gperf"
      {"grey61", 0x9c9c9c},
      {"",0},
#line 241 "rgbtab.gperf"
      {"xterm212", 0xff87d7},
      {"",0},
#line 423 "rgbtab.gperf"
      {"snow2", 0xeee9e9},
      {"",0},
#line 100 "rgbtab.gperf"
      {"xterm71", 0x5faf5f},
#line 130 "rgbtab.gperf"
      {"xterm101", 0x87875f},
      {"",0}, {"",0},
#line 860 "rgbtab.gperf"
      {"grey62", 0x9e9e9e},
#line 91 "rgbtab.gperf"
      {"xterm62", 0x5f5fd7},
#line 230 "rgbtab.gperf"
      {"xterm201", 0xff00ff},
#line 626 "rgbtab.gperf"
      {"tan1", 0xffa54f},
      {"",0},
#line 622 "rgbtab.gperf"
      {"wheat1", 0xffe7ba},
      {"",0},
#line 131 "rgbtab.gperf"
      {"xterm102", 0x878787},
#line 627 "rgbtab.gperf"
      {"tan2", 0xee9a49},
      {"",0},
#line 838 "rgbtab.gperf"
      {"grey51", 0x828282},
      {"",0},
#line 231 "rgbtab.gperf"
      {"xterm202", 0xff5f00},
      {"",0}, {"",0},
#line 623 "rgbtab.gperf"
      {"wheat2", 0xeed8ae},
      {"",0},
#line 146 "rgbtab.gperf"
      {"xterm117", 0x87d7ff},
      {"",0}, {"",0},
#line 840 "rgbtab.gperf"
      {"grey52", 0x858585},
#line 81 "rgbtab.gperf"
      {"xterm52", 0x5f0000},
#line 246 "rgbtab.gperf"
      {"xterm217", 0xffafaf},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 362 "rgbtab.gperf"
      {"green", 0x00ff00},
#line 870 "rgbtab.gperf"
      {"grey67", 0xababab},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 136 "rgbtab.gperf"
      {"xterm107", 0x87af5f},
#line 286 "rgbtab.gperf"
      {"snow", 0xfffafa},
      {"",0}, {"",0}, {"",0},
#line 236 "rgbtab.gperf"
      {"xterm207", 0xff5fff},
      {"",0}, {"",0},
#line 35 "rgbtab.gperf"
      {"xterm6", 0x008080},
#line 90 "rgbtab.gperf"
      {"xterm61", 0x5f5faf},
      {"",0}, {"",0},
#line 387 "rgbtab.gperf"
      {"wheat", 0xf5deb3},
#line 850 "rgbtab.gperf"
      {"grey57", 0x919191},
      {"",0},
#line 646 "rgbtab.gperf"
      {"lightsalmon1", 0xffa07a},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 389 "rgbtab.gperf"
      {"tan", 0xd2b48c},
#line 703 "rgbtab.gperf"
      {"magenta1", 0xff00ff},
#line 748 "rgbtab.gperf"
      {"grey6", 0x0f0f0f},
#line 768 "rgbtab.gperf"
      {"grey16", 0x292929},
      {"",0},
#line 647 "rgbtab.gperf"
      {"lightsalmon2", 0xee9572},
      {"",0}, {"",0},
#line 788 "rgbtab.gperf"
      {"grey26", 0x424242},
#line 80 "rgbtab.gperf"
      {"xterm51", 0x00ffff},
#line 155 "rgbtab.gperf"
      {"xterm126", 0xaf0087},
#line 704 "rgbtab.gperf"
      {"magenta2", 0xee00ee},
      {"",0}, {"",0}, {"",0},
#line 255 "rgbtab.gperf"
      {"xterm226", 0xffff00},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 888 "rgbtab.gperf"
      {"grey76", 0xc2c2c2},
#line 643 "rgbtab.gperf"
      {"salmon2", 0xee8262},
      {"",0}, {"",0},
#line 292 "rgbtab.gperf"
      {"linen", 0xfaf0e6},
#line 34 "rgbtab.gperf"
      {"xterm5", 0x800080},
#line 663 "rgbtab.gperf"
      {"tomato2", 0xee5c42},
      {"",0},
#line 670 "rgbtab.gperf"
      {"red1", 0xff0000},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 671 "rgbtab.gperf"
      {"red2", 0xee0000},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 746 "rgbtab.gperf"
      {"grey5", 0x0d0d0d},
#line 766 "rgbtab.gperf"
      {"grey15", 0x262626},
      {"",0},
#line 402 "rgbtab.gperf"
      {"red", 0xff0000},
      {"",0}, {"",0},
#line 786 "rgbtab.gperf"
      {"grey25", 0x404040},
#line 395 "rgbtab.gperf"
      {"lightsalmon", 0xffa07a},
#line 154 "rgbtab.gperf"
      {"xterm125", 0xaf005f},
      {"",0},
#line 943 "rgbtab.gperf"
      {"lightgreen", 0x90ee90},
#line 394 "rgbtab.gperf"
      {"salmon", 0xfa8072},
      {"",0},
#line 254 "rgbtab.gperf"
      {"xterm225", 0xffd7ff},
      {"",0},
#line 598 "rgbtab.gperf"
      {"goldenrod1", 0xffc125},
      {"",0}, {"",0},
#line 145 "rgbtab.gperf"
      {"xterm116", 0x87d7d7},
      {"",0},
#line 599 "rgbtab.gperf"
      {"goldenrod2", 0xeeb422},
#line 886 "rgbtab.gperf"
      {"grey75", 0xbfbfbf},
      {"",0},
#line 245 "rgbtab.gperf"
      {"xterm216", 0xffaf87},
#line 323 "rgbtab.gperf"
      {"lightgrey", 0xd3d3d3},
      {"",0},
#line 400 "rgbtab.gperf"
      {"tomato", 0xff6347},
#line 642 "rgbtab.gperf"
      {"salmon1", 0xff8c69},
      {"",0},
#line 378 "rgbtab.gperf"
      {"goldenrod", 0xdaa520},
      {"",0},
#line 868 "rgbtab.gperf"
      {"grey66", 0xa8a8a8},
#line 662 "rgbtab.gperf"
      {"tomato1", 0xff6347},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 591 "rgbtab.gperf"
      {"yellow2", 0xeeee00},
#line 135 "rgbtab.gperf"
      {"xterm106", 0x87af00},
      {"",0},
#line 336 "rgbtab.gperf"
      {"dodgerblue", 0x1e90ff},
#line 486 "rgbtab.gperf"
      {"dodgerblue1", 0x1e90ff},
      {"",0},
#line 235 "rgbtab.gperf"
      {"xterm206", 0xff5fd7},
      {"",0}, {"",0},
#line 487 "rgbtab.gperf"
      {"dodgerblue2", 0x1c86ee},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 848 "rgbtab.gperf"
      {"grey56", 0x8f8f8f},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 144 "rgbtab.gperf"
      {"xterm115", 0x87d7af},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 244 "rgbtab.gperf"
      {"xterm215", 0xffaf5f},
      {"",0},
#line 311 "rgbtab.gperf"
      {"white", 0xffffff},
#line 375 "rgbtab.gperf"
      {"yellow", 0xffff00},
      {"",0}, {"",0}, {"",0},
#line 190 "rgbtab.gperf"
      {"xterm161", 0xd7005f},
#line 866 "rgbtab.gperf"
      {"grey65", 0xa6a6a6},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 134 "rgbtab.gperf"
      {"xterm105", 0x8787ff},
      {"",0},
#line 191 "rgbtab.gperf"
      {"xterm162", 0xd70087},
      {"",0},
#line 590 "rgbtab.gperf"
      {"yellow1", 0xffff00},
#line 234 "rgbtab.gperf"
      {"xterm205", 0xff5faf},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 170 "rgbtab.gperf"
      {"xterm141", 0xaf87ff},
      {"",0}, {"",0},
#line 846 "rgbtab.gperf"
      {"grey55", 0x8c8c8c},
#line 325 "rgbtab.gperf"
      {"midnightblue", 0x191970},
#line 270 "rgbtab.gperf"
      {"xterm241", 0x626262},
      {"",0}, {"",0},
#line 382 "rgbtab.gperf"
      {"saddlebrown", 0x8b4513},
#line 316 "rgbtab.gperf"
      {"dimgrey", 0x696969},
#line 171 "rgbtab.gperf"
      {"xterm142", 0xafaf00},
      {"",0}, {"",0},
#line 918 "rgbtab.gperf"
      {"grey91", 0xe8e8e8},
      {"",0},
#line 271 "rgbtab.gperf"
      {"xterm242", 0x6c6c6c},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 160 "rgbtab.gperf"
      {"xterm131", 0xaf5f5f},
      {"",0},
#line 196 "rgbtab.gperf"
      {"xterm167", 0xd75f5f},
#line 920 "rgbtab.gperf"
      {"grey92", 0xebebeb},
#line 121 "rgbtab.gperf"
      {"xterm92", 0x8700d7},
#line 260 "rgbtab.gperf"
      {"xterm231", 0xffffff},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 161 "rgbtab.gperf"
      {"xterm132", 0xaf5f87},
#line 546 "rgbtab.gperf"
      {"darkseagreen1", 0xc1ffc1},
      {"",0},
#line 898 "rgbtab.gperf"
      {"grey81", 0xcfcfcf},
#line 696 "rgbtab.gperf"
      {"maroon2", 0xee30a7},
#line 261 "rgbtab.gperf"
      {"xterm232", 0x080808},
#line 310 "rgbtab.gperf"
      {"mistyrose", 0xffe4e1},
#line 466 "rgbtab.gperf"
      {"mistyrose1", 0xffe4e1},
      {"",0}, {"",0},
#line 176 "rgbtab.gperf"
      {"xterm147", 0xafafff},
#line 547 "rgbtab.gperf"
      {"darkseagreen2", 0xb4eeb4},
#line 467 "rgbtab.gperf"
      {"mistyrose2", 0xeed5d2},
#line 900 "rgbtab.gperf"
      {"grey82", 0xd1d1d1},
#line 111 "rgbtab.gperf"
      {"xterm82", 0x5fff00},
#line 276 "rgbtab.gperf"
      {"xterm247", 0x9e9e9e},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 45 "rgbtab.gperf"
      {"xterm16", 0x000000},
      {"",0},
#line 930 "rgbtab.gperf"
      {"grey97", 0xf7f7f7},
      {"",0}, {"",0},
#line 55 "rgbtab.gperf"
      {"xterm26", 0x005fd7},
      {"",0},
#line 454 "rgbtab.gperf"
      {"ivory1", 0xfffff0},
      {"",0},
#line 166 "rgbtab.gperf"
      {"xterm137", 0xaf875f},
      {"",0}, {"",0},
#line 408 "rgbtab.gperf"
      {"maroon", 0xb03060},
      {"",0},
#line 266 "rgbtab.gperf"
      {"xterm237", 0x3a3a3a},
      {"",0},
#line 333 "rgbtab.gperf"
      {"mediumblue", 0x0000cd},
#line 455 "rgbtab.gperf"
      {"ivory2", 0xeeeee0},
#line 120 "rgbtab.gperf"
      {"xterm91", 0x8700af},
      {"",0},
#line 105 "rgbtab.gperf"
      {"xterm76", 0x5fd700},
      {"",0},
#line 910 "rgbtab.gperf"
      {"grey87", 0xdedede},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 695 "rgbtab.gperf"
      {"maroon1", 0xff34b3},
      {"",0}, {"",0}, {"",0},
#line 285 "rgbtab.gperf"
      {"indigo", 0x4b0082},
      {"",0},
#line 355 "rgbtab.gperf"
      {"darkseagreen", 0x8fbc8f},
      {"",0}, {"",0}, {"",0},
#line 110 "rgbtab.gperf"
      {"xterm81", 0x5fd7ff},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 357 "rgbtab.gperf"
      {"mediumseagreen", 0x3cb371},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 287 "rgbtab.gperf"
      {"ghostwhite", 0xf8f8ff},
      {"",0}, {"",0},
#line 220 "rgbtab.gperf"
      {"xterm191", 0xd7ff5f},
      {"",0},
#line 582 "rgbtab.gperf"
      {"lightgoldenrod1", 0xffec8b},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 583 "rgbtab.gperf"
      {"lightgoldenrod2", 0xeedc82},
      {"",0}, {"",0},
#line 221 "rgbtab.gperf"
      {"xterm192", 0xd7ff87},
#line 95 "rgbtab.gperf"
      {"xterm66", 0x5f8787},
#line 195 "rgbtab.gperf"
      {"xterm166", 0xd75f00},
      {"",0}, {"",0}, {"",0},
#line 377 "rgbtab.gperf"
      {"lightgoldenrod", 0xeedd82},
#line 301 "rgbtab.gperf"
      {"ivory", 0xfffff0},
      {"",0}, {"",0}, {"",0},
#line 326 "rgbtab.gperf"
      {"navy", 0x000080},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 175 "rgbtab.gperf"
      {"xterm146", 0xafafd7},
#line 85 "rgbtab.gperf"
      {"xterm56", 0x5f00d7},
      {"",0},
#line 719 "rgbtab.gperf"
      {"darkorchid1", 0xbf3eff},
      {"",0},
#line 275 "rgbtab.gperf"
      {"xterm246", 0x949494},
      {"",0}, {"",0},
#line 720 "rgbtab.gperf"
      {"darkorchid2", 0xb23aee},
      {"",0},
#line 226 "rgbtab.gperf"
      {"xterm197", 0xff005f},
      {"",0}, {"",0},
#line 928 "rgbtab.gperf"
      {"grey96", 0xf5f5f5},
      {"",0}, {"",0}, {"",0},
#line 416 "rgbtab.gperf"
      {"darkorchid", 0x9932cc},
      {"",0}, {"",0},
#line 165 "rgbtab.gperf"
      {"xterm136", 0xaf8700},
#line 314 "rgbtab.gperf"
      {"darkslategrey", 0x2f4f4f},
#line 194 "rgbtab.gperf"
      {"xterm165", 0xd700ff},
#line 412 "rgbtab.gperf"
      {"violet", 0xee82ee},
#line 558 "rgbtab.gperf"
      {"springgreen1", 0x00ff7f},
#line 265 "rgbtab.gperf"
      {"xterm236", 0x303030},
      {"",0}, {"",0},
#line 367 "rgbtab.gperf"
      {"yellowgreen", 0x9acd32},
      {"",0}, {"",0}, {"",0},
#line 373 "rgbtab.gperf"
      {"lightgoldenrodyellow", 0xfafad2},
#line 908 "rgbtab.gperf"
      {"grey86", 0xdbdbdb},
#line 559 "rgbtab.gperf"
      {"springgreen2", 0x00ee76},
      {"",0}, {"",0},
#line 699 "rgbtab.gperf"
      {"violetred1", 0xff3e96},
      {"",0}, {"",0},
#line 174 "rgbtab.gperf"
      {"xterm145", 0xafafaf},
      {"",0},
#line 700 "rgbtab.gperf"
      {"violetred2", 0xee3a8c},
      {"",0}, {"",0},
#line 274 "rgbtab.gperf"
      {"xterm245", 0x8a8a8a},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 410 "rgbtab.gperf"
      {"violetred", 0xd02090},
      {"",0},
#line 926 "rgbtab.gperf"
      {"grey95", 0xf2f2f2},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 164 "rgbtab.gperf"
      {"xterm135", 0xaf5fff},
#line 341 "rgbtab.gperf"
      {"lightsteelblue", 0xb0c4de},
#line 510 "rgbtab.gperf"
      {"lightsteelblue1", 0xcae1ff},
      {"",0}, {"",0},
#line 264 "rgbtab.gperf"
      {"xterm235", 0x262626},
      {"",0},
#line 511 "rgbtab.gperf"
      {"lightsteelblue2", 0xbcd2ee},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 683 "rgbtab.gperf"
      {"pink1", 0xffb5c5},
#line 906 "rgbtab.gperf"
      {"grey85", 0xd9d9d9},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 360 "rgbtab.gperf"
      {"springgreen", 0x00ff7f},
      {"",0}, {"",0},
#line 180 "rgbtab.gperf"
      {"xterm151", 0xafd7af},
#line 684 "rgbtab.gperf"
      {"pink2", 0xeea9b8},
      {"",0}, {"",0}, {"",0},
#line 280 "rgbtab.gperf"
      {"xterm251", 0xc6c6c6},
      {"",0}, {"",0},
#line 368 "rgbtab.gperf"
      {"forestgreen", 0x228b22},
      {"",0},
#line 181 "rgbtab.gperf"
      {"xterm152", 0xafd7d7},
#line 288 "rgbtab.gperf"
      {"whitesmoke", 0xf5f5f5},
#line 397 "rgbtab.gperf"
      {"darkorange", 0xff8c00},
#line 654 "rgbtab.gperf"
      {"darkorange1", 0xff7f00},
      {"",0},
#line 281 "rgbtab.gperf"
      {"xterm252", 0xd0d0d0},
      {"",0}, {"",0},
#line 655 "rgbtab.gperf"
      {"darkorange2", 0xee7600},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 225 "rgbtab.gperf"
      {"xterm196", 0xff0000},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 186 "rgbtab.gperf"
      {"xterm157", 0xafffaf},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 938 "rgbtab.gperf"
      {"darkgray", 0xa9a9a9},
      {"",0}, {"",0}, {"",0},
#line 715 "rgbtab.gperf"
      {"mediumorchid1", 0xe066ff},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 38 "rgbtab.gperf"
      {"xterm9", 0xff0000},
      {"",0},
#line 716 "rgbtab.gperf"
      {"mediumorchid2", 0xd15fee},
      {"",0}, {"",0}, {"",0},
#line 415 "rgbtab.gperf"
      {"mediumorchid", 0xba55d3},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 224 "rgbtab.gperf"
      {"xterm195", 0xd7ffff},
      {"",0},
#line 754 "rgbtab.gperf"
      {"grey9", 0x171717},
#line 774 "rgbtab.gperf"
      {"grey19", 0x303030},
#line 942 "rgbtab.gperf"
      {"darkred", 0x8b0000},
      {"",0}, {"",0}, {"",0},
#line 794 "rgbtab.gperf"
      {"grey29", 0x4a4a4a},
      {"",0},
#line 158 "rgbtab.gperf"
      {"xterm129", 0xaf00ff},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 258 "rgbtab.gperf"
      {"xterm229", 0xffffaf},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 44 "rgbtab.gperf"
      {"xterm15", 0xffffff},
      {"",0}, {"",0},
#line 894 "rgbtab.gperf"
      {"grey79", 0xc9c9c9},
      {"",0},
#line 54 "rgbtab.gperf"
      {"xterm25", 0x005faf},
      {"",0}, {"",0},
#line 37 "rgbtab.gperf"
      {"xterm8", 0x808080},
      {"",0}, {"",0},
#line 329 "rgbtab.gperf"
      {"darkslateblue", 0x483d8b},
      {"",0}, {"",0},
#line 339 "rgbtab.gperf"
      {"lightskyblue", 0x87cefa},
#line 502 "rgbtab.gperf"
      {"lightskyblue1", 0xb0e2ff},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 104 "rgbtab.gperf"
      {"xterm75", 0x5fafff},
#line 125 "rgbtab.gperf"
      {"xterm96", 0x875f87},
#line 752 "rgbtab.gperf"
      {"grey8", 0x141414},
#line 772 "rgbtab.gperf"
      {"grey18", 0x2e2e2e},
      {"",0},
#line 503 "rgbtab.gperf"
      {"lightskyblue2", 0xa4d3ee},
      {"",0},
#line 574 "rgbtab.gperf"
      {"darkolivegreen1", 0xcaff70},
#line 792 "rgbtab.gperf"
      {"grey28", 0x474747},
      {"",0},
#line 157 "rgbtab.gperf"
      {"xterm128", 0xaf00d7},
      {"",0},
#line 575 "rgbtab.gperf"
      {"darkolivegreen2", 0xbcee68},
#line 419 "rgbtab.gperf"
      {"purple", 0xa020f0},
      {"",0},
#line 257 "rgbtab.gperf"
      {"xterm228", 0xffff87},
      {"",0},
#line 687 "rgbtab.gperf"
      {"lightpink1", 0xffaeb9},
      {"",0}, {"",0},
#line 148 "rgbtab.gperf"
      {"xterm119", 0x87ff5f},
#line 115 "rgbtab.gperf"
      {"xterm86", 0x5fffd7},
#line 688 "rgbtab.gperf"
      {"lightpink2", 0xeea2ad},
#line 892 "rgbtab.gperf"
      {"grey78", 0xc7c7c7},
      {"",0},
#line 248 "rgbtab.gperf"
      {"xterm219", 0xffafff},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 185 "rgbtab.gperf"
      {"xterm156", 0xafff87},
      {"",0},
#line 874 "rgbtab.gperf"
      {"grey69", 0xb0b0b0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 724 "rgbtab.gperf"
      {"purple2", 0x912cee},
#line 138 "rgbtab.gperf"
      {"xterm109", 0x87afaf},
#line 347 "rgbtab.gperf"
      {"turquoise", 0x40e0d0},
#line 530 "rgbtab.gperf"
      {"turquoise1", 0x00f5ff},
      {"",0}, {"",0},
#line 238 "rgbtab.gperf"
      {"xterm209", 0xff875f},
      {"",0},
#line 531 "rgbtab.gperf"
      {"turquoise2", 0x00e5ee},
      {"",0}, {"",0},
#line 94 "rgbtab.gperf"
      {"xterm65", 0x5f875f},
      {"",0}, {"",0},
#line 854 "rgbtab.gperf"
      {"grey59", 0x969696},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 48 "rgbtab.gperf"
      {"xterm19", 0x0000af},
#line 147 "rgbtab.gperf"
      {"xterm118", 0x87ff00},
#line 354 "rgbtab.gperf"
      {"darkolivegreen", 0x556b2f},
      {"",0}, {"",0},
#line 58 "rgbtab.gperf"
      {"xterm29", 0x00875f},
#line 247 "rgbtab.gperf"
      {"xterm218", 0xffafd7},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 84 "rgbtab.gperf"
      {"xterm55", 0x5f00af},
#line 184 "rgbtab.gperf"
      {"xterm155", 0xafff5f},
      {"",0},
#line 872 "rgbtab.gperf"
      {"grey68", 0xadadad},
      {"",0}, {"",0},
#line 284 "rgbtab.gperf"
      {"xterm255", 0xeeeeee},
      {"",0}, {"",0},
#line 108 "rgbtab.gperf"
      {"xterm79", 0x5fd7af},
#line 137 "rgbtab.gperf"
      {"xterm108", 0x87af87},
      {"",0}, {"",0}, {"",0},
#line 723 "rgbtab.gperf"
      {"purple1", 0x9b30ff},
#line 237 "rgbtab.gperf"
      {"xterm208", 0xff8700},
#line 602 "rgbtab.gperf"
      {"darkgoldenrod1", 0xffb90f},
#line 343 "rgbtab.gperf"
      {"powderblue", 0xb0e0e6},
      {"",0}, {"",0}, {"",0},
#line 603 "rgbtab.gperf"
      {"darkgoldenrod2", 0xeead0e},
      {"",0},
#line 852 "rgbtab.gperf"
      {"grey58", 0x949494},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 379 "rgbtab.gperf"
      {"darkgoldenrod", 0xb8860b},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 737 "rgbtab.gperf"
      {"gray1", 0x030303},
#line 757 "rgbtab.gperf"
      {"gray11", 0x1c1c1c},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 777 "rgbtab.gperf"
      {"gray21", 0x363636},
      {"",0}, {"",0}, {"",0},
#line 739 "rgbtab.gperf"
      {"gray2", 0x050505},
#line 759 "rgbtab.gperf"
      {"gray12", 0x1f1f1f},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 779 "rgbtab.gperf"
      {"gray22", 0x383838},
      {"",0}, {"",0},
#line 413 "rgbtab.gperf"
      {"plum", 0xdda0dd},
#line 711 "rgbtab.gperf"
      {"plum1", 0xffbbff},
#line 877 "rgbtab.gperf"
      {"gray71", 0xb5b5b5},
#line 98 "rgbtab.gperf"
      {"xterm69", 0x5f87ff},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 679 "rgbtab.gperf"
      {"hotpink1", 0xff6eb4},
      {"",0},
#line 712 "rgbtab.gperf"
      {"plum2", 0xeeaeee},
#line 879 "rgbtab.gperf"
      {"gray72", 0xb8b8b8},
      {"",0}, {"",0},
#line 210 "rgbtab.gperf"
      {"xterm181", 0xd7afaf},
      {"",0}, {"",0}, {"",0},
#line 680 "rgbtab.gperf"
      {"hotpink2", 0xee6aa7},
      {"",0},
#line 749 "rgbtab.gperf"
      {"gray7", 0x121212},
#line 769 "rgbtab.gperf"
      {"gray17", 0x2b2b2b},
#line 88 "rgbtab.gperf"
      {"xterm59", 0x5f5f5f},
      {"",0},
#line 211 "rgbtab.gperf"
      {"xterm182", 0xd7afd7},
#line 409 "rgbtab.gperf"
      {"mediumvioletred", 0xc71585},
#line 789 "rgbtab.gperf"
      {"gray27", 0x454545},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 39 "rgbtab.gperf"
      {"xterm10", 0x00ff00},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 49 "rgbtab.gperf"
      {"xterm20", 0x0000d7},
      {"",0}, {"",0}, {"",0},
#line 889 "rgbtab.gperf"
      {"gray77", 0xc4c4c4},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 396 "rgbtab.gperf"
      {"orange", 0xffa500},
      {"",0}, {"",0},
#line 321 "rgbtab.gperf"
      {"gray", 0xbebebe},
#line 417 "rgbtab.gperf"
      {"darkviolet", 0x9400d3},
#line 857 "rgbtab.gperf"
      {"gray61", 0x9c9c9c},
#line 99 "rgbtab.gperf"
      {"xterm70", 0x5faf00},
      {"",0},
#line 216 "rgbtab.gperf"
      {"xterm187", 0xd7d7af},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 859 "rgbtab.gperf"
      {"gray62", 0x9e9e9e},
#line 615 "rgbtab.gperf"
      {"sienna2", 0xee7942},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 837 "rgbtab.gperf"
      {"gray51", 0x828282},
#line 651 "rgbtab.gperf"
      {"orange2", 0xee9a00},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 839 "rgbtab.gperf"
      {"gray52", 0x858585},
      {"",0}, {"",0}, {"",0},
#line 198 "rgbtab.gperf"
      {"xterm169", 0xd75faf},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 869 "rgbtab.gperf"
      {"gray67", 0xababab},
#line 290 "rgbtab.gperf"
      {"floralwhite", 0xfffaf0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 89 "rgbtab.gperf"
      {"xterm60", 0x5f5f87},
#line 178 "rgbtab.gperf"
      {"xterm149", 0xafd75f},
      {"",0},
#line 666 "rgbtab.gperf"
      {"orangered1", 0xff4500},
      {"",0},
#line 614 "rgbtab.gperf"
      {"sienna1", 0xff8247},
#line 278 "rgbtab.gperf"
      {"xterm249", 0xb2b2b2},
      {"",0},
#line 667 "rgbtab.gperf"
      {"orangered2", 0xee4000},
#line 849 "rgbtab.gperf"
      {"gray57", 0x919191},
#line 364 "rgbtab.gperf"
      {"mediumspringgreen", 0x00fa9a},
      {"",0}, {"",0}, {"",0},
#line 934 "rgbtab.gperf"
      {"grey99", 0xfcfcfc},
#line 650 "rgbtab.gperf"
      {"orange1", 0xffa500},
#line 411 "rgbtab.gperf"
      {"magenta", 0xff00ff},
#line 401 "rgbtab.gperf"
      {"orangered", 0xff4500},
#line 747 "rgbtab.gperf"
      {"gray6", 0x0f0f0f},
#line 767 "rgbtab.gperf"
      {"gray16", 0x292929},
#line 79 "rgbtab.gperf"
      {"xterm50", 0x00ffd7},
#line 168 "rgbtab.gperf"
      {"xterm139", 0xaf87af},
#line 318 "rgbtab.gperf"
      {"slategrey", 0x708090},
#line 197 "rgbtab.gperf"
      {"xterm168", 0xd75f87},
#line 787 "rgbtab.gperf"
      {"gray26", 0x424242},
      {"",0},
#line 268 "rgbtab.gperf"
      {"xterm239", 0x4e4e4e},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 124 "rgbtab.gperf"
      {"xterm95", 0x875f5f},
      {"",0}, {"",0},
#line 914 "rgbtab.gperf"
      {"grey89", 0xe3e3e3},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 887 "rgbtab.gperf"
      {"gray76", 0xc2c2c2},
      {"",0},
#line 177 "rgbtab.gperf"
      {"xterm148", 0xafd700},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 277 "rgbtab.gperf"
      {"xterm248", 0xa8a8a8},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 114 "rgbtab.gperf"
      {"xterm85", 0x5fffaf},
#line 215 "rgbtab.gperf"
      {"xterm186", 0xd7d787},
      {"",0},
#line 932 "rgbtab.gperf"
      {"grey98", 0xfafafa},
      {"",0}, {"",0}, {"",0},
#line 745 "rgbtab.gperf"
      {"gray5", 0x0d0d0d},
#line 765 "rgbtab.gperf"
      {"gray15", 0x262626},
      {"",0},
#line 167 "rgbtab.gperf"
      {"xterm138", 0xaf8787},
      {"",0}, {"",0},
#line 785 "rgbtab.gperf"
      {"gray25", 0x404040},
      {"",0},
#line 267 "rgbtab.gperf"
      {"xterm238", 0x444444},
#line 384 "rgbtab.gperf"
      {"peru", 0xcd853f},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 912 "rgbtab.gperf"
      {"grey88", 0xe0e0e0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 885 "rgbtab.gperf"
      {"gray75", 0xbfbfbf},
#line 420 "rgbtab.gperf"
      {"mediumpurple", 0x9370db},
#line 727 "rgbtab.gperf"
      {"mediumpurple1", 0xab82ff},
#line 324 "rgbtab.gperf"
      {"lightgray", 0xd3d3d3},
      {"",0},
#line 818 "rgbtab.gperf"
      {"grey41", 0x696969},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 867 "rgbtab.gperf"
      {"gray66", 0xa8a8a8},
      {"",0},
#line 728 "rgbtab.gperf"
      {"mediumpurple2", 0x9f79ee},
#line 214 "rgbtab.gperf"
      {"xterm185", 0xd7d75f},
      {"",0},
#line 820 "rgbtab.gperf"
      {"grey42", 0x6b6b6b},
#line 71 "rgbtab.gperf"
      {"xterm42", 0x00d787},
#line 345 "rgbtab.gperf"
      {"darkturquoise", 0x00ced1},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 798 "rgbtab.gperf"
      {"grey31", 0x4f4f4f},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 847 "rgbtab.gperf"
      {"gray56", 0x8f8f8f},
#line 128 "rgbtab.gperf"
      {"xterm99", 0x875fff},
#line 228 "rgbtab.gperf"
      {"xterm199", 0xff00af},
      {"",0}, {"",0},
#line 800 "rgbtab.gperf"
      {"grey32", 0x525252},
#line 61 "rgbtab.gperf"
      {"xterm32", 0x0087d7},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 346 "rgbtab.gperf"
      {"mediumturquoise", 0x48d1cc},
#line 830 "rgbtab.gperf"
      {"grey47", 0x787878},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 865 "rgbtab.gperf"
      {"gray65", 0xa6a6a6},
#line 118 "rgbtab.gperf"
      {"xterm89", 0x87005f},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 70 "rgbtab.gperf"
      {"xterm41", 0x00d75f},
#line 372 "rgbtab.gperf"
      {"palegoldenrod", 0xeee8aa},
      {"",0}, {"",0},
#line 810 "rgbtab.gperf"
      {"grey37", 0x5e5e5e},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 845 "rgbtab.gperf"
      {"gray55", 0x8c8c8c},
      {"",0},
#line 227 "rgbtab.gperf"
      {"xterm198", 0xff0087},
      {"",0}, {"",0}, {"",0},
#line 315 "rgbtab.gperf"
      {"dimgray", 0x696969},
#line 200 "rgbtab.gperf"
      {"xterm171", 0xd75fff},
      {"",0}, {"",0},
#line 917 "rgbtab.gperf"
      {"gray91", 0xe8e8e8},
#line 60 "rgbtab.gperf"
      {"xterm31", 0x0087af},
      {"",0}, {"",0},
#line 306 "rgbtab.gperf"
      {"azure", 0xf0ffff},
#line 470 "rgbtab.gperf"
      {"azure1", 0xf0ffff},
      {"",0},
#line 201 "rgbtab.gperf"
      {"xterm172", 0xd78700},
#line 629 "rgbtab.gperf"
      {"tan4", 0x8b5a2b},
      {"",0},
#line 919 "rgbtab.gperf"
      {"gray92", 0xebebeb},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 471 "rgbtab.gperf"
      {"azure2", 0xe0eeee},
#line 46 "rgbtab.gperf"
      {"xterm17", 0x00005f},
#line 658 "rgbtab.gperf"
      {"coral1", 0xff7256},
      {"",0}, {"",0},
#line 897 "rgbtab.gperf"
      {"gray81", 0xcfcfcf},
#line 56 "rgbtab.gperf"
      {"xterm27", 0x005fff},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 659 "rgbtab.gperf"
      {"coral2", 0xee6a50},
#line 628 "rgbtab.gperf"
      {"tan3", 0xcd853f},
      {"",0},
#line 899 "rgbtab.gperf"
      {"gray82", 0xd1d1d1},
#line 398 "rgbtab.gperf"
      {"coral", 0xff7f50},
      {"",0},
#line 691 "rgbtab.gperf"
      {"palevioletred1", 0xff82ab},
      {"",0}, {"",0},
#line 106 "rgbtab.gperf"
      {"xterm77", 0x5fd75f},
#line 206 "rgbtab.gperf"
      {"xterm177", 0xd787ff},
#line 692 "rgbtab.gperf"
      {"palevioletred2", 0xee799f},
#line 610 "rgbtab.gperf"
      {"indianred1", 0xff6a6a},
#line 929 "rgbtab.gperf"
      {"gray97", 0xf7f7f7},
      {"",0}, {"",0}, {"",0},
#line 611 "rgbtab.gperf"
      {"indianred2", 0xee6363},
      {"",0}, {"",0},
#line 407 "rgbtab.gperf"
      {"palevioletred", 0xdb7093},
      {"",0},
#line 570 "rgbtab.gperf"
      {"olivedrab1", 0xc0ff3e},
      {"",0},
#line 119 "rgbtab.gperf"
      {"xterm90", 0x870087},
#line 47 "rgbtab.gperf"
      {"xterm18", 0x000087},
#line 381 "rgbtab.gperf"
      {"indianred", 0xcd5c5c},
#line 571 "rgbtab.gperf"
      {"olivedrab2", 0xb3ee3a},
      {"",0}, {"",0},
#line 57 "rgbtab.gperf"
      {"xterm28", 0x008700},
      {"",0}, {"",0},
#line 909 "rgbtab.gperf"
      {"gray87", 0xdedede},
#line 390 "rgbtab.gperf"
      {"chocolate", 0xd2691e},
#line 630 "rgbtab.gperf"
      {"chocolate1", 0xff7f24},
#line 188 "rgbtab.gperf"
      {"xterm159", 0xafffff},
      {"",0},
#line 828 "rgbtab.gperf"
      {"grey46", 0x757575},
      {"",0},
#line 631 "rgbtab.gperf"
      {"chocolate2", 0xee7621},
#line 426 "rgbtab.gperf"
      {"seashell1", 0xfff5ee},
      {"",0}, {"",0},
#line 109 "rgbtab.gperf"
      {"xterm80", 0x5fd7d7},
#line 107 "rgbtab.gperf"
      {"xterm78", 0x5fd787},
#line 427 "rgbtab.gperf"
      {"seashell2", 0xeee5de},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 538 "rgbtab.gperf"
      {"darkslategray1", 0x97ffff},
      {"",0}, {"",0},
#line 303 "rgbtab.gperf"
      {"seashell", 0xfff5ee},
      {"",0},
#line 539 "rgbtab.gperf"
      {"darkslategray2", 0x8deeee},
#line 808 "rgbtab.gperf"
      {"grey36", 0x5c5c5c},
#line 96 "rgbtab.gperf"
      {"xterm67", 0x5f87af},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 673 "rgbtab.gperf"
      {"red4", 0x8b0000},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 187 "rgbtab.gperf"
      {"xterm158", 0xafffd7},
      {"",0},
#line 826 "rgbtab.gperf"
      {"grey45", 0x737373},
#line 86 "rgbtab.gperf"
      {"xterm57", 0x5f00ff},
      {"",0}, {"",0}, {"",0},
#line 638 "rgbtab.gperf"
      {"brown1", 0xff4040},
      {"",0}, {"",0},
#line 672 "rgbtab.gperf"
      {"red3", 0xcd0000},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 601 "rgbtab.gperf"
      {"goldenrod4", 0x8b6914},
#line 639 "rgbtab.gperf"
      {"brown2", 0xee3b3b},
      {"",0},
#line 97 "rgbtab.gperf"
      {"xterm68", 0x5f87d7},
      {"",0}, {"",0},
#line 806 "rgbtab.gperf"
      {"grey35", 0x595959},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 414 "rgbtab.gperf"
      {"orchid", 0xda70d6},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 205 "rgbtab.gperf"
      {"xterm176", 0xd787d7},
      {"",0},
#line 600 "rgbtab.gperf"
      {"goldenrod3", 0xcd9b1d},
#line 927 "rgbtab.gperf"
      {"gray96", 0xf5f5f5},
#line 399 "rgbtab.gperf"
      {"lightcoral", 0xf08080},
#line 87 "rgbtab.gperf"
      {"xterm58", 0x5f5f00},
#line 708 "rgbtab.gperf"
      {"orchid2", 0xee7ae9},
      {"",0},
#line 489 "rgbtab.gperf"
      {"dodgerblue4", 0x104e8b},
      {"",0}, {"",0},
#line 313 "rgbtab.gperf"
      {"darkslategray", 0x2f4f4f},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 907 "rgbtab.gperf"
      {"gray86", 0xdbdbdb},
      {"",0}, {"",0}, {"",0},
#line 392 "rgbtab.gperf"
      {"brown", 0xa52a2a},
#line 488 "rgbtab.gperf"
      {"dodgerblue3", 0x1874cd},
      {"",0}, {"",0}, {"",0},
#line 446 "rgbtab.gperf"
      {"lemonchiffon1", 0xfffacd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 204 "rgbtab.gperf"
      {"xterm175", 0xd787af},
      {"",0},
#line 447 "rgbtab.gperf"
      {"lemonchiffon2", 0xeee9bf},
#line 925 "rgbtab.gperf"
      {"gray95", 0xf2f2f2},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 707 "rgbtab.gperf"
      {"orchid1", 0xff83fa},
      {"",0}, {"",0},
#line 43 "rgbtab.gperf"
      {"xterm14", 0x00ffff},
      {"",0}, {"",0},
#line 386 "rgbtab.gperf"
      {"beige", 0xf5f5dc},
      {"",0},
#line 53 "rgbtab.gperf"
      {"xterm24", 0x005f87},
      {"",0}, {"",0}, {"",0},
#line 905 "rgbtab.gperf"
      {"gray85", 0xd9d9d9},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 344 "rgbtab.gperf"
      {"paleturquoise", 0xafeeee},
#line 522 "rgbtab.gperf"
      {"paleturquoise1", 0xbbffff},
      {"",0}, {"",0},
#line 103 "rgbtab.gperf"
      {"xterm74", 0x5fafd7},
      {"",0},
#line 523 "rgbtab.gperf"
      {"paleturquoise2", 0xaeeeee},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 302 "rgbtab.gperf"
      {"lemonchiffon", 0xfffacd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 469 "rgbtab.gperf"
      {"mistyrose4", 0x8b7d7b},
      {"",0},
#line 42 "rgbtab.gperf"
      {"xterm13", 0xff00ff},
      {"",0}, {"",0}, {"",0},
#line 940 "rgbtab.gperf"
      {"darkcyan", 0x008b8b},
#line 52 "rgbtab.gperf"
      {"xterm23", 0x005f5f},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 75 "rgbtab.gperf"
      {"xterm46", 0x00ff00},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 468 "rgbtab.gperf"
      {"mistyrose3", 0xcdb7b5},
      {"",0},
#line 102 "rgbtab.gperf"
      {"xterm73", 0x5fafaf},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 93 "rgbtab.gperf"
      {"xterm64", 0x5f8700},
      {"",0},
#line 65 "rgbtab.gperf"
      {"xterm36", 0x00af87},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 753 "rgbtab.gperf"
      {"gray9", 0x171717},
#line 773 "rgbtab.gperf"
      {"gray19", 0x303030},
#line 83 "rgbtab.gperf"
      {"xterm54", 0x5f0087},
      {"",0}, {"",0}, {"",0},
#line 793 "rgbtab.gperf"
      {"gray29", 0x4a4a4a},
      {"",0}, {"",0},
#line 335 "rgbtab.gperf"
      {"blue", 0x0000ff},
#line 482 "rgbtab.gperf"
      {"blue1", 0x0000ff},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 393 "rgbtab.gperf"
      {"darksalmon", 0xe9967a},
      {"",0},
#line 126 "rgbtab.gperf"
      {"xterm97", 0x875faf},
      {"",0}, {"",0},
#line 483 "rgbtab.gperf"
      {"blue2", 0x0000ee},
#line 893 "rgbtab.gperf"
      {"gray79", 0xc9c9c9},
#line 92 "rgbtab.gperf"
      {"xterm63", 0x5f5fff},
      {"",0},
#line 405 "rgbtab.gperf"
      {"pink", 0xffc0cb},
#line 585 "rgbtab.gperf"
      {"lightgoldenrod4", 0x8b814c},
#line 296 "rgbtab.gperf"
      {"bisque", 0xffe4c4},
      {"",0}, {"",0}, {"",0},
#line 388 "rgbtab.gperf"
      {"sandybrown", 0xf4a460},
      {"",0}, {"",0},
#line 939 "rgbtab.gperf"
      {"darkblue", 0x00008b},
#line 218 "rgbtab.gperf"
      {"xterm189", 0xd7d7ff},
      {"",0}, {"",0},
#line 116 "rgbtab.gperf"
      {"xterm87", 0x5fffff},
      {"",0}, {"",0},
#line 751 "rgbtab.gperf"
      {"gray8", 0x141414},
#line 771 "rgbtab.gperf"
      {"gray18", 0x2e2e2e},
#line 82 "rgbtab.gperf"
      {"xterm53", 0x5f005f},
      {"",0}, {"",0},
#line 584 "rgbtab.gperf"
      {"lightgoldenrod3", 0xcdbe70},
#line 791 "rgbtab.gperf"
      {"gray28", 0x474747},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 435 "rgbtab.gperf"
      {"bisque2", 0xeed5b7},
#line 127 "rgbtab.gperf"
      {"xterm98", 0x875fd7},
      {"",0}, {"",0},
#line 722 "rgbtab.gperf"
      {"darkorchid4", 0x68228b},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 891 "rgbtab.gperf"
      {"gray78", 0xc7c7c7},
#line 645 "rgbtab.gperf"
      {"salmon4", 0x8b4c39},
      {"",0}, {"",0},
#line 331 "rgbtab.gperf"
      {"mediumslateblue", 0x7b68ee},
      {"",0},
#line 665 "rgbtab.gperf"
      {"tomato4", 0x8b3626},
      {"",0}, {"",0}, {"",0},
#line 873 "rgbtab.gperf"
      {"gray69", 0xb0b0b0},
      {"",0},
#line 117 "rgbtab.gperf"
      {"xterm88", 0x870000},
#line 217 "rgbtab.gperf"
      {"xterm188", 0xd7d7d7},
      {"",0},
#line 721 "rgbtab.gperf"
      {"darkorchid3", 0x9a32cd},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 29 "rgbtab.gperf"
      {"xterm0", 0x000000},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 702 "rgbtab.gperf"
      {"violetred4", 0x8b2252},
#line 853 "rgbtab.gperf"
      {"gray59", 0x969696},
      {"",0}, {"",0},
#line 320 "rgbtab.gperf"
      {"lightslategrey", 0x778899},
#line 736 "rgbtab.gperf"
      {"grey0", 0x000000},
#line 756 "rgbtab.gperf"
      {"grey10", 0x1a1a1a},
#line 434 "rgbtab.gperf"
      {"bisque1", 0xffe4c4},
      {"",0}, {"",0}, {"",0},
#line 776 "rgbtab.gperf"
      {"grey20", 0x333333},
#line 644 "rgbtab.gperf"
      {"salmon3", 0xcd7054},
#line 149 "rgbtab.gperf"
      {"xterm120", 0x87ff87},
      {"",0}, {"",0}, {"",0},
#line 664 "rgbtab.gperf"
      {"tomato3", 0xcd4f39},
#line 249 "rgbtab.gperf"
      {"xterm220", 0xffd700},
      {"",0},
#line 701 "rgbtab.gperf"
      {"violetred3", 0xcd3278},
#line 871 "rgbtab.gperf"
      {"gray68", 0xadadad},
      {"",0}, {"",0}, {"",0},
#line 513 "rgbtab.gperf"
      {"lightsteelblue4", 0x6e7b8b},
#line 876 "rgbtab.gperf"
      {"grey70", 0xb3b3b3},
#line 593 "rgbtab.gperf"
      {"yellow4", 0x8b8b00},
      {"",0}, {"",0},
#line 618 "rgbtab.gperf"
      {"burlywood1", 0xffd39b},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 619 "rgbtab.gperf"
      {"burlywood2", 0xeec591},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 851 "rgbtab.gperf"
      {"gray58", 0x949494},
      {"",0}, {"",0},
#line 385 "rgbtab.gperf"
      {"burlywood", 0xdeb887},
#line 512 "rgbtab.gperf"
      {"lightsteelblue3", 0xa2b5cd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 657 "rgbtab.gperf"
      {"darkorange4", 0x8b4500},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 139 "rgbtab.gperf"
      {"xterm110", 0x87afd7},
      {"",0}, {"",0}, {"",0},
#line 592 "rgbtab.gperf"
      {"yellow3", 0xcdcd00},
#line 239 "rgbtab.gperf"
      {"xterm210", 0xff8787},
#line 406 "rgbtab.gperf"
      {"lightpink", 0xffb6c1},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 856 "rgbtab.gperf"
      {"grey60", 0x999999},
#line 656 "rgbtab.gperf"
      {"darkorange3", 0xcd6600},
      {"",0},
#line 332 "rgbtab.gperf"
      {"lightslateblue", 0x8470ff},
#line 606 "rgbtab.gperf"
      {"rosybrown1", 0xffc1c1},
      {"",0}, {"",0},
#line 129 "rgbtab.gperf"
      {"xterm100", 0x878700},
      {"",0},
#line 607 "rgbtab.gperf"
      {"rosybrown2", 0xeeb4b4},
      {"",0}, {"",0},
#line 229 "rgbtab.gperf"
      {"xterm200", 0xff00d7},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 836 "rgbtab.gperf"
      {"grey50", 0x7f7f7f},
#line 936 "rgbtab.gperf"
      {"grey100", 0xffffff},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 123 "rgbtab.gperf"
      {"xterm94", 0x875f00},
#line 327 "rgbtab.gperf"
      {"navyblue", 0x000080},
      {"",0}, {"",0}, {"",0},
#line 328 "rgbtab.gperf"
      {"cornflowerblue", 0x6495ed},
      {"",0}, {"",0}, {"",0},
#line 383 "rgbtab.gperf"
      {"sienna", 0xa0522d},
#line 698 "rgbtab.gperf"
      {"maroon4", 0x8b1c62},
      {"",0}, {"",0}, {"",0},
#line 834 "rgbtab.gperf"
      {"grey49", 0x7d7d7d},
      {"",0}, {"",0},
#line 675 "rgbtab.gperf"
      {"deeppink1", 0xff1493},
      {"",0}, {"",0},
#line 113 "rgbtab.gperf"
      {"xterm84", 0x5fff87},
      {"",0},
#line 676 "rgbtab.gperf"
      {"deeppink2", 0xee1289},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 506 "rgbtab.gperf"
      {"slategray1", 0xc6e2ff},
      {"",0}, {"",0},
#line 74 "rgbtab.gperf"
      {"xterm45", 0x00d7ff},
#line 380 "rgbtab.gperf"
      {"rosybrown", 0xbc8f8f},
#line 507 "rgbtab.gperf"
      {"slategray2", 0xb9d3ee},
#line 814 "rgbtab.gperf"
      {"grey39", 0x636363},
      {"",0}, {"",0},
#line 289 "rgbtab.gperf"
      {"gainsboro", 0xdcdcdc},
      {"",0}, {"",0},
#line 122 "rgbtab.gperf"
      {"xterm93", 0x8700ff},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 697 "rgbtab.gperf"
      {"maroon3", 0xcd2990},
#line 64 "rgbtab.gperf"
      {"xterm35", 0x00af5f},
      {"",0}, {"",0},
#line 832 "rgbtab.gperf"
      {"grey48", 0x7a7a7a},
      {"",0}, {"",0}, {"",0},
#line 577 "rgbtab.gperf"
      {"darkolivegreen4", 0x6e8b3d},
      {"",0},
#line 112 "rgbtab.gperf"
      {"xterm83", 0x5fff5f},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 534 "rgbtab.gperf"
      {"cyan1", 0x00ffff},
      {"",0}, {"",0},
#line 690 "rgbtab.gperf"
      {"lightpink4", 0x8b5f65},
#line 586 "rgbtab.gperf"
      {"lightyellow1", 0xffffe0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 812 "rgbtab.gperf"
      {"grey38", 0x616161},
#line 535 "rgbtab.gperf"
      {"cyan2", 0x00eeee},
      {"",0}, {"",0},
#line 576 "rgbtab.gperf"
      {"darkolivegreen3", 0xa2cd5a},
#line 587 "rgbtab.gperf"
      {"lightyellow2", 0xeeeed1},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 403 "rgbtab.gperf"
      {"hotpink", 0xff69b4},
#line 208 "rgbtab.gperf"
      {"xterm179", 0xd7af5f},
      {"",0},
#line 689 "rgbtab.gperf"
      {"lightpink3", 0xcd8c95},
#line 933 "rgbtab.gperf"
      {"gray99", 0xfcfcfc},
      {"",0}, {"",0}, {"",0},
#line 533 "rgbtab.gperf"
      {"turquoise4", 0x00868b},
      {"",0},
#line 351 "rgbtab.gperf"
      {"mediumaquamarine", 0x66cdaa},
      {"",0},
#line 317 "rgbtab.gperf"
      {"slategray", 0x708090},
      {"",0}, {"",0}, {"",0},
#line 458 "rgbtab.gperf"
      {"honeydew1", 0xf0fff0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 459 "rgbtab.gperf"
      {"honeydew2", 0xe0eee0},
      {"",0}, {"",0},
#line 913 "rgbtab.gperf"
      {"gray89", 0xe3e3e3},
#line 78 "rgbtab.gperf"
      {"xterm49", 0x00ffaf},
      {"",0}, {"",0},
#line 532 "rgbtab.gperf"
      {"turquoise3", 0x00c5cd},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 365 "rgbtab.gperf"
      {"greenyellow", 0xadff2f},
#line 348 "rgbtab.gperf"
      {"cyan", 0x00ffff},
      {"",0}, {"",0}, {"",0},
#line 374 "rgbtab.gperf"
      {"lightyellow", 0xffffe0},
      {"",0}, {"",0},
#line 207 "rgbtab.gperf"
      {"xterm178", 0xd7af00},
      {"",0}, {"",0},
#line 931 "rgbtab.gperf"
      {"gray98", 0xfafafa},
#line 68 "rgbtab.gperf"
      {"xterm39", 0x00afff},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 605 "rgbtab.gperf"
      {"darkgoldenrod4", 0x8b6508},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 941 "rgbtab.gperf"
      {"darkmagenta", 0x8b008b},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 911 "rgbtab.gperf"
      {"gray88", 0xe0e0e0},
      {"",0}, {"",0}, {"",0},
#line 189 "rgbtab.gperf"
      {"xterm160", 0xd70000},
      {"",0},
#line 304 "rgbtab.gperf"
      {"honeydew", 0xf0fff0},
      {"",0},
#line 604 "rgbtab.gperf"
      {"darkgoldenrod3", 0xcd950c},
      {"",0},
#line 817 "rgbtab.gperf"
      {"gray41", 0x696969},
      {"",0},
#line 366 "rgbtab.gperf"
      {"limegreen", 0x32cd32},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 819 "rgbtab.gperf"
      {"gray42", 0x6b6b6b},
      {"",0},
#line 169 "rgbtab.gperf"
      {"xterm140", 0xaf87d7},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 269 "rgbtab.gperf"
      {"xterm240", 0x585858},
      {"",0}, {"",0},
#line 797 "rgbtab.gperf"
      {"gray31", 0x4f4f4f},
#line 518 "rgbtab.gperf"
      {"lightcyan1", 0xe0ffff},
      {"",0},
#line 340 "rgbtab.gperf"
      {"steelblue", 0x4682b4},
#line 490 "rgbtab.gperf"
      {"steelblue1", 0x63b8ff},
#line 916 "rgbtab.gperf"
      {"grey90", 0xe5e5e5},
#line 519 "rgbtab.gperf"
      {"lightcyan2", 0xd1eeee},
      {"",0}, {"",0},
#line 491 "rgbtab.gperf"
      {"steelblue2", 0x5cacee},
#line 799 "rgbtab.gperf"
      {"gray32", 0x525252},
      {"",0},
#line 159 "rgbtab.gperf"
      {"xterm130", 0xaf5f00},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 259 "rgbtab.gperf"
      {"xterm230", 0xffffd7},
      {"",0}, {"",0},
#line 829 "rgbtab.gperf"
      {"gray47", 0x787878},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 896 "rgbtab.gperf"
      {"grey80", 0xcccccc},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 69 "rgbtab.gperf"
      {"xterm40", 0x00d700},
      {"",0}, {"",0}, {"",0},
#line 421 "rgbtab.gperf"
      {"thistle", 0xd8bfd8},
#line 731 "rgbtab.gperf"
      {"thistle1", 0xffe1ff},
#line 361 "rgbtab.gperf"
      {"lawngreen", 0x7cfc00},
      {"",0}, {"",0},
#line 809 "rgbtab.gperf"
      {"gray37", 0x5e5e5e},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 732 "rgbtab.gperf"
      {"thistle2", 0xeed2ee},
      {"",0},
#line 342 "rgbtab.gperf"
      {"lightblue", 0xadd8e6},
#line 514 "rgbtab.gperf"
      {"lightblue1", 0xbfefff},
      {"",0},
#line 59 "rgbtab.gperf"
      {"xterm30", 0x008787},
      {"",0}, {"",0},
#line 515 "rgbtab.gperf"
      {"lightblue2", 0xb2dfee},
#line 349 "rgbtab.gperf"
      {"lightcyan", 0xe0ffff},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 299 "rgbtab.gperf"
      {"moccasin", 0xffe4b5},
#line 578 "rgbtab.gperf"
      {"khaki1", 0xfff68f},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 579 "rgbtab.gperf"
      {"khaki2", 0xeee685},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 438 "rgbtab.gperf"
      {"peachpuff1", 0xffdab9},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 439 "rgbtab.gperf"
      {"peachpuff2", 0xeecbad},
      {"",0},
#line 219 "rgbtab.gperf"
      {"xterm190", 0xd7ff00},
      {"",0},
#line 669 "rgbtab.gperf"
      {"orangered4", 0x8b2500},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 827 "rgbtab.gperf"
      {"gray46", 0x757575},
      {"",0}, {"",0}, {"",0},
#line 668 "rgbtab.gperf"
      {"orangered3", 0xcd3700},
#line 338 "rgbtab.gperf"
      {"skyblue", 0x87ceeb},
#line 498 "rgbtab.gperf"
      {"skyblue1", 0x87ceff},
      {"",0}, {"",0}, {"",0},
#line 297 "rgbtab.gperf"
      {"peachpuff", 0xffdab9},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 499 "rgbtab.gperf"
      {"skyblue2", 0x7ec0ee},
      {"",0}, {"",0}, {"",0},
#line 807 "rgbtab.gperf"
      {"gray36", 0x5c5c5c},
      {"",0}, {"",0}, {"",0},
#line 298 "rgbtab.gperf"
      {"navajowhite", 0xffdead},
#line 442 "rgbtab.gperf"
      {"navajowhite1", 0xffdead},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 353 "rgbtab.gperf"
      {"darkgreen", 0x006400},
      {"",0}, {"",0},
#line 443 "rgbtab.gperf"
      {"navajowhite2", 0xeecfa1},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 825 "rgbtab.gperf"
      {"gray45", 0x737373},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 726 "rgbtab.gperf"
      {"purple4", 0x551a8b},
      {"",0}, {"",0}, {"",0},
#line 33 "rgbtab.gperf"
      {"xterm4", 0x000080},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 805 "rgbtab.gperf"
      {"gray35", 0x595959},
      {"",0}, {"",0}, {"",0},
#line 744 "rgbtab.gperf"
      {"grey4", 0x0a0a0a},
#line 764 "rgbtab.gperf"
      {"grey14", 0x242424},
      {"",0}, {"",0},
#line 674 "rgbtab.gperf"
      {"debianred", 0xd70751},
      {"",0},
#line 784 "rgbtab.gperf"
      {"grey24", 0x3d3d3d},
      {"",0},
#line 153 "rgbtab.gperf"
      {"xterm124", 0xaf0000},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 253 "rgbtab.gperf"
      {"xterm224", 0xffd7d7},
      {"",0},
#line 597 "rgbtab.gperf"
      {"gold4", 0x8b7500},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 884 "rgbtab.gperf"
      {"grey74", 0xbdbdbd},
#line 725 "rgbtab.gperf"
      {"purple3", 0x7d26cd},
      {"",0}, {"",0}, {"",0},
#line 32 "rgbtab.gperf"
      {"xterm3", 0x808000},
      {"",0}, {"",0},
#line 179 "rgbtab.gperf"
      {"xterm150", 0xafd787},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 279 "rgbtab.gperf"
      {"xterm250", 0xbcbcbc},
      {"",0},
#line 337 "rgbtab.gperf"
      {"deepskyblue", 0x00bfff},
#line 494 "rgbtab.gperf"
      {"deepskyblue1", 0x00bfff},
      {"",0}, {"",0},
#line 742 "rgbtab.gperf"
      {"grey3", 0x080808},
#line 762 "rgbtab.gperf"
      {"grey13", 0x212121},
      {"",0}, {"",0},
#line 334 "rgbtab.gperf"
      {"royalblue", 0x4169e1},
#line 478 "rgbtab.gperf"
      {"royalblue1", 0x4876ff},
#line 782 "rgbtab.gperf"
      {"grey23", 0x3b3b3b},
#line 495 "rgbtab.gperf"
      {"deepskyblue2", 0x00b2ee},
#line 152 "rgbtab.gperf"
      {"xterm123", 0x87ffff},
      {"",0},
#line 479 "rgbtab.gperf"
      {"royalblue2", 0x436eee},
      {"",0},
#line 937 "rgbtab.gperf"
      {"darkgrey", 0xa9a9a9},
#line 252 "rgbtab.gperf"
      {"xterm223", 0xffd7af},
      {"",0},
#line 596 "rgbtab.gperf"
      {"gold3", 0xcdad00},
#line 565 "rgbtab.gperf"
      {"green4", 0x008b00},
      {"",0},
#line 143 "rgbtab.gperf"
      {"xterm114", 0x87d787},
      {"",0},
#line 418 "rgbtab.gperf"
      {"blueviolet", 0x8a2be2},
#line 882 "rgbtab.gperf"
      {"grey73", 0xbababa},
      {"",0},
#line 243 "rgbtab.gperf"
      {"xterm214", 0xffaf00},
      {"",0},
#line 425 "rgbtab.gperf"
      {"snow4", 0x8b8989},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 864 "rgbtab.gperf"
      {"grey64", 0xa3a3a3},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 133 "rgbtab.gperf"
      {"xterm104", 0x8787d7},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 233 "rgbtab.gperf"
      {"xterm204", 0xff5f87},
      {"",0}, {"",0},
#line 625 "rgbtab.gperf"
      {"wheat4", 0x8b7e66},
      {"",0}, {"",0}, {"",0},
#line 371 "rgbtab.gperf"
      {"khaki", 0xf0e68c},
#line 844 "rgbtab.gperf"
      {"grey54", 0x8a8a8a},
#line 76 "rgbtab.gperf"
      {"xterm47", 0x00ff5f},
      {"",0}, {"",0}, {"",0},
#line 564 "rgbtab.gperf"
      {"green3", 0x00cd00},
      {"",0},
#line 142 "rgbtab.gperf"
      {"xterm113", 0x87d75f},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 242 "rgbtab.gperf"
      {"xterm213", 0xff87ff},
      {"",0},
#line 424 "rgbtab.gperf"
      {"snow3", 0xcdc9c9},
      {"",0},
#line 309 "rgbtab.gperf"
      {"lavenderblush", 0xfff0f5},
#line 462 "rgbtab.gperf"
      {"lavenderblush1", 0xfff0f5},
#line 694 "rgbtab.gperf"
      {"palevioletred4", 0x8b475d},
      {"",0},
#line 862 "rgbtab.gperf"
      {"grey63", 0xa1a1a1},
#line 66 "rgbtab.gperf"
      {"xterm37", 0x00afaf},
#line 463 "rgbtab.gperf"
      {"lavenderblush2", 0xeee0e5},
      {"",0},
#line 613 "rgbtab.gperf"
      {"indianred4", 0x8b3a3a},
      {"",0}, {"",0},
#line 132 "rgbtab.gperf"
      {"xterm103", 0x8787af},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 232 "rgbtab.gperf"
      {"xterm203", 0xff5f5f},
      {"",0},
#line 573 "rgbtab.gperf"
      {"olivedrab4", 0x698b22},
#line 624 "rgbtab.gperf"
      {"wheat3", 0xcdba96},
      {"",0},
#line 77 "rgbtab.gperf"
      {"xterm48", 0x00ff87},
#line 693 "rgbtab.gperf"
      {"palevioletred3", 0xcd6889},
      {"",0},
#line 842 "rgbtab.gperf"
      {"grey53", 0x878787},
#line 617 "rgbtab.gperf"
      {"sienna4", 0x8b4726},
#line 649 "rgbtab.gperf"
      {"lightsalmon4", 0x8b5742},
      {"",0},
#line 612 "rgbtab.gperf"
      {"indianred3", 0xcd5555},
      {"",0}, {"",0},
#line 633 "rgbtab.gperf"
      {"chocolate4", 0x8b4513},
#line 706 "rgbtab.gperf"
      {"magenta4", 0x8b008b},
#line 352 "rgbtab.gperf"
      {"aquamarine", 0x7fffd4},
#line 542 "rgbtab.gperf"
      {"aquamarine1", 0x7fffd4},
#line 653 "rgbtab.gperf"
      {"orange4", 0x8b5a00},
      {"",0},
#line 429 "rgbtab.gperf"
      {"seashell4", 0x8b8682},
#line 572 "rgbtab.gperf"
      {"olivedrab3", 0x9acd32},
#line 543 "rgbtab.gperf"
      {"aquamarine2", 0x76eec6},
      {"",0},
#line 67 "rgbtab.gperf"
      {"xterm38", 0x00afd7},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 541 "rgbtab.gperf"
      {"darkslategray4", 0x528b8b},
      {"",0}, {"",0},
#line 632 "rgbtab.gperf"
      {"chocolate3", 0xcd661d},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 428 "rgbtab.gperf"
      {"seashell3", 0xcdc5bf},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 616 "rgbtab.gperf"
      {"sienna3", 0xcd6839},
#line 648 "rgbtab.gperf"
      {"lightsalmon3", 0xcd8162},
      {"",0},
#line 540 "rgbtab.gperf"
      {"darkslategray3", 0x79cdcd},
      {"",0}, {"",0}, {"",0},
#line 705 "rgbtab.gperf"
      {"magenta3", 0xcd00cd},
      {"",0}, {"",0},
#line 652 "rgbtab.gperf"
      {"orange3", 0xcd8500},
      {"",0},
#line 554 "rgbtab.gperf"
      {"palegreen1", 0x9aff9a},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 555 "rgbtab.gperf"
      {"palegreen2", 0x90ee90},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 369 "rgbtab.gperf"
      {"olivedrab", 0x6b8e23},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 370 "rgbtab.gperf"
      {"darkkhaki", 0xbdb76b},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 359 "rgbtab.gperf"
      {"palegreen", 0x98fb98},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 319 "rgbtab.gperf"
      {"lightslategray", 0x778899},
#line 735 "rgbtab.gperf"
      {"gray0", 0x000000},
#line 755 "rgbtab.gperf"
      {"gray10", 0x1a1a1a},
      {"",0}, {"",0}, {"",0},
#line 193 "rgbtab.gperf"
      {"xterm164", 0xd700d7},
#line 775 "rgbtab.gperf"
      {"gray20", 0x333333},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 308 "rgbtab.gperf"
      {"lavender", 0xe6e6fa},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 875 "rgbtab.gperf"
      {"gray70", 0xb3b3b3},
#line 73 "rgbtab.gperf"
      {"xterm44", 0x00d7d7},
#line 173 "rgbtab.gperf"
      {"xterm144", 0xafaf87},
      {"",0},
#line 294 "rgbtab.gperf"
      {"papayawhip", 0xffefd5},
      {"",0}, {"",0},
#line 273 "rgbtab.gperf"
      {"xterm244", 0x808080},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 209 "rgbtab.gperf"
      {"xterm180", 0xd7af87},
      {"",0},
#line 924 "rgbtab.gperf"
      {"grey94", 0xf0f0f0},
      {"",0}, {"",0},
#line 525 "rgbtab.gperf"
      {"paleturquoise4", 0x668b8b},
      {"",0}, {"",0},
#line 63 "rgbtab.gperf"
      {"xterm34", 0x00af00},
#line 163 "rgbtab.gperf"
      {"xterm134", 0xaf5fd7},
      {"",0},
#line 192 "rgbtab.gperf"
      {"xterm163", 0xd700af},
      {"",0}, {"",0},
#line 263 "rgbtab.gperf"
      {"xterm234", 0x1c1c1c},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 549 "rgbtab.gperf"
      {"darkseagreen4", 0x698b69},
      {"",0},
#line 904 "rgbtab.gperf"
      {"grey84", 0xd6d6d6},
      {"",0}, {"",0},
#line 524 "rgbtab.gperf"
      {"paleturquoise3", 0x96cdcd},
      {"",0}, {"",0},
#line 72 "rgbtab.gperf"
      {"xterm43", 0x00d7af},
#line 172 "rgbtab.gperf"
      {"xterm143", 0xafaf5f},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 272 "rgbtab.gperf"
      {"xterm243", 0x767676},
      {"",0}, {"",0},
#line 855 "rgbtab.gperf"
      {"gray60", 0x999999},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 922 "rgbtab.gperf"
      {"grey93", 0xededed},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 457 "rgbtab.gperf"
      {"ivory4", 0x8b8b83},
#line 62 "rgbtab.gperf"
      {"xterm33", 0x0087ff},
#line 162 "rgbtab.gperf"
      {"xterm133", 0xaf5faf},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 262 "rgbtab.gperf"
      {"xterm233", 0x121212},
      {"",0}, {"",0},
#line 835 "rgbtab.gperf"
      {"gray50", 0x7f7f7f},
#line 935 "rgbtab.gperf"
      {"gray100", 0xffffff},
      {"",0},
#line 548 "rgbtab.gperf"
      {"darkseagreen3", 0x9bcd9b},
      {"",0},
#line 902 "rgbtab.gperf"
      {"grey83", 0xd4d4d4},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 833 "rgbtab.gperf"
      {"gray49", 0x7d7d7d},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 456 "rgbtab.gperf"
      {"ivory3", 0xcdcdc1},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 813 "rgbtab.gperf"
      {"gray39", 0x636363},
      {"",0},
#line 223 "rgbtab.gperf"
      {"xterm194", 0xd7ffd7},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 831 "rgbtab.gperf"
      {"gray48", 0x7a7a7a},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 811 "rgbtab.gperf"
      {"gray38", 0x616161},
      {"",0},
#line 222 "rgbtab.gperf"
      {"xterm193", 0xd7ffaf},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 634 "rgbtab.gperf"
      {"firebrick1", 0xff3030},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 635 "rgbtab.gperf"
      {"firebrick2", 0xee2c2c},
      {"",0}, {"",0}, {"",0},
#line 561 "rgbtab.gperf"
      {"springgreen4", 0x008b45},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 816 "rgbtab.gperf"
      {"grey40", 0x666666},
#line 358 "rgbtab.gperf"
      {"lightseagreen", 0x20b2aa},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 404 "rgbtab.gperf"
      {"deeppink", 0xff1493},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 796 "rgbtab.gperf"
      {"grey30", 0x4d4d4d},
#line 560 "rgbtab.gperf"
      {"springgreen3", 0x00cd66},
      {"",0},
#line 710 "rgbtab.gperf"
      {"orchid4", 0x8b4789},
#line 621 "rgbtab.gperf"
      {"burlywood4", 0x8b7355},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 686 "rgbtab.gperf"
      {"pink4", 0x8b636c},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 550 "rgbtab.gperf"
      {"seagreen1", 0x54ff9f},
#line 183 "rgbtab.gperf"
      {"xterm154", 0xafff00},
      {"",0}, {"",0}, {"",0},
#line 551 "rgbtab.gperf"
      {"seagreen2", 0x4eee94},
#line 283 "rgbtab.gperf"
      {"xterm254", 0xe4e4e4},
#line 620 "rgbtab.gperf"
      {"burlywood3", 0xcdaa7d},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 330 "rgbtab.gperf"
      {"slateblue", 0x6a5acd},
#line 474 "rgbtab.gperf"
      {"slateblue1", 0x836fff},
      {"",0}, {"",0}, {"",0},
#line 709 "rgbtab.gperf"
      {"orchid3", 0xcd69c9},
#line 475 "rgbtab.gperf"
      {"slateblue2", 0x7a67ee},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 685 "rgbtab.gperf"
      {"pink3", 0xcd919e},
      {"",0}, {"",0},
#line 199 "rgbtab.gperf"
      {"xterm170", 0xd75fd7},
      {"",0},
#line 609 "rgbtab.gperf"
      {"rosybrown4", 0x8b6969},
#line 915 "rgbtab.gperf"
      {"gray90", 0xe5e5e5},
      {"",0}, {"",0},
#line 182 "rgbtab.gperf"
      {"xterm153", 0xafd7ff},
      {"",0},
#line 450 "rgbtab.gperf"
      {"cornsilk1", 0xfff8dc},
      {"",0}, {"",0},
#line 282 "rgbtab.gperf"
      {"xterm253", 0xdadada},
      {"",0},
#line 451 "rgbtab.gperf"
      {"cornsilk2", 0xeee8cd},
      {"",0},
#line 718 "rgbtab.gperf"
      {"mediumorchid4", 0x7a378b},
      {"",0}, {"",0}, {"",0},
#line 356 "rgbtab.gperf"
      {"seagreen", 0x2e8b57},
      {"",0}, {"",0},
#line 608 "rgbtab.gperf"
      {"rosybrown3", 0xcd9b9b},
#line 895 "rgbtab.gperf"
      {"gray80", 0xcccccc},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 678 "rgbtab.gperf"
      {"deeppink4", 0x8b0a50},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 509 "rgbtab.gperf"
      {"slategray4", 0x6c7b8b},
      {"",0}, {"",0},
#line 717 "rgbtab.gperf"
      {"mediumorchid3", 0xb452cd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 677 "rgbtab.gperf"
      {"deeppink3", 0xcd1076},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 505 "rgbtab.gperf"
      {"lightskyblue4", 0x607b8b},
      {"",0},
#line 508 "rgbtab.gperf"
      {"slategray3", 0x9fb6cd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 504 "rgbtab.gperf"
      {"lightskyblue3", 0x8db6cd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 461 "rgbtab.gperf"
      {"honeydew4", 0x838b83},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 437 "rgbtab.gperf"
      {"bisque4", 0x8b7d6b},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 460 "rgbtab.gperf"
      {"honeydew3", 0xc1cdc1},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 350 "rgbtab.gperf"
      {"cadetblue", 0x5f9ea0},
#line 526 "rgbtab.gperf"
      {"cadetblue1", 0x98f5ff},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 527 "rgbtab.gperf"
      {"cadetblue2", 0x8ee5ee},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 307 "rgbtab.gperf"
      {"aliceblue", 0xf0f8ff},
      {"",0}, {"",0},
#line 436 "rgbtab.gperf"
      {"bisque3", 0xcdb79e},
      {"",0}, {"",0},
#line 743 "rgbtab.gperf"
      {"gray4", 0x0a0a0a},
#line 763 "rgbtab.gperf"
      {"gray14", 0x242424},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 783 "rgbtab.gperf"
      {"gray24", 0x3d3d3d},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 714 "rgbtab.gperf"
      {"plum4", 0x8b668b},
#line 883 "rgbtab.gperf"
      {"gray74", 0xbdbdbd},
#line 521 "rgbtab.gperf"
      {"lightcyan4", 0x7a8b8b},
      {"",0}, {"",0},
#line 493 "rgbtab.gperf"
      {"steelblue4", 0x36648b},
      {"",0}, {"",0},
#line 682 "rgbtab.gperf"
      {"hotpink4", 0x8b3a62},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 213 "rgbtab.gperf"
      {"xterm184", 0xd7d700},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 741 "rgbtab.gperf"
      {"gray3", 0x080808},
#line 761 "rgbtab.gperf"
      {"gray13", 0x212121},
#line 520 "rgbtab.gperf"
      {"lightcyan3", 0xb4cdcd},
      {"",0}, {"",0},
#line 492 "rgbtab.gperf"
      {"steelblue3", 0x4f94cd},
#line 781 "rgbtab.gperf"
      {"gray23", 0x3b3b3b},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 713 "rgbtab.gperf"
      {"plum3", 0xcd96cd},
#line 881 "rgbtab.gperf"
      {"gray73", 0xbababa},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 681 "rgbtab.gperf"
      {"hotpink3", 0xcd6090},
      {"",0},
#line 517 "rgbtab.gperf"
      {"lightblue4", 0x68838b},
#line 863 "rgbtab.gperf"
      {"gray64", 0xa3a3a3},
      {"",0}, {"",0},
#line 212 "rgbtab.gperf"
      {"xterm183", 0xd7afff},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 516 "rgbtab.gperf"
      {"lightblue3", 0x9ac0cd},
#line 843 "rgbtab.gperf"
      {"gray54", 0x8a8a8a},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 861 "rgbtab.gperf"
      {"gray63", 0xa1a1a1},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 441 "rgbtab.gperf"
      {"peachpuff4", 0x8b7765},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 841 "rgbtab.gperf"
      {"gray53", 0x878787},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 440 "rgbtab.gperf"
      {"peachpuff3", 0xcdaf95},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 730 "rgbtab.gperf"
      {"mediumpurple4", 0x5d478b},
      {"",0}, {"",0},
#line 824 "rgbtab.gperf"
      {"grey44", 0x707070},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 804 "rgbtab.gperf"
      {"grey34", 0x575757},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 729 "rgbtab.gperf"
      {"mediumpurple3", 0x8968cd},
      {"",0}, {"",0},
#line 822 "rgbtab.gperf"
      {"grey43", 0x6e6e6e},
      {"",0}, {"",0}, {"",0},
#line 481 "rgbtab.gperf"
      {"royalblue4", 0x27408b},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 802 "rgbtab.gperf"
      {"grey33", 0x545454},
      {"",0}, {"",0}, {"",0},
#line 480 "rgbtab.gperf"
      {"royalblue3", 0x3a5fcd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 293 "rgbtab.gperf"
      {"antiquewhite", 0xfaebd7},
#line 430 "rgbtab.gperf"
      {"antiquewhite1", 0xffefdb},
#line 203 "rgbtab.gperf"
      {"xterm174", 0xd78787},
      {"",0}, {"",0},
#line 923 "rgbtab.gperf"
      {"gray94", 0xf0f0f0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 473 "rgbtab.gperf"
      {"azure4", 0x838b8b},
#line 431 "rgbtab.gperf"
      {"antiquewhite2", 0xeedfcc},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 661 "rgbtab.gperf"
      {"coral4", 0x8b3e2f},
      {"",0}, {"",0},
#line 903 "rgbtab.gperf"
      {"gray84", 0xd6d6d6},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 465 "rgbtab.gperf"
      {"lavenderblush4", 0x8b8386},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 202 "rgbtab.gperf"
      {"xterm173", 0xd7875f},
      {"",0}, {"",0},
#line 921 "rgbtab.gperf"
      {"gray93", 0xededed},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 472 "rgbtab.gperf"
      {"azure3", 0xc1cdcd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 464 "rgbtab.gperf"
      {"lavenderblush3", 0xcdc1c5},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 660 "rgbtab.gperf"
      {"coral3", 0xcd5b45},
      {"",0}, {"",0},
#line 901 "rgbtab.gperf"
      {"gray83", 0xd4d4d4},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 545 "rgbtab.gperf"
      {"aquamarine4", 0x458b74},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 544 "rgbtab.gperf"
      {"aquamarine3", 0x66cdaa},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 557 "rgbtab.gperf"
      {"palegreen4", 0x548b54},
      {"",0},
#line 641 "rgbtab.gperf"
      {"brown4", 0x8b2323},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 556 "rgbtab.gperf"
      {"palegreen3", 0x7ccd7c},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 640 "rgbtab.gperf"
      {"brown3", 0xcd3333},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 449 "rgbtab.gperf"
      {"lemonchiffon4", 0x8b8970},
#line 815 "rgbtab.gperf"
      {"gray40", 0x666666},
      {"",0},
#line 291 "rgbtab.gperf"
      {"oldlace", 0xfdf5e6},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 795 "rgbtab.gperf"
      {"gray30", 0x4d4d4d},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 448 "rgbtab.gperf"
      {"lemonchiffon3", 0xcdc9a5},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 391 "rgbtab.gperf"
      {"firebrick", 0xb22222},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 485 "rgbtab.gperf"
      {"blue4", 0x00008b},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 305 "rgbtab.gperf"
      {"mintcream", 0xf5fffa},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 484 "rgbtab.gperf"
      {"blue3", 0x0000cd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0},
#line 637 "rgbtab.gperf"
      {"firebrick4", 0x8b1a1a},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 300 "rgbtab.gperf"
      {"cornsilk", 0xfff8dc},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 636 "rgbtab.gperf"
      {"firebrick3", 0xcd2626},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 553 "rgbtab.gperf"
      {"seagreen4", 0x2e8b57},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 552 "rgbtab.gperf"
      {"seagreen3", 0x43cd80},
      {"",0},
#line 477 "rgbtab.gperf"
      {"slateblue4", 0x473c8b},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 476 "rgbtab.gperf"
      {"slateblue3", 0x6959cd},
#line 453 "rgbtab.gperf"
      {"cornsilk4", 0x8b8878},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 452 "rgbtab.gperf"
      {"cornsilk3", 0xcdc8b1},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 312 "rgbtab.gperf"
      {"black", 0x000000},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 537 "rgbtab.gperf"
      {"cyan4", 0x008b8b},
      {"",0}, {"",0}, {"",0},
#line 589 "rgbtab.gperf"
      {"lightyellow4", 0x8b8b7a},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 536 "rgbtab.gperf"
      {"cyan3", 0x00cdcd},
      {"",0}, {"",0}, {"",0},
#line 588 "rgbtab.gperf"
      {"lightyellow3", 0xcdcdb4},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 529 "rgbtab.gperf"
      {"cadetblue4", 0x53868b},
      {"",0}, {"",0}, {"",0},
#line 363 "rgbtab.gperf"
      {"chartreuse", 0x7fff00},
#line 566 "rgbtab.gperf"
      {"chartreuse1", 0x7fff00},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 567 "rgbtab.gperf"
      {"chartreuse2", 0x76ee00},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
#line 528 "rgbtab.gperf"
      {"cadetblue3", 0x7ac5cd},
      {"",0}, {"",0}, {"",0},
#line 823 "rgbtab.gperf"
      {"gray44", 0x707070},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 803 "rgbtab.gperf"
      {"gray34", 0x575757},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 821 "rgbtab.gperf"
      {"gray43", 0x6e6e6e},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 734 "rgbtab.gperf"
      {"thistle4", 0x8b7b8b},
      {"",0}, {"",0}, {"",0},
#line 801 "rgbtab.gperf"
      {"gray33", 0x545454},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 581 "rgbtab.gperf"
      {"khaki4", 0x8b864e},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 733 "rgbtab.gperf"
      {"thistle3", 0xcdb5cd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 580 "rgbtab.gperf"
      {"khaki3", 0xcdc673},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 501 "rgbtab.gperf"
      {"skyblue4", 0x4a708b},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 445 "rgbtab.gperf"
      {"navajowhite4", 0x8b795e},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 500 "rgbtab.gperf"
      {"skyblue3", 0x6ca6cd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 444 "rgbtab.gperf"
      {"navajowhite3", 0xcdb38b},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 497 "rgbtab.gperf"
      {"deepskyblue4", 0x00688b},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 496 "rgbtab.gperf"
      {"deepskyblue3", 0x009acd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 295 "rgbtab.gperf"
      {"blanchedalmond", 0xffebcd},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 569 "rgbtab.gperf"
      {"chartreuse4", 0x458b00},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
#line 568 "rgbtab.gperf"
      {"chartreuse3", 0x66cd00},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0},
#line 433 "rgbtab.gperf"
      {"antiquewhite4", 0x8b8378},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0}, {"",0}, {"",0},
#line 432 "rgbtab.gperf"
      {"antiquewhite3", 0xcdc0b0}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = colorname_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
#line 944 "rgbtab.gperf"

