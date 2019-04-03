#ifndef _CHUNK_H_
#define _CHUNK_H_

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "mushtype.h"

typedef uintptr_t chunk_reference_t;
#define NULL_CHUNK_REFERENCE 0

chunk_reference_t chunk_create(char const *data, uint16_t len, uint8_t derefs);
void chunk_delete(chunk_reference_t reference);
uint16_t chunk_fetch(chunk_reference_t reference, char *buffer,
                     uint16_t buffer_len);
uint16_t chunk_len(chunk_reference_t reference);
uint8_t chunk_derefs(chunk_reference_t reference);


#endif /* _CHUNK_H_ */
