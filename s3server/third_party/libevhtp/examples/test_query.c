#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "evhtp.h"

struct test {
    const char * raw_query;
    int          exp_error;
    struct expected {
        char * key;
        char * val;
    } exp[10]; /* avoid flexible array member: limit expectations per raw_query */
};

static int
test_cmp(evhtp_query_t * query, evhtp_kv_t * kvobj, const char * valstr, struct expected * exp) {
    if (!query || !kvobj) {
        return -1;
    }

    if (exp->val == NULL) {
        if (kvobj->val || valstr) {
            return -1;
        }

        return 0;
    }

    if (strcmp(kvobj->val, exp->val)) {
        printf("\n");
        printf("    expected: '%s'\n", exp->val);
        printf("    actual:   '%s'\n", kvobj->val);
        return -1;
    }

    if (strcmp(valstr, exp->val)) {
        return -1;
    }

    return 0;
}

/* evhtp_kvs_iterator */
int
kvs_print(evhtp_kv_t * kvobj, void * arg) {
    int * key_idx = arg;

    if (*key_idx) {
        printf(", ");
    }

    printf("\"%s\": %s%s%s", kvobj->key,
           kvobj->val ? "\"" : "",
           kvobj->val,
           kvobj->val ? "\"" : "");

    *key_idx += 1;

    return 0;
}

static int
query_test(const char * raw_query, int exp_error, struct expected exp[], int flags) {
    evhtp_query_t   * query;
    struct expected * check;
    int               key_idx    = 0;
    int               idx        = 0;
    int               num_errors = 0;

    /* print whether error is expected or not */
    printf("%-7s  ", exp_error ? "(error)" : "");
    query = evhtp_parse_query_wflags(raw_query, strlen(raw_query), flags);
    if (!query) {
        printf("<error>");
        return exp_error == 0;
    }

    printf("{");
    evhtp_kvs_for_each(query, kvs_print, &key_idx);
    /* TODO check for keys in query but not in exp */
    printf("}");

    while (1) {
        evhtp_kv_t * kvobj  = NULL;
        const char * valstr = NULL;

        check = &exp[idx++];

        if (check == NULL || check->key == NULL) {
            break;
        }

        kvobj  = evhtp_kvs_find_kv(query, check->key);
        valstr = evhtp_kv_find(query, check->key);

        if (test_cmp(query, kvobj, valstr, check) == -1) {
            num_errors += 1;
            printf("         ");
        }
    }

    if (exp_error) {
        return -1;
    }

    return num_errors;
} /* query_test */

struct test base_tests[] = {
    { "a=b;key&c=val",           0, {
                { "a",                       "b;key" },
                { "c",                       "val" },
                { NULL,                      NULL },
            } },
    { "a=b;key=val",             0, {
                { "a",                       "b;key=val" },
                { NULL,                      NULL },
            } },
    { "a;b=val",                 0, {
                { "a;b",                     "val" },
                { NULL,                      NULL },
            } },
    { "end_empty_string=",       1 },
    { "end_null",                1 },
    { "hexa=some%20&hexb=bla%0", 0, {
                { "hexa",                    "some%20" },
                { "hexb",                    "bla%0" },
                { NULL,                      NULL },
            } },
    { "hexa=some%20;hexb=bla",   0, {
                { "hexa",                    "some%20;hexb=bla" },
                { NULL,                      NULL },
            } },
    { "hexa%z=some",             0, {
                { "hexa%z",                  "some" },
                { NULL,                      NULL },
            } },
    { "aaa=some\%az",            1 },
};

struct test ignore_hex_tests[] = {
    { "hexa=some%20&hexb=bla%0&hexc=%", 0, {
                { "hexa",                           "some%20" },
                { "hexb",                           "bla%0" },
                { "hexc",                           "%" },
                { NULL,                             NULL },
            } },
    { "hexa%z=some",                    0, {
                { "hexa%z",                         "some" },
                { NULL,                             NULL },
            } },
    { "aaa=some%zz",                    0, {
                { "aaa",                            "some%zz" },
                { NULL,                             NULL },
            } },
};

struct test allow_empty_tests[] = {
    { "end_empty_string=", 0, {
                { "end_empty_string",  "" },
                { NULL,                NULL },
            } },
};

struct test allow_null_tests[] = {
    { "end_null", 0, {
                { "end_null", NULL },
                { NULL,       NULL },
            } },
};

struct test treat_semicolon_as_sep_tests[] = {
    { "a=b;key=val", 0, {
                { "a",           "b" },
                { "key",         "val" },
                { NULL,          NULL },
            } },
    { "a;b=val",     1 },
};

struct test lenient_tests[] = {
    { "a=b;key&c=val",                    0, {
                { "a",                                "b" },
                { "key",                              NULL },
                { "c",                                "val" },
                { NULL,                               NULL },
            } },
    { "a=b;key=val",                      0, {
                { "a",                                "b" },
                { "key",                              "val" },
                { NULL,                               NULL },
            } },
    { "end_empty_string=",                0, {
                { "end_empty_string",                 "" },
                { NULL,                               NULL },
            } },
    { "end_null",                         0, {
                { "end_null",                         NULL },
                { NULL,                               NULL },
            } },
    { "hexa=some\%a;hexb=bl%0&hexc=\%az", 0, {
                { "hexa",                             "some\%a" },
                { "hexb",                             "bl%0" },
                { "hexc",                             "\%az" },
                { NULL,                               NULL },
            } },
};

static void
test(const char * raw_query, int exp_error, struct expected exp[], int flags) {
    printf("         %-30s  ", raw_query);
    printf("\r  %s\n", query_test(raw_query, exp_error, exp, flags) ? "ERROR" : "OK");
}

#define ARRAY_SIZE(arr)                    (sizeof(arr) / sizeof((arr)[0]))

int
main(int argc, char ** argv) {
    int i;

    #define PARSE_QUERY_TEST(tests, flags) do {                                          \
            printf("- " # tests "\n");                                                   \
            for (i = 0; i < ARRAY_SIZE(tests); i++) {                                    \
                test((tests)[i].raw_query, (tests)[i].exp_error, (tests)[i].exp, flags); \
            }                                                                            \
    } while (0)

    PARSE_QUERY_TEST(base_tests, EVHTP_PARSE_QUERY_FLAG_STRICT);
    PARSE_QUERY_TEST(ignore_hex_tests, EVHTP_PARSE_QUERY_FLAG_IGNORE_HEX);
    PARSE_QUERY_TEST(allow_empty_tests, EVHTP_PARSE_QUERY_FLAG_ALLOW_EMPTY_VALS);
    PARSE_QUERY_TEST(allow_null_tests, EVHTP_PARSE_QUERY_FLAG_ALLOW_NULL_VALS);
    PARSE_QUERY_TEST(treat_semicolon_as_sep_tests, EVHTP_PARSE_QUERY_FLAG_TREAT_SEMICOLON_AS_SEP);
    PARSE_QUERY_TEST(lenient_tests, EVHTP_PARSE_QUERY_FLAG_LENIENT);

    return 0;
}

