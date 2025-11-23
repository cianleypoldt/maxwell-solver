#ifndef C_UTILS
#define C_UTILS

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef double   f64;
typedef uint64_t u64;
typedef uint32_t u32;

typedef struct {
        void* buffer;
        u64   size;
} file;

file load_file(const char* path);
void free_file(file);

f64  len3(const f64* a);
f64  dot3(const f64* a, const f64* b);
void assign3(f64* res, const f64* v);
void add3(f64* res, const f64* a, const f64* b);
void sub3(f64* res, const f64* a, const f64* b);
void scale3(f64* res, const f64* a, const f64 factor);
void div3(f64* res, const f64* a, const f64 dividend);
void norm3(f64* res, const f64* a);
void cross3(f64* res, const f64* left, const f64* right);
void mul4x4(f64* Res, const f64* LHS, const f64* RHS);
void assignN(f64* res, const f64* v, u32 N);

#endif
