/**
 * \file funcrypt.c
 *
 * \brief Functions for cryptographic stuff in softcode
 *
 *
 */
#include "copyrite.h"

#include "config.h"
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "conf.h"
#include "case.h"
#include "externs.h"
#include "version.h"
#include "extchat.h"
#include "htab.h"
#include "flags.h"
#include "dbdefs.h"
#include "parse.h"
#include "function.h"
#include "command.h"
#include "game.h"
#include "attrib.h"
#include "ansi.h"
#include "match.h"
#include "sort.h"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include "confmagic.h"


char *crunch_code(char *code);
char *crypt_code(char *code, char *text, int type);

static bool
encode_base64(const char *input, int len, char *buff, char **bp)
{
  BIO *bio, *b64, *bmem;
  char *membuf;

  b64 = BIO_new(BIO_f_base64());
  if (!b64) {
    safe_str(T("#-1 ALLOCATION ERROR"), buff, bp);
    return false;
  }

  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

  bmem = BIO_new(BIO_s_mem());
  if (!bmem) {
    safe_str(T("#-1 ALLOCATION ERROR"), buff, bp);
    BIO_free(b64);
    return false;
  }

  bio = BIO_push(b64, bmem);

  if (BIO_write(bio, input, len) < 0) {
    safe_str(T("#-1 CONVERSION ERROR"), buff, bp);
    BIO_free_all(bio);
    return false;
  }

  (void) BIO_flush(bio);

  len = BIO_get_mem_data(bmem, &membuf);

  safe_strl(membuf, len, buff, bp);

  BIO_free_all(bio);

  return true;
}

extern char valid_ansi_codes[UCHAR_MAX + 1];

static bool
decode_base64(char *encoded, int len, char *buff, char **bp)
{
  BIO *bio, *b64, *bmem;
  char *sbp;

  b64 = BIO_new(BIO_f_base64());
  if (!b64) {
    safe_str(T("#-1 ALLOCATION ERROR"), buff, bp);
    return false;
  }
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

  bmem = BIO_new_mem_buf(encoded, len);
  if (!bmem) {
    safe_str(T("#-1 ALLOCATION ERROR"), buff, bp);
    BIO_free(b64);
    return false;
  }
  /*  len = BIO_set_close(bmem, BIO_NOCLOSE); This makes valgrind report a memory leak. */

  bio = BIO_push(b64, bmem);

  sbp = *bp;
  while (true) {
    char decoded[BUFFER_LEN];
    int dlen;

    dlen = BIO_read(bio, decoded, BUFFER_LEN);
    if (dlen > 0) {
      int n;
      for (n = 0; n < dlen; n++) {
        if (decoded[n] == TAG_START) {
          int end;
          n += 1;
          for (end = n; end < dlen; end++) {
            if (decoded[end] == TAG_END)
              break;
          }
          if (end == dlen || decoded[n] != MARKUP_COLOR) {
            BIO_free_all(bio);
            *bp = sbp;
            safe_str(T("#-1 CONVERSION ERROR"), buff, bp);
            return false;
          }
          for (; n < end; n++) {
            if (!valid_ansi_codes[(unsigned char) decoded[n]]) {
              BIO_free_all(bio);
              *bp = sbp;
              safe_str(T("#-1 CONVERSION ERROR"), buff, bp);
              return false;
            }
          }
          n = end;
        } else if (!isprint((unsigned char) decoded[n]))
          decoded[n] = '?';
      }
      safe_strl(decoded, dlen, buff, bp);
    } else if (dlen == 0)
      break;
    else {
      BIO_free_all(bio);
      *bp = sbp;
      safe_str(T("#-1 CONVERSION ERROR"), buff, bp);
      return false;
    }
  }

  BIO_free_all(bio);

  return true;
}

/* Encode a string in base64 */
FUNCTION(fun_encode64)
{
  encode_base64(args[0], arglens[0], buff, bp);
}

/* Decode a string from base64 */
FUNCTION(fun_decode64)
{
  decode_base64(args[0], arglens[0], buff, bp);
}

/* Copy over only alphanumeric chars */
char *
crunch_code(char *code)
{
  char *in;
  char *out;
  static char output[BUFFER_LEN];

  out = output;
  in = code;
  WALK_ANSI_STRING(in) {
    if ((*in >= 32) && (*in <= 126)) {
      *out++ = *in;
    }
    in++;
  }
  *out = '\0';
  return output;
}

