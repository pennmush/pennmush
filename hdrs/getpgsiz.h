#ifndef __GETPGSIZ_H
#define __GETPGSIZ_H

#ifndef HAVE_GETPAGESIZE

#ifdef HAVE_SYSCONF
#define getpagesize() sysconf(_SC_PAGESIZE)
#elif defined(WIN32)
unsigned int getpagesize_win32(void);
#define getpagesize() getpagesize_win32()
#else
/* Guess. */
#define getpagesize() 4096
#endif

#endif                          /* !HAVE _GETPAGESIZE */

#endif                          /* __GETPGSIZ_H */
