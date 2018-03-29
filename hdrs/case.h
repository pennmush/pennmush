/**
 * \file case.h
 *
 * \brief Routines for upper/lower casing characters
 */

#ifndef CASE_H
#define CASE_H

#include <ctype.h>

#ifdef HAVE_SAFE_TOUPPER
#define DOWNCASE(x) tolower(x) /**< Returns 'x' lowercased */
#define UPCASE(x) toupper(x)   /**< Returns 'x' uppercased */
#else
#define DOWNCASE(x)                                                            \
  (isupper(x) ? tolower(x) : (x)) /**< Returns 'x' lowercased */
#define UPCASE(x)                                                              \
  (islower(x) ? toupper(x) : (x)) /**< Returns 'x' uppercased                  \
                                   */
#endif
#endif /* CASE_H */
