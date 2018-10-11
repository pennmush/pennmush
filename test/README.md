# Regression tests for Penn functions and commands.

Penn has two different test suites -- one for hardcode, one for softcode. This document describes how to run them and write tests for them.

# Hardcode Tests

These are run at startup, after configuration files are read and the database is loaded, but before the game starts accepting connections. It's intended for things that can't easily be tested in softcode and things softcode tests themselves depend on working right.

## Running tests

The `--tests` option to `netmush` runs the tests and logs the results to `log/trace.log`. The MUSH continues to finish starting up and running normally unless a test case fails, in which case it ends with exit code 1.

The `--only-tests` option also runs the test cases, but then exits instead of continuing to start up. An exit code of 0 means all tests passed, 1 means there were failures.

## Writing tests

All source files that define tests need to `#include "tests.h"`.

A *test group* is defined with the `TEST_GROUP()` macro like so:

    TEST_GROUP(some_name) {
        // test cases
    }

Each test group can have one or more tests. Tests for the same hardcode function should all go in a single test group. The tests look like:

    TEST("name", test expression);

`test expression` should evaluate to a true value to indicate success, and 0 or `false` for failure. Failures get logged.

If a *test group* should only be called after another *test group* has successfully run, add a comment like:

    // TEST some_name REQUIRES other_test1 other_test2

# Softcode Tests

## Running tests

From the test subdirectory:

    $ perl runtest.pl [--valgrind] testFOO.t ...

or     

    $ ./alltests.sh [--valgrind]

Note: The the hardcode tests are automatically run as well.

## Writing tests

The test*.t files in the **test** subdirectory define softcode test cases.

The `--valgrind` option runs the test game under valgrind to help detect memory issues.

Their format:

    login mortal
    expect N failures!
    run tests:
    test cases

All the lines above the 'run tests:' one are optional. 

The *test cases* are perl code:

The `test()` function has four arguments -- the name of the test, the player to run it as (Either `$god` or `$mortal`, a softcode command, and a regular expression that should match the expected result (Or an array ref of REs).

The player objects have a `command()` method that runs its argument in the game without counting as a test: `$mortal->command("think not a test");`

Some hints: $god is always available as a test connection. If 'login mortal' was given, $mortal is too. See existing tests for examples of how to write new ones.

