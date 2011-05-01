/**
 * \file compress.c
 *
 * \brief Compression routine wrapper file for PennMUSH.
 *
 * This file does nothing but conditionally include the appropriate
 * attribute compression source code.
 *
 */

#include "config.h"
#include "options.h"
#include <time.h>
/* It's rather dumb mushtype.h has to be included */
#include "mushtype.h"
#include "log.h"

#if defined(COMPRESSION_TYPE) && (COMPRESSION_TYPE == 0)
/* No compression */
char ucbuff[BUFFER_LEN];        /**< Dummy buffer for no compression */

#elif (COMPRESSION_TYPE == 1) || (COMPRESSION_TYPE == 2)
/* Huffman compression */
#include "comp_h.c"

#elif (COMPRESSION_TYPE == 3)
/* Word compression - necessary for Win32 */
#include "comp_w.c"
#elif (COMPRESSION_TYPE == 4)
/* Nearly 8-bit clean word compression. Prefer 3 unless you're using a 
 * language with an extended character set. 0x06 is the only character
 * we can't encode right now.
 */
#include "comp_w8.c"
#else
/* You didn't define it, or gave an invalid value!
 * Lucky for you, we're forgiving. You get no compression.
 */
char ucbuff[BUFFER_LEN];

#endif                          /* Compression type checks */
