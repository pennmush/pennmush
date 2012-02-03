/** 
 * \file mypcre.h
 * \brief Wrapper for pcre.h
 */
#ifndef _MYPCRE_H
#define _MYPCRE_H

#define PENN_MATCH_LIMIT 100000
struct pcre_extra;
void set_match_limit(struct pcre_extra *);
struct pcre_extra *default_match_limit(void);

#ifdef HAVE_PCRE

#ifdef HAVE_PCRE_H
#include <pcre.h>
#else
#error "You appear to have a system PCRE library but not the pcre.h header."
#endif

#endif                          /* !HAVE_PCRE */
#endif                          /* End of pcre.h */
