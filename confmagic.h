/*
 * This file was produced by running metaconfig and is intended to be included
 * after config.h and after all the other needed includes have been dealt with.
 *
 * This file may be empty, and should not be edited. Rerun metaconfig instead.
 * If you wish to get rid of this magic, remove this file and rerun metaconfig
 * without the -M option.
 *
 *  $Id: Magic_h.U,v 3.0.1.2 1993/11/10 17:32:58 ram Exp $
 */

#ifndef _confmagic_h_
#define _confmagic_h_

#ifndef HAVE_SNPRINTF
#ifdef HAVE__VSNPRINTF_S
#define snprintf sane_snprintf_s
/* Win32 needs size_t to be defined... */
#include <crtdefs.h>
int sane_snprintf_s(char *, size_t, const char *, ...);
#elif defined(HAVE__SNPRINTF)
#define snprintf _snprintf
#endif
#endif

#if !defined(HAVE_VSNPRINTF)
#if defined(HAVE__VSNPRINTF_S)
#define vsnprintf(str, size, fmt, args)  _vsnprintf_s((str), (size), \
						      _TRUNCATE,     \
						      (fmt), (args))
#elif defined(HAVE__VSNPRINTF)
#define vsnprintf _vsnprintf
#endif
#endif

#endif