char *
crypt_code(char *code, char *text, int type)
{
  static char textbuff[BUFFER_LEN];
  char codebuff[BUFFER_LEN];
  int start = 32;
  int end = 126;
  int mod = end - start + 1;
  char *p, *q, *r;

  if (!(text && *text))
    return (char *) "";
  if (!code || !*code)
    return text;
  mush_strncpy(codebuff, crunch_code(code), BUFFER_LEN);
  if (!*codebuff)
    return text;
  textbuff[0] = '\0';

  p = text;
  q = codebuff;
  r = textbuff;
  /* Encryption: Simply go through each character of the text, get its ascii
   * value, subtract start, add the ascii value (less start) of the
   * code, mod the result, add start. Continue  */
  while (*p) {
    if ((*p < start) || (*p > end)) {
      p++;
      continue;
    }
    if (type)
      *r++ = (((*p++ - start) + (*q++ - start)) % mod) + start;
    else
      *r++ = (((*p++ - *q++) + 2 * mod) % mod) + start;
    if (!*q)
      q = codebuff;
  }
  *r = '\0';
  return textbuff;
}

FUNCTION(fun_encrypt)
{
  char tbuff[BUFFER_LEN], *tp;
  char *pass;
  size_t len;
  ansi_string *as = parse_ansi_string(args[0]);

  pass = remove_markup(args[1], &len);

  tp = tbuff;
  safe_str(crypt_code(pass, as->text, 1), tbuff, &tp);
  *tp = '\0';
  memcpy(as->text, tbuff, as->len);

  if (nargs == 3 && parse_boolean(args[2])) {
    tp = tbuff;
    safe_ansi_string(as, 0, as->len, tbuff, &tp);
    if (!encode_base64(tbuff, tp - tbuff, buff, bp)) {
      free_ansi_string(as);
      return;
    }
  } else
    safe_ansi_string(as, 0, as->len, buff, bp);

  free_ansi_string(as);
}

FUNCTION(fun_decrypt)
{
  char tbuff[BUFFER_LEN], *tp;
  char *pass;
  size_t len;
  ansi_string *as;
  char *input;

  if (nargs == 3 && parse_boolean(args[2])) {
    tp = tbuff;
    if (!decode_base64(args[0], arglens[0], tbuff, &tp)) {
      safe_strl(tbuff, tp - tbuff, buff, bp);
      return;
    }
    *tp = '\0';
    input = tbuff;
  } else
    input = args[0];

  as = parse_ansi_string(input);

  pass = remove_markup(args[1], &len);

  tp = tbuff;
  safe_str(crypt_code(pass, as->text, 0), tbuff, &tp);
  *tp = '\0';
  memcpy(as->text, tbuff, as->len + 1);
  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}

FUNCTION(fun_checkpass)
{
  dbref it = match_thing(executor, args[0]);
  if (!(GoodObject(it) && IsPlayer(it))) {
    safe_str(T("#-1 NO SUCH PLAYER"), buff, bp);
    return;
  }
  safe_boolean(password_check(it, args[1]), buff, bp);
}

FUNCTION(fun_sha0)
{
  unsigned char hash[SHA_DIGEST_LENGTH];

  SHA((unsigned char *) args[0], arglens[0], hash);

  safe_hexstr(hash, SHA_DIGEST_LENGTH, buff, bp);
}

/* From mycrypt.c */
int safe_hash_byname(const char *algo, const char *plaintext, int len,
                     char *buff, char **bp, bool inplace_err);


#if OPENSSL_VERSION_NUMBER >= 0x10000000L
#define CAN_LIST_DIGESTS

static void
list_dgst_populate(const EVP_MD *m, const char *from
                   __attribute__ ((__unused__)), const char *to
                   __attribute__ ((__unused__)), void *data)
{
  HASHTAB *digests = data;
  if (m)
    hash_add(digests, EVP_MD_name(m), "foo");
}

#endif

FUNCTION(fun_digest)
{
  if (nargs == 1 && strcmp(args[0], "list") == 0) {
#ifdef CAN_LIST_DIGESTS
    HASHTAB digests_tab;
    const char **digests;
    const char *d;
    int m, n;

    hashinit(&digests_tab, 100);
    EVP_MD_do_all(list_dgst_populate, &digests_tab);
    digests = mush_calloc(digests_tab.entries, sizeof(char *), "digest.list");

    for (n = 0, d = hash_firstentry_key(&digests_tab);
         d; n += 1, d = hash_nextentry_key(&digests_tab))
      digests[n] = d;

    qsort(digests, n, sizeof(char *), stri_comp);

    for (m = 0; m < n; m += 1) {
      if (m > 0)
        safe_chr(' ', buff, bp);
      safe_str(digests[m], buff, bp);
    }

    mush_free(digests, "digest.list");
    hashfree(&digests_tab);
#else
    safe_str(T("#-1 LISTING NOT SUPPORTED"), buff, bp);
#endif
  } else if (nargs == 2)
    safe_hash_byname(args[0], args[1], arglens[1], buff, bp, 1);
  else
    safe_str(T("#-1 INVALID ARGUMENT"), buff, bp);
}
