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

/*
 * (which isn't exportable from the U.S.), then don't encrypt
 */
#ifndef HAS_CRYPT
#define crypt(s,t) (s)
#endif

/* You better get with the 90's if this isn't true! */
#define HAS_IEEE_MATH

#ifndef HAVE_SIGCHLD
#define SIGCHLD	SIGCLD
#elif !defined(HAVE_SIGCLD)
#define SIGCLD	SIGCHLD
#endif

#ifndef HAVE_STRCOLL
#undef strcoll
#define strcoll strcmp
#endif

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

#if !defined(HAS_VSNPRINTF)
#if defined(HAVE__VSNPRINTF_S)
#define vsnprintf(str, size, fmt, args)  _vsnprintf_s((str), (size), \
						      _TRUNCATE,     \
						      (fmt), (args))
#elif defined(HAVE__VSNPRINTF)
#define vsnprintf _vsnprintf
#endif
#endif

#if defined(HAVE_POLLTS) && !defined(HAVE_PPOLL)
/* Linux's ppoll() is identical to NetBSD's pollts() in all but name. */
#define ppoll pollts
#define HAVE_PPOLL
#endif

#endif
