/* Auto-generated file. DO NOT EDIT */
void test_is_integer(int *, int *);
void test_is_boolean(int *, int *);
void test_is_dbref(int *, int *);
void test_is_number(int *, int *);
void test_is_uinteger(int *, int *);
struct test_record {
    const char *name;
    void (*fun)(int *, int *);
    const char *depends;
    int status;
};
#define TEST_NOT_RUN 0
#define TEST_PASSED 1
#define TEST_FAILED 2
#define TEST_SKIPME 3

static struct test_record tests[] = {
{"is_integer", test_is_integer, "||", TEST_NOT_RUN},
{"is_boolean", test_is_boolean, "|is_integer|", TEST_NOT_RUN},
{"is_dbref", test_is_dbref, "||", TEST_NOT_RUN},
{"is_number", test_is_number, "||", TEST_NOT_RUN},
{"is_uinteger", test_is_uinteger, "||", TEST_NOT_RUN},
{NULL, NULL, NULL, TEST_NOT_RUN}
};
