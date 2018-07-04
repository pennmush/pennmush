/** \file myutf8.h
 *
 * \brief Basic UTF-8 handling.
 */

#pragma once
#ifdef HAVE_ICU
#include <unicode/utypes.h>
#include <unicode/utf8.h>
#else
/* Use a stripped down copy of ICU 61.1 headers. */
#define U_NO_DEFAULT_INCLUDE_UTF_HEADERS 1
#include "punicode/utypes.h"
#include "punicode/utf8.h"
#endif
