/**
 * \file mycrypt.c
 *
 * \brief Password encryption for PennMUSH
 *
 * Routines for hashing passwords and comparing against them.
 * Also see player.c.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef WIN32
#include <Windows.h>
#include <ntstatus.h>
#include <Bcrypt.h>
#else
#include <openssl/sha.h>
#include <openssl/evp.h>
#endif
#include "conf.h"
#include "mypcre.h"
#include "log.h"
#include "notify.h"
#include "strutil.h"
#include "externs.h"
#include "mymalloc.h"

#define PASSWORD_HASH "sha512"

bool decode_base64(char *encoded, int len, bool printonly, char *buff,
                   char **bp);
bool check_mux_password(const char *saved, const char *password);
char *mush_crypt_sha0(const char *key);
int safe_hash_byname(const char *algo, const char *plaintext, int len,
                     char *buff, char **bp, bool inplace_err);
char *password_hash(const char *key, const char *algo);
bool password_comp(const char *saved, const char *pass);

/** Encrypt a password and return ciphertext, using SHA0. Icky old
 *  style password format, used for migrating to new style.
 *
 * \param key plaintext to encrypt.
 * \return encrypted password.
 */
char *
mush_crypt_sha0(const char *key __attribute__((__unused__)))
{
#ifdef HAVE_SHA
  static char crypt_buff[70];
  uint8_t hash[SHA_DIGEST_LENGTH];
  unsigned int a, b;

  SHA((uint8_t *) key, strlen(key), hash);

  memcpy(&a, hash, sizeof a);
  memcpy(&b, hash + sizeof a, sizeof b);

  if (options.reverse_shs) {
    int ra, rb;
    ra = (a << 16) | (a >> 16);
    rb = (b << 16) | (b >> 16);
    a = ((ra & 0xFF00FF00L) >> 8) | ((ra & 0x00FF00FFL) << 8);
    b = ((rb & 0xFF00FF00L) >> 8) | ((rb & 0x00FF00FFL) << 8);
  }

  /* TODO: SHA-0 is already considered insecure, but due to the lack of
   * delimiters, this matches far more than it should. For example, suppose
   * a= 23 and b=456. Anything which hashed to a=1, b=23456 or a=12, b=3456
   * would also erroneously match! */
  snprintf(crypt_buff, sizeof crypt_buff, "XX%u%u", a, b);

  return crypt_buff;
#else
  return "";
#endif
}

#ifdef WIN32
static const wchar_t *
lookup_bcrypt_algo(const char *name)
{
  const struct hashname_map {
    const char *name;
    const wchar_t *algo;
  } names[] = {
    {"MD2", BCRYPT_MD2_ALGORITHM},       {"MD4", BCRYPT_MD4_ALGORITHM},
    {"MD5", BCRYPT_MD5_ALGORITHM},       {"SHA1", BCRYPT_SHA1_ALGORITHM},
    {"SHA256", BCRYPT_SHA256_ALGORITHM}, {"SHA384", BCRYPT_SHA384_ALGORITHM},
    {"SHA512", BCRYPT_SHA512_ALGORITHM}, {NULL, NULL}};
  for (int n = 0; names[n].name; n += 1) {
    if (_stricmp(name, names[n].name) == 0) {
      return names[n].algo;
    }
  }
  return NULL;
}
#endif

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
#ifdef WIN32
  const wchar_t *dgst;
  BCRYPT_ALG_HANDLE balgo;
  BCRYPT_HASH_HANDLE hfun;
  PUCHAR hash;
  DWORD hashlen = 0;
  ULONG cbhash = 0;
  int r;

  dgst = lookup_bcrypt_algo(algo);
  if (!dgst) {
    if (inplace_err)
      safe_str(T("#-1 UNSUPPORTED DIGEST TYPE"), buff, bp);
    else
      do_rawlog(LT_ERR, "safe_hash_byname: Unknown password hash function: %s",
                algo);
    return 1;
  }

  if (BCryptOpenAlgorithmProvider(&balgo, dgst, NULL, 0) != STATUS_SUCCESS) {
    if (inplace_err)
      safe_str(T("#-1 UNSUPPORTED DIGEST TYPE"), buff, bp);
    else
      do_rawlog(LT_ERR, "safe_hash_byname: Unknown password hash function: %s",
                algo);
    return 1;
  }
  if (BCryptCreateHash(balgo, &hfun, NULL, 0, NULL, 0, 0) != STATUS_SUCCESS) {
    if (inplace_err)
      safe_str(T("#-1 UNSUPPORTED DIGEST TYPE"), buff, bp);
    else
      do_rawlog(LT_ERR, "safe_hash_byname: Unknown password hash function: %s",
                algo);
    BCryptCloseAlgorithmProvider(balgo, 0);
    return 1;
  }
  if (BCryptGetProperty(balgo, BCRYPT_HASH_LENGTH, (PBYTE) &hashlen,
                        sizeof(hashlen), &cbhash, 0) != STATUS_SUCCESS) {
    if (inplace_err)
      safe_str(T("#-1 UNSUPPORTED DIGEST TYPE"), buff, bp);
    else
      do_rawlog(LT_ERR, "safe_hash_byname: Unknown password hash function: %s",
                algo);
    BCryptDestroyHash(hfun);
    BCryptCloseAlgorithmProvider(balgo, 0);
    return 1;
  }
  BCryptHashData(hfun, (PUCHAR) plaintext, (ULONG) len, 0);
  hash = mush_malloc(hashlen + 1, "string");
  BCryptFinishHash(hfun, hash, hashlen, 0);
  hash[hashlen] = '\0';
  BCryptDestroyHash(hfun);
  BCryptCloseAlgorithmProvider(balgo, 0);
  r = safe_hexstr(hash, hashlen, buff, bp);
  mush_free(hash, "string");
  return r;
