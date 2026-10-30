#pragma once

/* --------------------------------------------------------------------------------------------------------------- */
/* Test API                                                                                                        */
/* --------------------------------------------------------------------------------------------------------------- */

#define TEST(suite, name) \
    static void test_##suite##_##name(void); \
    static Test_Definition test_entry_##suite##_##name = { \
        #suite, \
        #name, \
        test_##suite##_##name, \
        0 \
    }; \
    static void __attribute__((constructor)) \
    register_##suite##_##name(void) { \
        register_test_definition(&test_entry_##suite##_##name); \
    } \
    static void test_##suite##_##name(void)


/* --------------------------------------------------------------------------------------------------------------- */
/* Test Internals                                                                                                  */
/* --------------------------------------------------------------------------------------------------------------- */

typedef void(*Test_Function)(void);

typedef struct Test_Definition {
    const char *suite;
    const char *name;
    Test_Function function;
    struct Test_Definition *next;
} Test_Definition;

extern Test_Definition *first_test_definition;

void register_test_definition(Test_Definition *definition);


