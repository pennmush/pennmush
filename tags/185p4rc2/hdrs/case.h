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
#define DOWNCASE(x)     tolower((unsigned char)x) /**< Returns 'x' lowercased */
#define UPCASE(x)       toupper((unsigned char)x) /**< Returns 'x' uppercased */
#else
#define DOWNCASE(x) (isupper((unsigned char)x) ? tolower((unsigned char)x) : (x)) /**< Returns 'x' lowercased */
#define UPCASE(x)   (islower((unsigned char)x) ? toupper((unsigned char)x) : (x)) /**< Returns 'x' uppercased */
#endif
#endif                          /* CASE_H */