#else
  EVP_MD_CTX *ctx;
  const EVP_MD *md;
  uint8_t hash[EVP_MAX_MD_SIZE];
  unsigned int rlen = EVP_MAX_MD_SIZE;

  md = EVP_get_digestbyname(algo);
  if (!md) {
    if (inplace_err)
      safe_str(T("#-1 UNSUPPORTED DIGEST TYPE"), buff, bp);
    else
      do_rawlog(LT_ERR, "safe_hash_byname: Unknown password hash function: %s",
                algo);
    return 1;
  }
  ctx = EVP_MD_CTX_create();
  EVP_DigestInit(ctx, md);
  EVP_DigestUpdate(ctx, plaintext, len);
  EVP_DigestFinal(ctx, hash, &rlen);
  EVP_MD_CTX_destroy(ctx);

  return safe_hexstr(hash, rlen, buff, bp);
#endif
}

bool
check_mux_password(const char *saved, const char *password)
{
  uint8_t hash[BUFFER_LEN];
  unsigned int rlen = BUFFER_LEN;
  char decoded[BUFFER_LEN];
  char *dp;
  char *start, *end;
  char *algo;

  start = (char *) saved;

  /* MUX passwords start with a '$' */
  if (*start != '$')
    return 0;

  start++;
  /* The next '$' marks the end of the encryption algo */
  end = strchr(start, '$');
  if (end == NULL)
    return 0;

  *end++ = '\0';

  algo = start;

  start = end;
  /* Up until the next '$' is the salt. After that is the password */
  end = strchr(start, '$');
  if (end == NULL)
    return 0;

  *end++ = '\0';

  /* 'start' now holds the salt, 'end' the password.
   * Both are base64-encoded. */

  /* decode the salt */
  dp = decoded;
  decode_base64(start, strlen(start), 0, decoded, &dp);
  *dp = '\0';
  /* Double-hash the password */
#ifdef WIN32
  {
    BCRYPT_ALG_HANDLE balgo;
    BCRYPT_HASH_HANDLE hfun;
    ULONG hashlen = 0, cbhash = 0;

    const wchar_t *dgst = lookup_bcrypt_algo(algo);
    if (!dgst) {
      return 0;
    }
    if (BCryptOpenAlgorithmProvider(&balgo, dgst, NULL, 0) != STATUS_SUCCESS) {
      return 0;
    }
    if (BCryptCreateHash(balgo, &hfun, NULL, 0, NULL, 0, 0) != STATUS_SUCCESS) {
      BCryptCloseAlgorithmProvider(balgo, 0);
      return 0;
    }
    if (BCryptGetProperty(balgo, BCRYPT_HASH_LENGTH, (PBYTE) &hashlen,
                          sizeof(DWORD), &cbhash, 0) != STATUS_SUCCESS) {
      BCryptDestroyHash(hfun);
      BCryptCloseAlgorithmProvider(balgo, 0);
      return 0;
    }
    BCryptHashData(hfun, (PUCHAR) start, (ULONG) strlen(start), 0);
    BCryptHashData(hfun, (PUCHAR) password, (ULONG) strlen(password), 0);
    BCryptFinishHash(hfun, (PUCHAR) hash, hashlen, 0);
    hash[hashlen] = '\0';
    rlen = hashlen;
    BCryptDestroyHash(hfun);
    BCryptCloseAlgorithmProvider(balgo, 0);
  }
#else
  {
    EVP_MD_CTX *ctx;
    const EVP_MD *md;
    md = EVP_get_digestbyname(algo);
    if (!md)
      return 0;
    ctx = EVP_MD_CTX_create();
    EVP_DigestInit(ctx, md);
    EVP_DigestUpdate(ctx, start, strlen(start));
    EVP_DigestUpdate(ctx, password, strlen(password));
    EVP_DigestFinal(ctx, hash, &rlen);
    EVP_MD_CTX_destroy(ctx);
  }
#endif

  /* Decode the stored password */
  dp = decoded;
  decode_base64(end, strlen(end), 0, decoded, &dp);
  *dp = '\0';

  if (rlen != (dp - decoded))
    return 0;

  /* Compare stored to hashed */
  return memcmp(decoded, hash, rlen) == 0;
}

