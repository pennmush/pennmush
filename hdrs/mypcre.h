/**
 * \file mypcre.h
 * \brief Wrapper for pcre2.h and related functions
 */
#ifndef _MYPCRE_H
#define _MYPCRE_H

#define PENN_MATCH_LIMIT 100000

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8

#include "pcre2.h"

extern uint32_t re_compile_flags;
extern uint32_t re_match_flags;
extern pcre2_compile_context *re_compile_ctx;
extern pcre2_match_context *re_match_ctx;
extern pcre2_convert_context *glob_convert_ctx;

#endif /* End of mypcre.h */
