/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: /usr/local/bin/gperf -C --output-file jsontypes.c jsontypes.gperf  */
/* Computed positions: -k'3' */

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
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 10 "jsontypes.gperf"
struct json_type_map {
       const char *name;
       const char *pname;
};
/* maximum key range = 16, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
json_type_hash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 15, 20, 20,
      20, 20, 20, 20, 20, 20,  0, 20,  5, 20,
      20, 20, 20, 20,  0, 20,  0, 10, 20, 20,
       0, 20, 20, 20, 20, 20, 20, 20, 20, 20,
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
  return len + asso_values[(unsigned char)str[2]];
}

const struct json_type_map *
json_type_lookup (register const char *str, register size_t len)
{
  enum
    {
      TOTAL_KEYWORDS = 8,
      MIN_WORD_LENGTH = 4,
      MAX_WORD_LENGTH = 7,
      MIN_HASH_VALUE = 4,
      MAX_HASH_VALUE = 19
    };

  static const unsigned char lengthtable[] =
    {
       0,  0,  0,  0,  4,  5,  6,  7,  0,  4,  5,  0,  0,  0,
       4,  0,  0,  0,  0,  4
    };
  static const struct json_type_map wordlist[] =
    {
      {"",NULL}, {"",NULL}, {"",NULL}, {"",NULL},
#line 21 "jsontypes.gperf"
      {"text", "string"},
#line 15 "jsontypes.gperf"
      {"array", "array"},
#line 19 "jsontypes.gperf"
      {"object", "object"},
#line 17 "jsontypes.gperf"
      {"integer", "number"},
      {"",NULL},
#line 18 "jsontypes.gperf"
      {"null", "null"},
#line 16 "jsontypes.gperf"
      {"false", "boolean"},
      {"",NULL}, {"",NULL}, {"",NULL},
#line 22 "jsontypes.gperf"
      {"true", "boolean"},
      {"",NULL}, {"",NULL}, {"",NULL}, {"",NULL},
#line 20 "jsontypes.gperf"
      {"real", "number"}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = json_type_hash (str, len);

      if (key <= MAX_HASH_VALUE)
        if (len == lengthtable[key])
          {
            register const char *s = wordlist[key].name;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return &wordlist[key];
          }
    }
  return 0;
}
#line 23 "jsontypes.gperf"

