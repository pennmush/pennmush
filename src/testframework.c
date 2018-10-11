/** \file testframework.c
 *
 * \brief Hardcode test framework
 */

#include <stdio.h>
#include <string.h>

#include "sqlite3.h"
#include "tests.h"

#include "tests.inc"

/** Run the hardcode tests.
 * \return true if all tests passed, false if tests failed.
 */
bool
run_tests(void)
{
  int total_success = 0, total_failure = 0, skipped = 0, total_tests = 0;
  struct test_record *t;
  sqlite3_str *logmsg;
  char *logstr;
  do_rawlog(LT_TRACE, "Starting tests.");
  for (t = tests; t->name; t += 1) {
    total_tests += 1;
    if (t->status == TEST_NOT_RUN) {
      int success = 0, failure = 0;
      t->fun(&success, &failure);
      if (failure == 0) {
        t->status = TEST_PASSED;
      } else {
        struct test_record *deps;
        char *testname = sqlite3_mprintf("|%s|", t->name);
        for (deps = t + 1; deps->name; deps += 1) {
          if (strstr(deps->depends, testname)) {
            deps->status = TEST_SKIPME;
          }
        }
        sqlite3_free(testname);
        t->status = TEST_FAILED;
      }
      logmsg = sqlite3_str_new(NULL);
      sqlite3_str_appendf(logmsg, "%s %s: %d/%d tests succeeded", t->name,
                          t->status == TEST_PASSED ? "PASSED" : "FAILED",
                          success, success + failure);
      if (failure) {
        sqlite3_str_appendf(logmsg, ", %d failed", failure);
      }
      logstr = sqlite3_str_finish(logmsg);
      do_rawlog(LT_TRACE, "%s.", logstr);
      sqlite3_free(logstr);
      total_success += success;
      total_failure += failure;
    } else if (t->status == TEST_SKIPME) {
      do_rawlog(LT_TRACE, "%s SKIPPED", t->name);
      skipped += 1;
    }
  }
  logmsg = sqlite3_str_new(NULL);
  sqlite3_str_appendf(logmsg, "%d test groups, with %d/%d tests succeeding",
                      total_tests, total_success,
                      total_success + total_failure);
  if (skipped) {
    sqlite3_str_appendf(logmsg, ", %d test groups skipped", skipped);
  }
  if (total_failure) {
    sqlite3_str_appendf(logmsg, ", and %d tests failed", total_failure);
  }
  logstr = sqlite3_str_finish(logmsg);
  do_rawlog(LT_TRACE, "%s.", logstr);
  sqlite3_free(logstr);
  return total_failure == 0;
}
