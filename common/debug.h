#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG_MODE  // TEMP

#ifdef DEBUG_MODE
#include <stdio.h>
#include <assert.h>
#define DB_LOG_ERROR(...)                                 \
    printf("DB ERROR at [%s, %i]: ", __FILE__, __LINE__), \
        printf(__VA_ARGS__),                              \
        printf("\n")

#define DB_LOG_INFO(...)                                 \
    printf("DB info at [%s, %i]: ", __FILE__, __LINE__), \
        printf(__VA_ARGS__),                             \
        printf("\n")

#define Assert(expression) assert(expression)
#else
#define DB_LOG_ERROR(...)
#define Assert(expression)
#endif

#define AssertInvalidPath() Assert(!"InvalidPath")

#endif
