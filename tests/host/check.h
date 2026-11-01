#pragma once

// Minimal test helper: CHECK() records a failure and goes on, tests_result() is the exit code of main().

#include <stdio.h>

static int checks_failed;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            checks_failed++;                                                  \
        }                                                                     \
    } while (0)

static inline int tests_result(void)
{
    if (checks_failed) {
        printf("%d check(s) failed\n", checks_failed);
        return 1;
    }

    return 0;
}
