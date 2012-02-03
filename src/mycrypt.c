/**
 * \file mycrypt.c
 *
 * \brief Password encryption for PennMUSH
 *
 * Routines for hashing passwords and comparing against them.
 * Also see player.c.
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <pcre.h>
#include "conf.h"
#include "externs.h"
#include "log.h"
#include "confmagic.h"

#define PASSWORD_HASH "sha1"

/** Encrypt a password and return ciphertext, using SHA0. Icky old
 *  style password format, used for migrating to new style.
 * 
 * \param key plaintext to encrypt.
 * \return encrypted password.
 */
char *
mush_crypt_sha0(const char *key)
{
  static char crypt_buff[70];
  unsigned char hash[SHA_DIGEST_LENGTH];
  unsigned int a, b;

  SHA((unsigned char *) key, strlen(key), hash);

  memcpy(&a, hash, sizeof a);
  memcpy(&b, hash + sizeof a, sizeof b);

  if (options.reverse_shs) {
    int ra, rb;
    ra = (a << 16) | (a >> 16);
    rb = (b << 16) | (b >> 16);
    a = ((ra & 0xFF00FF00L) >> 8) | ((ra & 0x00FF00FFL) << 8);
    b = ((rb & 0xFF00FF00L) >> 8) | ((rb & 0x00FF00FFL) << 8);
  }

  sprintf(crypt_buff, "XX%u%u", a, b);

  return crypt_buff;
}

/** Hash a string and store it base-16 encoded in a buffer.
 * \param algo the name of the hash algorithm (sha1, md5, etc.)
 * \param plaintext the text to hash.
 * \param len length of text to hash.
 * \param buff where to store it.
 * \param bp pointer into buff to store at.
 * \param inplace_err true to put error messages in buff.
 * \return 1 on failure, 0 on success.
 */
int
safe_hash_byname(const char *algo, const char *plaintext, int len, char *buff,
                 char **bp, bool inplace_err)
{
  EVP_MD_CTX ctx;
  const EVP_MD *md;
  uint8_t hash[EVP_MAX_MD_SIZE];
  unsigned int rlen = EVP_MAX_MD_SIZE;

  md = EVP_get_digestbyname(algo);
  if (!md) {
    if (inplace_err)
      safe_str(T("#-1 UNSUPPORTED DIGEST TYPE"), buff, bp);
    else
      do_rawlog(LT_ERR,
                T("safe_hash_byname: Unknown password hash function: %s"),
                algo);
    return 1;
  }

  EVP_DigestInit(&ctx, md);
  EVP_DigestUpdate(&ctx, plaintext, len);
  EVP_DigestFinal(&ctx, hash, &rlen);

  return safe_hexstr(hash, rlen, buff, bp);
}

/** Encrypt a password and return the formatted password
 * string. Supports user-supplied algorithms. Password format:
 *
 * V:ALGO:HASH:TIMESTAMP
 * 
 * V is the version number (Currently 1), ALGO is the digest algorithm
 * used (Default is SHA1), HASH is the hashed password. TIMESTAMP is
 * when it was set. If fields are added, the version gets bumped.
 *
 * \param key The plaintext password to hash.
 * \param algo The digest algorithm to use. If NULL, uses SHA-1.
 * \return A static buffer holding the formatted password string.
 */
char *
password_hash(const char *key, const char *algo)
{
  static char buff[BUFFER_LEN];
  char *bp;
  int len;

  if (!algo)
    algo = PASSWORD_HASH;

  len = strlen(key);

  bp = buff;
  safe_strl("1:", 2, buff, &bp);
  safe_str(algo, buff, &bp);
  safe_chr(':', buff, &bp);
  safe_hash_byname(algo, key, len, buff, &bp, 0);
  safe_chr(':', buff, &bp);
  safe_time_t(time(NULL), buff, &bp);
  *bp = '\0';

  return buff;
}

extern const unsigned char *tables;

/** Compare a plaintext password against a hashed password.
 *
 * \param saved The contents of a player's password attribute.
 * \param pass The plain-text password.
 * \return true or false.
 */
bool
password_comp(const char *saved, const char *pass)
{
  static pcre *passwd_re = NULL;
  char buff[BUFFER_LEN], *bp;
  const char *algo = NULL, *shash = NULL;
  int len, slen;
  int ovec[30];
  int c;

  if (!passwd_re) {
    static const char *re = "^\\d+:(\\w+):([0-9a-zA-Z]+):\\d+";
    const char *errptr;
    int erroffset = 0;
    passwd_re = pcre_compile(re, 0, &errptr, &erroffset, tables);
    if (!passwd_re) {
      do_rawlog(LT_ERR, "Unable to compile password regexp: %s, at '%c'",
                errptr, re[erroffset]);
      return 0;
    }
  }

  len = strlen(pass);
  slen = strlen(saved);

  if ((c = pcre_exec(passwd_re, NULL, saved, slen, 0, 0, ovec, 30)) <= 0) {
    /* Not a well-formed password string. */
    return 0;
  }

  pcre_get_substring(saved, ovec, c, 1, &algo);
  pcre_get_substring(saved, ovec, c, 2, &shash);

  /* Hash the plaintext password using the right digest */
  bp = buff;
  if (safe_hash_byname(algo, pass, len, buff, &bp, 0)) {
    pcre_free_substring(algo);
    pcre_free_substring(shash);
    return 0;
  }
  *bp = '\0';

  /* And compare against the saved one */
  c = strcmp(shash, buff);
  pcre_free_substring(algo);
  pcre_free_substring(shash);

  return c == 0;

}
