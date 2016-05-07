#include <stdlib.h>
#include <string.h>
#include <unistr.h>

#include "mymalloc.h"
#include "memcheck.h"
#include "pennstr.h"

/* TODO: Store strings as nul-terminated or not? */

/** Allocate a new empty pennstr */
pennstr *
ps_new() {
  pennstr *ps;
  ps = mush_malloc(sizeof *ps, "pennstr");
  ps->buf = ps->bp = ps->sso;
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
    ps->buf = u8_cpy_alloc(orig->buf, ps_nbytes(orig));
    add_check("pennstr.buffer");
    ps->bp = ps->buf;
    ps->capacity = ps_nbytes(orig);
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
  size_t len = u8_strlen(s);
  if (len < PS_SSO_LEN) {
    u8_cpy(ps->sso, s, len);
    ps->bp = ps->buf + len;
  } else {
    ps->buf = u8_cpy_alloc(s, len);
    add_check("pennstr.buffer");
    ps->bp = ps->buf + len;
    ps->capacity = len;
  }
  return ps; 
}

