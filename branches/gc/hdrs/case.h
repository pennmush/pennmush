/**
 * \file case.h
 *
 * \brief Routines for upper/lower casing characters
 */

#ifndef CASE_H
#define CASE_H
#include <ctype.h>
#include "config.h"

#ifdef HAVE_SAFE_TOUPPER
#define DOWNCASE(x)     tolower((unsigned char)x)
#define UPCASE(x)       toupper((unsigned char)x)
#else
#define DOWNCASE(x) (isupper((unsigned char)x) ? tolower((unsigned char)x) : (x))
#define UPCASE(x)   (islower((unsigned char)x) ? toupper((unsigned char)x) : (x))
#endif
#endif                          /* CASE_H */
