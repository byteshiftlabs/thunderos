/*
 * KUnit-inspired testing framework for ThunderOS
 * Simplified version that works in bare-metal RISC-V
 */

#ifndef KUNIT_H
#define KUNIT_H

#ifndef NULL
#define NULL ((void *)0)
#endif

// Test status
enum test_status {
    TEST_SUCCESS = 0,
    TEST_FAILURE = 1,
};

// Test structure
struct kunit_test {
    const char *name;
    void (*run)(struct kunit_test *test);
    enum test_status status;
    const char *failure_msg;
    int line;
};

// Test case definition macro (like KUnit)
#define KUNIT_CASE(test_name) { #test_name, test_name, TEST_SUCCESS, NULL, 0 }

// Assertion macros (like KUnit)
#define KUNIT_EXPECT_EQ(test, left, right) \
    do { \
        if ((left) != (right)) { \
            (test)->status = TEST_FAILURE; \
            (test)->failure_msg = #left " != " #right; \
            (test)->line = __LINE__; \
            return; \
        } \
    } while (0)

#define KUNIT_EXPECT_NE(test, left, right) \
    do { \
        if ((left) == (right)) { \
            (test)->status = TEST_FAILURE; \
            (test)->failure_msg = #left " == " #right; \
            (test)->line = __LINE__; \
            return; \
        } \
    } while (0)

#define KUNIT_EXPECT_TRUE(test, condition) \
    do { \
        if (!(condition)) { \
            (test)->status = TEST_FAILURE; \
            (test)->failure_msg = #condition " is false"; \
            (test)->line = __LINE__; \
            return; \
        } \
    } while (0)

#define KUNIT_EXPECT_FALSE(test, condition) \
    do { \
        if (condition) { \
            (test)->status = TEST_FAILURE; \
            (test)->failure_msg = #condition " is true"; \
            (test)->line = __LINE__; \
            return; \
        } \
    } while (0)

#define KUNIT_EXPECT_NULL(test, ptr) \
    KUNIT_EXPECT_EQ(test, ptr, NULL)

#define KUNIT_EXPECT_NOT_NULL(test, ptr) \
    KUNIT_EXPECT_NE(test, ptr, NULL)

// Test suite runner
int kunit_run_tests(struct kunit_test *test_cases, int num_tests);

#endif // KUNIT_H
