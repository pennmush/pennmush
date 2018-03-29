/**
 * \file mypcre.h
 * \brief Wrapper for pcre.h
 */
#ifndef _MYPCRE_H
#define _MYPCRE_H

#define PENN_MATCH_LIMIT 100000

#include <pcre.h>

#ifndef PCRE_STUDY_JIT_COMPILE
#define PCRE_STUDY_JIT_COMPILE 0
#endif

void set_match_limit(pcre_extra *);
pcre_extra *default_match_limit(void);

extern int pcre_study_flags;
extern int pcre_public_study_flags;

#endif /* End of pcre.h */
