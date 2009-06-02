/**
 * \file mycrypt.c
 *
 * \brief Password encryption for PennMUSH
 *
 * This file defines the function mush_crypt(key) used for password
 * encryption, depending on the system. Actually, we pretty much
 * expect to use SHS.
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "conf.h"
#ifdef HAS_OPENSSL
#include <openssl/sha.h>
#else
#include "shs.h"
#endif
#include "confmagic.h"

char *mush_crypt(const char *key);

/** Encrypt a password and return ciphertext.
 * \param key plaintext to encrypt.
 * \return encrypted password.
 */
char *
mush_crypt(const char *key)
{
  static char crypt_buff[70];

#ifdef HAS_OPENSSL
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

#else
  SHS_INFO shsInfo;

  shsInfo.reverse_wanted = (BYTE) options.reverse_shs;
  shsInit(&shsInfo);
  shsUpdate(&shsInfo, (const BYTE *) key, strlen(key));
  shsFinal(&shsInfo);
  sprintf(crypt_buff, "XX%lu%lu", shsInfo.digest[0], shsInfo.digest[1]);
#endif

  return crypt_buff;
}
