/* Auto-generated file. DO NOT EDIT */
void test_switch_find(int *, int *);
void test_split_token(int *, int *);
void test_toupper(int *, int *);
void test_tolower(int *, int *);
void test_gcbytes(int *, int *);
void test_split_token_cp(int *, int *);
void test_is_integer(int *, int *);
void test_next_token(int *, int *);
void test_next_token_cp(int *, int *);
void test_switchmask(int *, int *);
void test_uupcasestr(int *, int *);
void test_udowncasestr(int *, int *);
void test_strnlen_gc(int *, int *);
void test_strlen_gc(int *, int *);
void test_remove_word(int *, int *);
void test_remove_uword(int *, int *);
void test_is_boolean(int *, int *);
void test_gc_breaks(int *, int *);
void test_do_wordcount(int *, int *);
void test_do_uwordcount(int *, int *);
void test_SW_BY_NAME(int *, int *);
void test_chopstr(int *, int *);
void test_copy_up_to(int *, int *);
void test_escape_like(int *, int *);
void test_glob_to_like(int *, int *);
void test_is_dbref(int *, int *);
void test_is_number(int *, int *);
void test_is_uinteger(int *, int *);
void test_isalnum(int *, int *);
void test_isalpha(int *, int *);
void test_isdigit(int *, int *);
void test_islower(int *, int *);
void test_isprint(int *, int *);
void test_ispunct(int *, int *);
void test_isspace(int *, int *);
void test_isupper(int *, int *);
void test_latin1_to_utf8(int *, int *);
void test_map_file(int *, int *);
void test_next_in_list(int *, int *);
void test_remove_trailing_whitespace(int *, int *);
void test_sanitize_utf8(int *, int *);
void test_seek_char(int *, int *);
void test_seek_cp(int *, int *);
void test_skip_space(int *, int *);
void test_strccat(int *, int *);
void test_strchr_unescaped(int *, int *);
void test_string_prefix(int *, int *);
void test_string_prefixe(int *, int *);
void test_strlen_cp(int *, int *);
void test_trim_space_sep(int *, int *);
void test_ustrlower_a(int *, int *);
void test_ustrupper_a(int *, int *);
void test_utf8_to_latin1(int *, int *);
void test_utf8_to_latin1_us(int *, int *);
void test_valid_utf8(int *, int *);
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
{"switch_find", test_switch_find, "||", TEST_NOT_RUN},
{"split_token", test_split_token, "||", TEST_NOT_RUN},
{"toupper", test_toupper, "||", TEST_NOT_RUN},
{"tolower", test_tolower, "||", TEST_NOT_RUN},
{"gcbytes", test_gcbytes, "||", TEST_NOT_RUN},
{"split_token_cp", test_split_token_cp, "||", TEST_NOT_RUN},
{"is_integer", test_is_integer, "||", TEST_NOT_RUN},
{"next_token", test_next_token, "||", TEST_NOT_RUN},
{"next_token_cp", test_next_token_cp, "||", TEST_NOT_RUN},
{"switchmask", test_switchmask, "|switch_find|split_token|", TEST_NOT_RUN},
{"uupcasestr", test_uupcasestr, "|toupper|", TEST_NOT_RUN},
{"udowncasestr", test_udowncasestr, "|tolower|", TEST_NOT_RUN},
{"strnlen_gc", test_strnlen_gc, "|gcbytes|", TEST_NOT_RUN},
{"strlen_gc", test_strlen_gc, "|gcbytes|", TEST_NOT_RUN},
{"remove_word", test_remove_word, "|split_token|", TEST_NOT_RUN},
{"remove_uword", test_remove_uword, "|split_token_cp|", TEST_NOT_RUN},
{"is_boolean", test_is_boolean, "|is_integer|", TEST_NOT_RUN},
{"gc_breaks", test_gc_breaks, "|gcbytes|", TEST_NOT_RUN},
{"do_wordcount", test_do_wordcount, "|next_token|", TEST_NOT_RUN},
{"do_uwordcount", test_do_uwordcount, "|next_token_cp|", TEST_NOT_RUN},
{"SW_BY_NAME", test_SW_BY_NAME, "|switch_find|switchmask|", TEST_NOT_RUN},
{"chopstr", test_chopstr, "||", TEST_NOT_RUN},
{"copy_up_to", test_copy_up_to, "||", TEST_NOT_RUN},
{"escape_like", test_escape_like, "||", TEST_NOT_RUN},
{"glob_to_like", test_glob_to_like, "||", TEST_NOT_RUN},
{"is_dbref", test_is_dbref, "||", TEST_NOT_RUN},
{"is_number", test_is_number, "||", TEST_NOT_RUN},
{"is_uinteger", test_is_uinteger, "||", TEST_NOT_RUN},
{"isalnum", test_isalnum, "||", TEST_NOT_RUN},
{"isalpha", test_isalpha, "||", TEST_NOT_RUN},
{"isdigit", test_isdigit, "||", TEST_NOT_RUN},
{"islower", test_islower, "||", TEST_NOT_RUN},
{"isprint", test_isprint, "||", TEST_NOT_RUN},
{"ispunct", test_ispunct, "||", TEST_NOT_RUN},
{"isspace", test_isspace, "||", TEST_NOT_RUN},
{"isupper", test_isupper, "||", TEST_NOT_RUN},
{"latin1_to_utf8", test_latin1_to_utf8, "||", TEST_NOT_RUN},
{"map_file", test_map_file, "||", TEST_NOT_RUN},
{"next_in_list", test_next_in_list, "||", TEST_NOT_RUN},
{"remove_trailing_whitespace", test_remove_trailing_whitespace, "||", TEST_NOT_RUN},
{"sanitize_utf8", test_sanitize_utf8, "||", TEST_NOT_RUN},
{"seek_char", test_seek_char, "||", TEST_NOT_RUN},
{"seek_cp", test_seek_cp, "||", TEST_NOT_RUN},
{"skip_space", test_skip_space, "||", TEST_NOT_RUN},
{"strccat", test_strccat, "||", TEST_NOT_RUN},
{"strchr_unescaped", test_strchr_unescaped, "||", TEST_NOT_RUN},
{"string_prefix", test_string_prefix, "||", TEST_NOT_RUN},
{"string_prefixe", test_string_prefixe, "||", TEST_NOT_RUN},
{"strlen_cp", test_strlen_cp, "||", TEST_NOT_RUN},
{"trim_space_sep", test_trim_space_sep, "||", TEST_NOT_RUN},
{"ustrlower_a", test_ustrlower_a, "||", TEST_NOT_RUN},
{"ustrupper_a", test_ustrupper_a, "||", TEST_NOT_RUN},
{"utf8_to_latin1", test_utf8_to_latin1, "||", TEST_NOT_RUN},
{"utf8_to_latin1_us", test_utf8_to_latin1_us, "||", TEST_NOT_RUN},
{"valid_utf8", test_valid_utf8, "||", TEST_NOT_RUN},
{NULL, NULL, NULL, TEST_NOT_RUN}
};
