#ifndef _SHS_H
#define _SHS_H

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/* -------------- SHS.H --------------------------- */

#ifdef _WIN32
#define LONG SHS_LONG
#endif

typedef uint8_t BYTE;
typedef uint32_t LONG;          /* Ick. */

#define SHS_BLOCKSIZE 64

#define SHS_DIGESTSIZE 20

/** An shs digest.
 */
typedef struct {
  LONG digest[5];               /**< message digest */
  LONG countLo;                 /**< 64-bit bit count, low half */
  LONG countHi;                 /**< 64-bit bit count, high half */
  LONG data[16];                /**< SHS data buffer */
  BYTE reverse_wanted;          /**< true to reverse (little_endian) false to not */
} SHS_INFO;

void shsInit(SHS_INFO *shsInfo);
void shsUpdate(SHS_INFO *shsInfo, const BYTE *buffer, int count);
void shsFinal(SHS_INFO *shsInfo);
#endif
