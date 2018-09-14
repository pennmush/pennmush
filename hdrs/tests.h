/** \file tests.h
 * 
 * \brief Headers to support hardcode test framework.
 */
#pragma once

#include "log.h"

#define TEST_GROUP(name) void test_##name (int *success, int *failure)

#define TEST(name, t) \
    do { \
        if (t) { \
            *success += 1; \
        } else { \
            *failure += 1; \
            do_rawlog(LT_TRACE, "%s:%d: Test %s failed!", __FILE__, __LINE__, name); \
        } \
    } while (0)

bool run_tests(void);
