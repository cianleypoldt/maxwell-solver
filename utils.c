#include "utils.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MATH

#define MAT4SET(ptr, vals) memcpy(ptr, (double[16]) vals, sizeof(double[16]))
#define MAT4IDX(row, col)  ((row) * 4 + (col))

f64 len3(const f64* a) {
        return sqrt(a[0] + a[1] + a[2]);
}

f64 dot3(const f64* a, const f64* b) {
        return (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]);
}

void assign3(f64* res, const f64* v) {
        assignN(res, v, 3);
}

void add3(f64* res, const f64* a, const f64* b) {
        res[0] = a[0] + b[0];
        res[1] = a[1] + b[1];
        res[2] = a[2] + b[2];
}

void sub3(f64* res, const f64* a, const f64* b) {
        res[0] = a[0] - b[0];
        res[1] = a[1] - b[1];
        res[2] = a[2] - b[2];
}

void scale3(f64* res, const f64* a, const f64 factor) {
        res[0] = a[0] * factor;
        res[1] = a[1] * factor;
        res[2] = a[2] * factor;
}

void div3(f64* res, const f64* a, const f64 dividend) {
        res[0] = a[0] / dividend;
        res[1] = a[1] / dividend;
        res[2] = a[2] / dividend;
}

void norm3(f64* res, const f64* a) {
        f64 len = len3(a);
        div3(res, a, len);
}

void cross3(f64* res, const f64* left, const f64* right) {
        f64 tmp[3];
        tmp[0] = left[1] * right[2] - left[2] * right[1];
        tmp[1] = left[2] * right[0] - left[0] * right[2];
        tmp[2] = left[0] * right[1] - left[1] * right[0];
        assign3(res, tmp);
}

void mul4x4(f64* Res, const f64* LHS, const f64* RHS) {
        f64 tmp[16] = { 0 };
        for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 4; col++) {
                        for (int idx = 0; idx < 4; idx++) {
                                tmp[MAT4IDX(row, col)] +=
                                        LHS[MAT4IDX(row, idx)] *
                                        RHS[MAT4IDX(idx, col)];
                        }
                }
        }
        assignN(Res, tmp, 16);
}

void assignN(f64* res, const f64* v, u32 N) {
        for (int i = 0; i < N; i++) {
                res[i] = v[i];
        }
}

// FILE IO

file load_file(const char* path) {
        FILE* fp = fopen(path, "rb");
        if (!fp) {
                return (file) { NULL, 0 };
        }

        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (size < 0) {
                fclose(fp);
                return (file) { NULL, 0 };
        }
        void* buffer = malloc(size + 1);
        if (!buffer) {
                fclose(fp);
                return (file) { NULL, 0 };
        }

        fread(buffer, 1, size, fp);
        fclose(fp);

        return (file) { buffer, size };
}

void free_file(file f) {
        free(f.buffer);
}
