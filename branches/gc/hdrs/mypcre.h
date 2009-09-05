#ifndef _MYPCRE_H
#define _MYPCRE_H

/*************************************************
*       Perl-Compatible Regular Expressions      *
*************************************************/

/* In its original form, this is the .in file that is transformed by
"configure" into pcre.h.

           Copyright (c) 1997-2005 University of Cambridge

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/

/* Modified a bit by Shawn Wagner for inclusion in PennMUSH. See
   pcre.c for details. */


#define PENN_MATCH_LIMIT 100000
struct pcre_extra;
void set_match_limit(struct pcre_extra *);
struct pcre_extra *default_match_limit(void);

#ifdef HAVE_PCRE
#include <pcre.h>
#else


#define PCRE_MAJOR          6
#define PCRE_MINOR          4
#define PCRE_DATE           05-Sep-2005

#ifndef PCRE_DATA_SCOPE
#  define PCRE_DATA_SCOPE     extern
#endif

/* Have to include stdlib.h in order to ensure that size_t is defined;
it is needed here for malloc. */

#include <stdlib.h>

/* Allow for C++ users */

#ifdef __cplusplus
extern "C" {
#endif

/* Options */

#define PCRE_CASELESS           0x00000001
#define PCRE_MULTILINE          0x00000002
#define PCRE_DOTALL             0x00000004
#define PCRE_EXTENDED           0x00000008
#define PCRE_ANCHORED           0x00000010
#define PCRE_DOLLAR_ENDONLY     0x00000020
#define PCRE_EXTRA              0x00000040
#define PCRE_NOTBOL             0x00000080
#define PCRE_NOTEOL             0x00000100
#define PCRE_UNGREEDY           0x00000200
#define PCRE_NOTEMPTY           0x00000400
#define PCRE_UTF8               0x00000800
#define PCRE_NO_AUTO_CAPTURE    0x00001000
#define PCRE_NO_UTF8_CHECK      0x00002000
#define PCRE_AUTO_CALLOUT       0x00004000
#define PCRE_PARTIAL            0x00008000
#define PCRE_DFA_SHORTEST       0x00010000
#define PCRE_DFA_RESTART        0x00020000
#define PCRE_FIRSTLINE          0x00040000


/* Exec-time and get/set-time error codes */

#define PCRE_ERROR_NOMATCH         (-1)
#define PCRE_ERROR_NULL            (-2)
#define PCRE_ERROR_BADOPTION       (-3)
#define PCRE_ERROR_BADMAGIC        (-4)
#define PCRE_ERROR_UNKNOWN_NODE    (-5)
#define PCRE_ERROR_NOMEMORY        (-6)
#define PCRE_ERROR_NOSUBSTRING     (-7)
#define PCRE_ERROR_MATCHLIMIT      (-8)
#define PCRE_ERROR_CALLOUT         (-9) /* Never used by PCRE itself */
#define PCRE_ERROR_BADUTF8        (-10)
#define PCRE_ERROR_BADUTF8_OFFSET (-11)
#define PCRE_ERROR_PARTIAL        (-12)
#define PCRE_ERROR_BADPARTIAL     (-13)
#define PCRE_ERROR_INTERNAL       (-14)
#define PCRE_ERROR_BADCOUNT       (-15)
#define PCRE_ERROR_DFA_UITEM      (-16)
#define PCRE_ERROR_DFA_UCOND      (-17)
#define PCRE_ERROR_DFA_UMLIMIT    (-18)
#define PCRE_ERROR_DFA_WSSIZE     (-19)
#define PCRE_ERROR_DFA_RECURSE    (-20)

/* Request types for pcre_fullinfo() */

#define PCRE_INFO_OPTIONS            0
#define PCRE_INFO_SIZE               1
#define PCRE_INFO_CAPTURECOUNT       2
#define PCRE_INFO_BACKREFMAX         3
#define PCRE_INFO_FIRSTBYTE          4
#define PCRE_INFO_FIRSTCHAR          4  /* For backwards compatibility */
#define PCRE_INFO_FIRSTTABLE         5
#define PCRE_INFO_LASTLITERAL        6
#define PCRE_INFO_NAMEENTRYSIZE      7
#define PCRE_INFO_NAMECOUNT          8
#define PCRE_INFO_NAMETABLE          9
#define PCRE_INFO_STUDYSIZE         10
#define PCRE_INFO_DEFAULT_TABLES    11

/* Request types for pcre_config() */

#define PCRE_CONFIG_UTF8                    0
#define PCRE_CONFIG_NEWLINE                 1
#define PCRE_CONFIG_LINK_SIZE               2
#define PCRE_CONFIG_POSIX_MALLOC_THRESHOLD  3
#define PCRE_CONFIG_MATCH_LIMIT             4

/* Bit flags for the pcre_extra structure */

#define PCRE_EXTRA_STUDY_DATA          0x0001
#define PCRE_EXTRA_MATCH_LIMIT         0x0002
#define PCRE_EXTRA_CALLOUT_DATA        0x0004
#define PCRE_EXTRA_TABLES              0x0008

/* Types */

  struct real_pcre;             /* declaration; the definition is private  */
  typedef struct real_pcre pcre;

/* The structure for passing additional data to pcre_exec(). This is defined in
such as way as to be extensible. */

  typedef struct pcre_extra {
    unsigned long int flags;    /* Bits for which fields are set */
    void *study_data;           /* Opaque data from pcre_study() */
    unsigned long int match_limit;      /* Maximum number of calls to match() */
    void *callout_data;         /* Data passed back in callouts */
    const unsigned char *tables;        /* Pointer to character tables */
  } pcre_extra;

/* The structure for passing out data via the pcre_callout_function. We use a
structure so that new fields can be added on the end in future versions,
without changing the API of the function, thereby allowing old clients to work
without modification. */

  typedef struct pcre_callout_block {
    int version;                /* Identifies version of block */
    /* ------------------------ Version 0 ------------------------------- */
    int callout_number;         /* Number compiled into pattern */
    int *offset_vector;         /* The offset vector */
    const char *subject;        /* The subject being matched */
    int subject_length;         /* The length of the subject */
    int start_match;            /* Offset to start of this match attempt */
    int current_position;       /* Where we currently are in the subject */
    int capture_top;            /* Max current capture */
    int capture_last;           /* Most recently closed capture */
    void *callout_data;         /* Data passed in with the call */
    /* ------------------- Added for Version 1 -------------------------- */
    int pattern_position;       /* Offset to next item in the pattern */
    int next_item_length;       /* Length of next item in the pattern */
    /* ------------------------------------------------------------------ */
  } pcre_callout_block;


/* Exported PCRE functions */

  extern pcre *pcre_compile(const char *, int, const char **,
                            int *, const unsigned char *);
  extern int pcre_copy_substring(const char *, int *, int, int, char *, int);
  int pcre_get_substring(const char *, int *, int, int, const char **);
  extern int pcre_exec(const pcre *, const pcre_extra *,
                       const char *, int, int, int, int *, int);
  extern const unsigned char *pcre_maketables(void);
  extern pcre_extra *pcre_study(const pcre *, int, const char **);
  extern int pcre_fullinfo(const pcre * argument_re,
                           const pcre_extra * extra_data, int what,
                           void *where);
  extern int pcre_get_stringnumber(const pcre * code, const char *stringname);
  extern int
   pcre_copy_named_substring(const pcre * code, const char *subject,
                             int *ovector, int stringcount,
                             const char *stringname, char *buffer, int size);

#ifdef __cplusplus
}                               /* extern "C" */
#endif
#endif                          /* !HAVE_PCRE */
#endif                          /* End of pcre.h */