/** Encrypt a password and return the formatted password
 * string. Supports user-supplied algorithms. Password format:
 *
 * V:ALGO:HASH:TIMESTAMP
 *
 * V is the version number (Currently 2), ALGO is the digest algorithm
 * used (Default is SHA1), HASH is the hashed password. TIMESTAMP is
 * when it was set. If fields are added, the version gets bumped.
 *
 * HASH is salted; the first two characters of the hashed password are
 * randomly chosen characters that are added to the start of the
 * plaintext password before it's hashed. This way two characters with
 * the same password will have different hashed ones.
 *
 * \param key The plaintext password to hash.
 * \param algo The digest algorithm to use. If NULL, uses SHA-1.
 * \return A static buffer holding the formatted password string.
 */
char *
password_hash(const char *key, const char *algo)
{
  static char buff[BUFFER_LEN];
  static char *salts =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  char s1, s2;
  char *bp;
  int len;
  char hbuff[BUFFER_LEN + 2];

  if (!algo) {
    algo = PASSWORD_HASH;
  }

  len = strlen(key);

  s1 = salts[get_random_u32(0, 61)];
  s2 = salts[get_random_u32(0, 61)];

  bp = buff;
  safe_strl("2:", 2, buff, &bp);
  safe_str(algo, buff, &bp);
  safe_chr(':', buff, &bp);
  safe_chr(s1, buff, &bp);
  safe_chr(s2, buff, &bp);
  snprintf(hbuff, sizeof hbuff, "%c%c%s", s1, s2, key);
  safe_hash_byname(algo, hbuff, len + 2, buff, &bp, 0);
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
  static pcre2_code *passwd_re = NULL;
  static pcre2_match_data *passwd_md = NULL;
  char buff[BUFFER_LEN], *bp;
  char *version = NULL, *algo = NULL, *shash = NULL;
  PCRE2_SIZE versionlen, algolen, shashlen;
  int len, slen;
  int c, r;
  int retval = 0;

  if (!passwd_re) {
    static const PCRE2_UCHAR re[] = "^(\\d+):(\\w+):([0-9a-zA-Z]+):\\d+";
    int errcode;
    PCRE2_SIZE erroffset;
    passwd_re = pcre2_compile(re, PCRE2_ZERO_TERMINATED, re_compile_flags,
                              &errcode, &erroffset, re_compile_ctx);
    if (!passwd_re) {
      char errstr[120];
      pcre2_get_error_message(errcode, (PCRE2_UCHAR *) errstr, sizeof errstr);
      do_rawlog(LT_ERR, "Unable to compile password regexp: %s, at '%c'",
                errstr, re[erroffset]);
      return 0;
    }
    pcre2_jit_compile(passwd_re, PCRE2_JIT_COMPLETE);
    passwd_md = pcre2_match_data_create_from_pattern(passwd_re, NULL);
  }

  len = strlen(pass);
  slen = strlen(saved);

  if ((c = pcre2_match(passwd_re, (const PCRE2_UCHAR *) saved, slen, 0,
                       re_match_flags, passwd_md, re_match_ctx)) < 0) {
    /* Not a well-formed password string. */
    return 0;
  }

  pcre2_substring_get_bynumber(passwd_md, 1, (PCRE2_UCHAR **) &version,
                               &versionlen);
  pcre2_substring_get_bynumber(passwd_md, 2, (PCRE2_UCHAR **) &algo, &algolen);
  pcre2_substring_get_bynumber(passwd_md, 3, (PCRE2_UCHAR **) &shash,
                               &shashlen);

  /* Hash the plaintext password using the right digest */
  bp = buff;
  if (strcmp(version, "1") == 0) {
    r = safe_hash_byname(algo, pass, len, buff, &bp, 0);
  } else if (strcmp(version, "2") == 0) {
    /* Salted password */
    char hbuff[BUFFER_LEN + 2];
    safe_chr(shash[0], buff, &bp);
    safe_chr(shash[1], buff, &bp);
    snprintf(hbuff, sizeof hbuff, "%c%c%s", shash[0], shash[1], pass);
    r = safe_hash_byname(algo, hbuff, len + 2, buff, &bp, 0);
  } else {
    /* Unknown password format version */
    retval = 0;
    goto cleanup;
  }

  if (r) {
    retval = 0;
    goto cleanup;
  }

  /* And compare against the saved one */
  *bp = '\0';
  retval = strcmp(shash, buff) == 0;

cleanup:
  pcre2_substring_free((PCRE2_UCHAR *) version);
  pcre2_substring_free((PCRE2_UCHAR *) algo);
  pcre2_substring_free((PCRE2_UCHAR *) shash);
  return retval;
}
