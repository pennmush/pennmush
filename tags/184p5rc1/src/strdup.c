/**
 * \file strdup.c
 *
 * \brief strdup routine for systems without one.
 *
 *
 */

#include "config.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "conf.h"
#include "copyrite.h"
#include "mymalloc.h"
#include "confmagic.h"

#ifndef HAVE_STRDUP
char *strdup(const char *s);

/** strdup for systems without one.
 * \param s string to duplicate.
 * \return newly-allocated copy of s
 */
char *
strdup(const char *s)
{
  int len;
  char *dup = NULL;

  len = strlen(s) + 1;
  if ((dup = (char *) malloc(len)) != NULL)
    memcpy(dup, s, len);
  return (dup);
}
#endif
