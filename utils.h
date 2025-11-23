#ifndef C_UTILS
#define C_UTILS

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MATH

typedef double   f64;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint32_t u32;
typedef uint64_t u64;

#ifndef M_PI
#        define M_PI 3.14159265359
#endif

#ifndef COL_MAJOR
#        define MATNIDX(row, col, rowN, colN) ((col) * rowN + (row))
#else
#        define MATNIDX(row, col, rowN, colN) ((row) * colN + (row))
#endif
#define MAT4IDX(row, col) MATNIDX((row), (col), 4, 4)

void assignN(f64* res, const f64* v, u32 N);
void fillN(f64* res, f64 val, u32 N);

f64  len3(const f64* a);
f64  dot3(const f64* a, const f64* b);
void add3(f64* res, const f64* a, const f64* b);
void scale3(f64* res, const f64* a, f64 factor);
void norm3(f64* res, const f64* a);
void cross3(f64* res, const f64* left, const f64* right);

void mul4x4vec4(f64* res, const f64* LHS, const f64* v);

void assign_identity4x4(f64* M);
void translate4x4(f64* Res, const f64* M, const f64* v);
void scale4x4(f64* Res, const f64* M, f64 s);

void mul4x4(f64* Res, const f64* LHS, const f64* RHS);

void rotate4x4X_euler(f64* Res, const f64* M, f64 angle);
void rotate4x4Y_euler(f64* Res, const f64* M, f64 angle);
void rotate4x4Z_euler(f64* Res, const f64* M, f64 angle);

// implementation missing
void get4x4rot_axis(f64* res, const f64* M);
void rotate4x4_axis(f64* Res, const f64* M, const f64* axis);
//

void transposeNxN(f64* Res, const f64* M, u32 N);

// debug
void printN(const f64* v, u32 N);
void printNxN(const f64* M, u32 rowN, u32 colN);
void printN_named(const f64* v, u32 N, const char* name);
void printNxN_named(const f64* M, u32 rowN, u32 colN, const char* name);

// FILE IO

typedef struct {
        void* buffer;
        u64   size;
} file;

file load_file(const char* path);
void free_file(file);

#endif
