#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG_MODE  // TEMP

#ifdef DEBUG_MODE
#include <stdio.h>
#include <assert.h>
#define DB_LOG_ERROR(message) printf("DB error in [%s, %i]: %s", __FILE__, __LINE__, message)
#define Assert(expression)    assert(expression)
#else
#define DB_LOG_ERROR(message)
#define Assert(expression)
#endif

#define AssertInvalidPath() Assert(!"InvalidPath")

#endif
