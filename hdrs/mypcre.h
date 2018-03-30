/**
 * \file mypcre.h
 * \brief Wrapper for pcre.h
 */
#ifndef _MYPCRE_H
#define _MYPCRE_H

#define PENN_MATCH_LIMIT 100000

#ifdef HAVE_PCRE

#ifdef HAVE_PCRE_H
#include <pcre.h>

#ifndef PCRE_STUDY_JIT_COMPILE
#define PCRE_STUDY_JIT_COMPILE 0
#endif

void set_match_limit(pcre_extra *);
pcre_extra *default_match_limit(void);

extern int pcre_study_flags;
extern int pcre_public_study_flags;

#else
#error "You appear to have a system PCRE library but not the pcre.h header."
#endif

#endif /* !HAVE_PCRE */
#endif /* End of pcre.h */
