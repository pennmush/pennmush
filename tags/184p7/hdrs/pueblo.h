/**
 * \file pueblo.h
 *
 * \brief Macros for dealing with Pueblo tags in strings
 *
 * Ok. The original idea for this came from seeing the Tiny patch for Pueblo.
 * A few months later I felt the urge to add some pueblo thingies to Penn,
 * and did so, though at a quite different level. This led to the
 * discovery of the trouble with bsd.c, which also got partly rewritten.
 */

#ifndef __PUEBLO_H
#define __PUEBLO_H


#include "conf.h"

#define PUEBLOBUFF \
       char pbuff[BUFFER_LEN]; \
       char *pp
#define PUSE \
       pp=pbuff
#define PEND \
       *pp=0;

#define tag_wrap(a,b,c) safe_tag_wrap(a,b,c,pbuff,&pp,NOTHING)
#define tag(a) safe_tag(a,pbuff,&pp)
#define tag_cancel(a) safe_tag_cancel(a,pbuff,&pp)

/* Please STAY SANE when modifying.
 * Making this something like 'x' and 'y' is a BAD IDEA
 */

#endif
