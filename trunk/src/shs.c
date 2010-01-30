/**
 * \file shs.c
 *
 * \brief The secure hash algorithm.
 *
 * Written 2 September 1992. Peter C. Gutmann.
 * This implementation placed in the public domain.
 *
 * Comments to pgutl@cs.aukuni.ac.nz  
 *
 * Keyed in from: "Applied Cryptography by Bruce Schneier"
 *
 * by Nick Gammon.
 *
 * Note: include files below are slightly different from the original source
 * in order to confirm to the MUSH include files standards.
 */

#include "copyrite.h"
#include "config.h"

#ifndef HAS_OPENSSL

#include <string.h>
#include "shs.h"
#include "confmagic.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/* The SHS f() functions */

#define f1(x, y, z) ( (x & y) | (~x & z) )      /* rounds 0-19 */
#define f2(x, y, z) ( x ^ y ^ z )       /* rounds 20-39 */
#define f3(x, y, z) ( (x & y) | (x & z) | (y & z) )     /* rounds 40-59 */
#define f4(x, y, z) ( x ^ y ^ z )       /* rounds 60-79 */

/* The SHS mysterious constants */

#define K1 0x5A827999L
#define K2 0x6ED9EBA1L
#define K3 0x8F1BBCDCL
#define K4 0xCA62C1D6L

/* SHS initial values */

#define h0init 0x67452301L
#define h1init 0xEFCDAB89L
#define h2init 0x98BADCFEL
#define h3init 0x10325476L
#define h4init 0xC3D2E1F0L

/* 32-bit rotate - kludged with shifts */

#define S(n, X) ( ( X << n) | ( X >> ( 32 - n) ) )

/* The initial expanding functions */

#define expand(count) W [count] = W [count - 3] ^ W [count - 8] ^  \
                      W [count - 14] ^ W [count - 16]

/* The four SHS sub-rounds */

#define subRound1(count)   \
  { \
  temp = S (5, A) + f1 (B, C, D) + E + W [count] + K1;  \
  E = D;  \
  D = C;  \
  C = S (30, B);  \
  B = A;  \
  A = temp;  \
  }

#define subRound2(count)   \
  { \
  temp = S (5, A) + f2 (B, C, D) + E + W [count] + K2;  \
  E = D;  \
  D = C;  \
  C = S (30, B);  \
  B = A;  \
  A = temp;  \
  }


#define subRound3(count)   \
  { \
  temp = S (5, A) + f3 (B, C, D) + E + W [count] + K3;  \
  E = D;  \
  D = C;  \
  C = S (30, B);  \
  B = A;  \
  A = temp;  \
  }


#define subRound4(count)   \
  { \
  temp = S (5, A) + f4 (B, C, D) + E + W [count] + K4;  \
  E = D;  \
  D = C;  \
  C = S (30, B);  \
  B = A;  \
  A = temp;  \
  }


/* The two buffers of 5 32-bit words */

LONG h0, h1, h2, h3, h4;
LONG A, B, C, D, E;

#endif                          /* DOXYGEN_SHOULD_SKIP_THIS */

/* Initialise the SHS values */

/** Initialize SHS values. 
 * \param shsInfo pointer to shs data structure.
 */
void
shsInit(SHS_INFO *shsInfo)
{
  /* Set the h-vars to their initial values */
  shsInfo->digest[0] = h0init;
  shsInfo->digest[1] = h1init;
  shsInfo->digest[2] = h2init;
  shsInfo->digest[3] = h3init;
  shsInfo->digest[4] = h4init;

  /* Initialise bit count */
  shsInfo->countLo = shsInfo->countHi = 0;

}


static void shsTransform(SHS_INFO *shsInfo);

/* Perform the SHS transformation. Note that this code, like MD5, seems to
 * break some optimizing compilers - it may be necessary to split it into 
 * sections, e.g. based on the four subrounds.
 */
