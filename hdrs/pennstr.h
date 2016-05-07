/** Functions for working with unicode strings. */

#ifndef PENNSTR_H
#define PENNSTR_H

#ifndef HAVE_UNISTR_H
#error "Requires libunistring to be installed."
#endif

#include <unitypes.h>

#define PS_SSO_LEN 32
struct pennstr {
  uint8_t *buf;
  uint8_t *bp;
  int capacity;
  uint8_t sso[PS_SSO_LEN];
};

typedef struct pennstr pennstr;

struct ps_iter_view {
  pennstr *ps;
  uint8_t *curr;
};
  
typedef struct ps_iter_view ps_iter_view;

/* Functions to create a new pennstr */
pennstr* ps_new();
pennstr* ps_dup(pennstr *);
pennstr* ps_from_utf8(const uint8_t *);
/** Create a new pennstr from an ascii string
 * \param s a 0-terminated string encoded in 7-bit ascii.
 * \return a newly allocated pennstr 
 */
static inline pennstr* ps_from_ascii(const char *s) {
  return ps_from_utf8((uint8_t *)s);
}
pennstr* ps_from_latin1(const char *);

/** The number of bytes used by the string stored in a pennstr */
static inline size_t ps_nbytes(pennstr *ps) {
  return ps->bp - ps->buf;
}

void ps_free(pennstr *);



#endif
