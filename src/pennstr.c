#include <stdlib.h>
#include <string.h>
#include <unistr.h>

#include "mymalloc.h"
#include "memcheck.h"
#include "pennstr.h"

/* TODO: Store strings as nul-terminated or not? ANSWER: Yes. */

/** Allocate a new empty pennstr */
pennstr *
ps_new() {
  pennstr *ps;
  ps = mush_malloc(sizeof *ps, "pennstr");
  ps->buf = ps->bp = ps->sso;
  ps->buf[0] = '\0';
  ps->capacity = PS_SSO_LEN;
  return ps;
}

/** Free a pennstr
 * \param ps the string to free.
 */
void
ps_free(pennstr *ps) {
  if (ps->buf != ps->sso)
    mush_free(ps->buf, "pennstr.buffer");
  mush_free(ps, "pennstr");
}

/** Copy a pennstr
 * \param orig the string to copy
 * \return a newly allocated copy of the string
 */
pennstr*
ps_dup(pennstr *orig) {
  pennstr *ps = ps_new();
  if (orig->buf != orig->sso) {
    size_t len = ps_nbytes(orig);
    ps->buf = mush_malloc(len + 1, "pennstr.buffer");
    memcpy(ps->buf, orig->buf, len + 1);
    ps->bp = ps->buf + len;
    ps->capacity = len;
  } else {
    memcpy(ps->sso, orig->sso, PS_SSO_LEN);
  }
  return ps;
}

/** Create a pennstr from a utf8 string.
 * \param s the original utf-8 encoded string.
 * \return a newly allocated pennstr
 */
pennstr*
ps_from_utf8(const uint8_t *s) {
  pennstr *ps = ps_new();
  size_t len = strlen((const char *)s);
  if (len + 1 < PS_SSO_LEN) {
    memcpy(ps->sso, s, len + 1);
    ps->bp = ps->buf + len;
  } else {
    ps->buf = mush_malloc(len + 1, "pennstr.buffer");
    memcpy(ps->buf, s, len + 1);
    ps->bp = ps->buf + len;
    ps->capacity = len;
  }
  return ps; 
}

