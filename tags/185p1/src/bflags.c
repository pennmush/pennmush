/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: gperf --output-file bflags.c bflags.gperf  */
/* Computed positions: -k'1' */

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

#line 15 "bflags.gperf"

/** The flag lock key (A^B) only allows a few values for A. This
 * struct and the the following table define the allowable ones. When
 * adding a new type here, a matching new bytecode instruction should
 * be added. */
#line 21 "bflags.gperf"
struct flag_lock_types {
  const char *name; /**< The value of A */
  bvm_opcode op;  /**< The associated opcode */
  int preserve; /**< If true, the parser preserves \\s in the match string */
};
/* maximum key range = 18, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
  static unsigned int
bflag_hash(register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] = {
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 0, 10, 20,
    10, 20, 0, 0, 20, 20, 20, 20, 5, 5,
    0, 20, 20, 20, 0, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20
  };
  return len + asso_values[(unsigned char) str[0]];
}

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
  __attribute__ ((__gnu_inline__))
#endif
#endif
    const struct flag_lock_types *is_allowed_bflag(register const char *str,
                                                   register unsigned int len)
{
  enum {
    TOTAL_KEYWORDS = 9,
    MIN_WORD_LENGTH = 2,
    MAX_WORD_LENGTH = 9,
    MIN_HASH_VALUE = 2,
    MAX_HASH_VALUE = 19
  };

  static const unsigned char lengthtable[] = {
    0, 0, 2, 0, 4, 5, 0, 7, 8, 4, 5, 0, 0, 0,
    4, 0, 0, 0, 0, 9
  };
  static const struct flag_lock_types wordlist[] = {
    {"", -1, 0}, {"", -1, 0},
#line 34 "bflags.gperf"
    {"IP", OP_TIP, 1},
    {"", -1, 0},
#line 30 "bflags.gperf"
    {"TYPE", OP_TTYPE, 0},
#line 29 "bflags.gperf"
    {"POWER", OP_TPOWER, 0},
    {"", -1, 0},
#line 32 "bflags.gperf"
    {"CHANNEL", OP_TCHANNEL, 0},
#line 35 "bflags.gperf"
    {"HOSTNAME", OP_THOSTNAME, 1},
#line 31 "bflags.gperf"
    {"NAME", OP_TNAME, 1},
#line 33 "bflags.gperf"
    {"OBJID", OP_TIS, 0},
    {"", -1, 0}, {"", -1, 0}, {"", -1, 0},
#line 28 "bflags.gperf"
    {"FLAG", OP_TFLAG, 0},
    {"", -1, 0}, {"", -1, 0}, {"", -1, 0}, {"", -1, 0},
#line 36 "bflags.gperf"
    {"DBREFLIST", OP_TDBREFLIST, 1}
  };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH) {
    register int key = bflag_hash(str, len);

    if (key <= MAX_HASH_VALUE && key >= 0)
      if (len == lengthtable[key]) {
        register const char *s = wordlist[key].name;

        if (*str == *s && !memcmp(str + 1, s + 1, len - 1))
          return &wordlist[key];
      }
  }
  return 0;
}

#line 37 "bflags.gperf"
