#ifndef __COMPILE_H
#define __COMPILE_H

/* Compiler-specific stuff. */

#ifndef __GNUC_PREREQ
#if defined __GNUC__ && defined __GNUC_MINOR__
#define __GNUC_PREREQ(maj, min) \
        ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
#define __GNUC_PREREQ(maj, min) 0
#endif
#endif

/* For modern gcc , this attribute lets the compiler know that the
 * function returns a newly allocated value, for pointer aliasing
 * optimizations.
 */
#if !defined(__attribute_malloc__) && defined(GCC_MALLOC_CALL)
#define __attribute_malloc__ GCC_MALLOC_CALL
#elif !defined(__attribute_malloc__)
#define __attribute_malloc__
#endif

/* If a compiler knows a function will never return, it can generate
   slightly better code for calls to it. */
#if defined(WIN32) && _MSC_VER >= 1200
#define NORETURN __declspec(noreturn)
#elif defined(HAVE___ATTRIBUTE__)
#define NORETURN __attribute__ ((__noreturn__))
#else
#define NORETURN
#endif

/* Enable Win32 services support */
#ifdef WIN32
#define WIN32SERVICES
#endif

/* Disable Win32 services support due to it failing to run properly
   when compiling with MinGW32. Eventually I would like to correct
   the issue. - EEH */
#ifdef __MINGW32__
#undef WIN32SERVICES
#endif

/* --------------- Stuff for Win32 services ------------------ */
/*
   When "exit" is called to handle an error condition, we really want to
   terminate the game thread, not the whole process.
   MS VS.NET (_MSC_VER >= 1200) requires the weird noreturn stuff.
 */

#ifdef WIN32SERVICES
/*
   Force stdlib.h to be included first so we don't mangle a system header.
 */
#include <stdlib.h>
#define exit(arg) Win32_Exit (arg)
void NORETURN WIN32_CDECL Win32_Exit(int exit_code);
#endif

#endif                          /* __COMPILE_H */