static void
shsTransform(SHS_INFO *shsInfo)
{
  LONG W[80], temp;
  int i;

  /* Step A. Copy the data buffer into the local work buffer */

  for (i = 0; i < 16; i++)
    W[i] = shsInfo->data[i];

  /* Step B. Expand the 16 words into 64 temporary data words */

  expand(16);
  expand(17);
  expand(18);
  expand(19);
  expand(20);
  expand(21);
  expand(22);
  expand(23);
  expand(24);
  expand(25);
  expand(26);
  expand(27);
  expand(28);
  expand(29);
  expand(30);
  expand(31);
  expand(32);
  expand(33);
  expand(34);
  expand(35);
  expand(36);
  expand(37);
  expand(38);
  expand(39);
  expand(40);
  expand(41);
  expand(42);
  expand(43);
  expand(44);
  expand(45);
  expand(46);
  expand(47);
  expand(48);
  expand(49);
  expand(50);
  expand(51);
  expand(52);
  expand(53);
  expand(54);
  expand(55);
  expand(56);
  expand(57);
  expand(58);
  expand(59);
  expand(60);
  expand(61);
  expand(62);
  expand(63);
  expand(64);
  expand(65);
  expand(66);
  expand(67);
  expand(68);
  expand(69);
  expand(70);
  expand(71);
  expand(72);
  expand(73);
  expand(74);
  expand(75);
  expand(76);
  expand(77);
  expand(78);
  expand(79);

  /* Step C. Set up first buffer */

  A = shsInfo->digest[0];
  B = shsInfo->digest[1];
  C = shsInfo->digest[2];
  D = shsInfo->digest[3];
  E = shsInfo->digest[4];

  /* Step D. Serious mangling, divided into four subrounds */

  subRound1(0);
  subRound1(1);
  subRound1(2);
  subRound1(3);
  subRound1(4);
  subRound1(5);
  subRound1(6);
  subRound1(7);
  subRound1(8);
  subRound1(9);
  subRound1(10);
  subRound1(11);
  subRound1(12);
  subRound1(13);
  subRound1(14);
  subRound1(15);
  subRound1(16);
  subRound1(17);
  subRound1(18);
  subRound1(19);
  subRound2(20);
  subRound2(21);
  subRound2(22);
  subRound2(23);
  subRound2(24);
  subRound2(25);
  subRound2(26);
  subRound2(27);
  subRound2(28);
  subRound2(29);
  subRound2(30);
  subRound2(31);
  subRound2(32);
  subRound2(33);
  subRound2(34);
  subRound2(35);
  subRound2(36);
  subRound2(37);
  subRound2(38);
  subRound2(39);
  subRound3(40);
  subRound3(41);
  subRound3(42);
  subRound3(43);
  subRound3(44);
  subRound3(45);
  subRound3(46);
  subRound3(47);
  subRound3(48);
  subRound3(49);
  subRound3(50);
  subRound3(51);
  subRound3(52);
  subRound3(53);
  subRound3(54);
  subRound3(55);
  subRound3(56);
  subRound3(57);
  subRound3(58);
  subRound3(59);
  subRound4(60);
  subRound4(61);
  subRound4(62);
  subRound4(63);
  subRound4(64);
  subRound4(65);
  subRound4(66);
  subRound4(67);
  subRound4(68);
  subRound4(69);
  subRound4(70);
  subRound4(71);
  subRound4(72);
  subRound4(73);
  subRound4(74);
  subRound4(75);
  subRound4(76);
  subRound4(77);
  subRound4(78);
  subRound4(79);

  /* Step E. Build message digest */

  shsInfo->digest[0] += A;
  shsInfo->digest[1] += B;
  shsInfo->digest[2] += C;
  shsInfo->digest[3] += D;
  shsInfo->digest[4] += E;

}                               /* end of shsTransform */

static void byteReverse(LONG *buffer, int byteCount);
static void
byteReverse(LONG *buffer, int byteCount)
{
  LONG value;
  int count;

  byteCount /= sizeof(LONG);

  for (count = 0; count < byteCount; count++) {
    value = (buffer[count] << 16) | (buffer[count] >> 16);
    buffer[count] = ((value & 0xFF00FF00L) >> 8) | ((value & 0x00FF00FFL) << 8);
  }
}                               /* end of byteReverse */


/** Update SHS for a block of data. This code assumes that the buffer
 * size is a multiple of SHS_BLOCKSIZE bytes long, which makes the
 * code a lot more efficient since it does away with the need to
 * handle partial blocks between calls to shsUpdate() 
 * \param shsInfo pointer to shs data.
 * \param buffer block of data to update the hash with.
 * \param count size of buffer in bytes.
 */
void
shsUpdate(SHS_INFO *shsInfo, const BYTE *buffer, int count)
{
  /* Update bitcount */

  if ((shsInfo->countLo + ((LONG) count << 3)) < shsInfo->countLo)
    shsInfo->countHi++;         /* Carry from low to high bitcount */

  shsInfo->countLo += ((LONG) count << 3);
  shsInfo->countHi += ((LONG) count >> 29);

  /* process data in SHS_BLOCKSIZE chunks */

  while (count >= SHS_BLOCKSIZE) {
    memcpy(shsInfo->data, buffer, SHS_BLOCKSIZE);
    if (shsInfo->reverse_wanted)
      byteReverse(shsInfo->data, SHS_BLOCKSIZE);
    shsTransform(shsInfo);
    buffer += SHS_BLOCKSIZE;
    count -= SHS_BLOCKSIZE;
  }                             /* end of looping processing each chunk */

  /* Handle any remaining bytes of data. This should only happen once on the final lot of data */

  memcpy(shsInfo->data, buffer, count);

}

/** Finalize the SHS hash.
 * \param shsInfo pointer to shs data.
 */
void
shsFinal(SHS_INFO *shsInfo)
{
  int count;
  LONG lowBitcount = shsInfo->countLo, highBitcount = shsInfo->countHi;

  /* compute number of bytes mod 64 */

  count = (int) ((shsInfo->countLo >> 3) & 0x3F);

  /* set the first char of padding to 0x80. This is safe since there is always at */
  /* least one byte free */

  ((BYTE *) shsInfo->data)[count++] = 0x80;

  /* pad out to 56 mod 64 */

  if (count > 56) {
    /* two lots of padding: Pad the first block to 64 bytes */

    memset((BYTE *) shsInfo->data + count, 0, 64 - count);

    if (shsInfo->reverse_wanted)
      byteReverse(shsInfo->data, SHS_BLOCKSIZE);

    shsTransform(shsInfo);

    /* now fill the next block with 56 bytes */

    memset((BYTE *) shsInfo->data, 0, 56);
  }
  /* end of count being > 56 */
  else
    /* pad block to 56 bytes */
    memset((BYTE *) shsInfo->data + count, 0, 56 - count);

  if (shsInfo->reverse_wanted)
    byteReverse(shsInfo->data, SHS_BLOCKSIZE);

  /* append length in bits and transform */

  shsInfo->data[14] = highBitcount;
  shsInfo->data[15] = lowBitcount;

  shsTransform(shsInfo);

  if (shsInfo->reverse_wanted)
    byteReverse(shsInfo->data, SHS_DIGESTSIZE);
}

#endif                          /* HAS_OPENSSL */


static void dummy(void) __attribute__((__unused__));
static void
dummy(void)
{
  /* This function exists to suppress a warning about an empty source
     file. */
  return;
}
