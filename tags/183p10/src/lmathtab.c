/* ANSI-C code produced by gperf version 3.0.3p1 */
/* Command-line: gperf --output-file lmathtab.c lmathtab.gperf  */
/* Computed positions: -k'1-2,5' */

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

#line 11 "lmathtab.gperf"

/** Declaration macro for math functions */
#define MATH_FUNC(func) static void func(char **ptr, int nptr, char *buff, char **bp)

/** Prototype macro for math functions */
#define MATH_PROTO(func) static void func (char **ptr, int nptr, char *buff, char **bp)

MATH_PROTO(math_add);
MATH_PROTO(math_sub);
MATH_PROTO(math_mul);
MATH_PROTO(math_div);
MATH_PROTO(math_floordiv);
MATH_PROTO(math_remainder);
MATH_PROTO(math_modulo);
MATH_PROTO(math_min);
MATH_PROTO(math_max);
MATH_PROTO(math_and);
MATH_PROTO(math_nand);
MATH_PROTO(math_or);
MATH_PROTO(math_nor);
MATH_PROTO(math_xor);
MATH_PROTO(math_band);
MATH_PROTO(math_bor);
MATH_PROTO(math_bxor);
MATH_PROTO(math_fdiv);
MATH_PROTO(math_mean);
MATH_PROTO(math_median);
MATH_PROTO(math_stddev);
MATH_PROTO(math_dist2d);
MATH_PROTO(math_dist3d);

/** A math function. */
#line 44 "lmathtab.gperf"
struct math {
  const char *name;     /**< Name of the function. */
  void (*func) (char **, int, char *, char **); /**< Pointer to function code. */
};
/* maximum key range = 46, duplicates = 0 */

#ifndef GPERF_DOWNCASE
#define GPERF_DOWNCASE 1
static unsigned char gperf_downcase[256] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
  30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
  45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
  60, 61, 62, 63, 64, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106,
  107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
  122, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104,
  105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
  120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
  135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
  150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
  165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
  180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
  195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
  210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
  225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
  240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
  255
};
#endif

#ifndef GPERF_CASE_MEMCMP
#define GPERF_CASE_MEMCMP 1
static int
gperf_case_memcmp(register const char *s1, register const char *s2,
                  register unsigned int n)
{
  for (; n > 0;) {
    unsigned char c1 = gperf_downcase[(unsigned char) *s1++];
    unsigned char c2 = gperf_downcase[(unsigned char) *s2++];
    if (c1 == c2) {
      n--;
      continue;
    }
    return (int) c1 - (int) c2;
  }
  return 0;
}
#endif

#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
  __attribute__ ((__gnu_inline__))
#endif
#endif
    static unsigned int
     math_hash(register const char *str, register unsigned int len)
{
  static unsigned char asso_values[] = {
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    5, 0, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 0, 25, 49, 25, 0,
    18, 49, 49, 10, 49, 49, 0, 10, 0, 5,
    49, 49, 0, 15, 10, 30, 49, 49, 3, 49,
    49, 49, 49, 49, 49, 49, 49, 0, 25, 49,
    25, 0, 18, 49, 49, 10, 49, 49, 0, 10,
    0, 5, 49, 49, 0, 15, 10, 30, 49, 49,
    3, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49
  };
  register int hval = len;

  switch (hval) {
  default:
    hval += asso_values[(unsigned char) str[4]];
   /*FALLTHROUGH*/ case 4:
  case 3:
  case 2:
    hval += asso_values[(unsigned char) str[1]];
   /*FALLTHROUGH*/ case 1:
    hval += asso_values[(unsigned char) str[0]];
    break;
  }
  return hval;
}

#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
  __attribute__ ((__gnu_inline__))
#endif
#endif
    struct math *math_hash_lookup(register const char *str,
                                  register unsigned int len)
{
  enum {
    TOTAL_KEYWORDS = 25,
    MIN_WORD_LENGTH = 2,
    MAX_WORD_LENGTH = 9,
    MIN_HASH_VALUE = 3,
    MAX_HASH_VALUE = 48
  };

  static unsigned char lengthtable[] = {
    0, 0, 0, 3, 4, 0, 0, 2, 3, 0, 0, 3, 0, 3,
    4, 0, 6, 0, 3, 9, 0, 6, 7, 3, 0, 0, 8, 0,
    3, 4, 0, 6, 4, 3, 0, 0, 0, 0, 3, 0, 0, 6,
    0, 3, 0, 0, 6, 4, 3
  };
  static struct math wordlist[] = {
    {"", NULL}, {"", NULL}, {"", NULL},
#line 60 "lmathtab.gperf"
    {"AND", math_and},
#line 61 "lmathtab.gperf"
    {"NAND", math_nand},
    {"", NULL}, {"", NULL},
#line 62 "lmathtab.gperf"
    {"OR", math_or},
#line 63 "lmathtab.gperf"
    {"NOR", math_nor},
    {"", NULL}, {"", NULL},
#line 64 "lmathtab.gperf"
    {"XOR", math_xor},
    {"", NULL},
#line 59 "lmathtab.gperf"
    {"MAX", math_max},
#line 69 "lmathtab.gperf"
    {"MEAN", math_mean},
    {"", NULL},
#line 70 "lmathtab.gperf"
    {"MEDIAN", math_median},
    {"", NULL},
#line 54 "lmathtab.gperf"
    {"MOD", math_modulo},
#line 57 "lmathtab.gperf"
    {"REMAINDER", math_remainder},
    {"", NULL},
#line 55 "lmathtab.gperf"
    {"MODULO", math_modulo},
#line 56 "lmathtab.gperf"
    {"MODULUS", math_modulo},
#line 58 "lmathtab.gperf"
    {"MIN", math_min},
    {"", NULL}, {"", NULL},
#line 53 "lmathtab.gperf"
    {"FLOORDIV", math_floordiv},
    {"", NULL},
#line 49 "lmathtab.gperf"
    {"ADD", math_add},
#line 65 "lmathtab.gperf"
    {"BAND", math_band},
    {"", NULL},
#line 71 "lmathtab.gperf"
    {"STDDEV", math_stddev},
#line 67 "lmathtab.gperf"
    {"BXOR", math_bxor},
#line 66 "lmathtab.gperf"
    {"BOR", math_bor},
    {"", NULL}, {"", NULL}, {"", NULL}, {"", NULL},
#line 52 "lmathtab.gperf"
    {"DIV", math_div},
    {"", NULL}, {"", NULL},
#line 73 "lmathtab.gperf"
    {"DIST3D", math_dist3d},
    {"", NULL},
#line 51 "lmathtab.gperf"
    {"MUL", math_mul},
    {"", NULL}, {"", NULL},
#line 72 "lmathtab.gperf"
    {"DIST2D", math_dist2d},
#line 68 "lmathtab.gperf"
    {"FDIV", math_fdiv},
#line 50 "lmathtab.gperf"
    {"SUB", math_sub}
  };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH) {
    register int key = math_hash(str, len);

    if (key <= MAX_HASH_VALUE && key >= 0)
      if (len == lengthtable[key]) {
        register const char *s = wordlist[key].name;

        if ((((unsigned char) *str ^ (unsigned char) *s) & ~32) == 0
            && !gperf_case_memcmp(str, s, len))
          return &wordlist[key];
      }
  }
  return 0;
}

#line 74 "lmathtab.gperf"


typedef struct math MATH;
