#include "utils.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MATH

void fillN(f64* res, f64 val, u32 N) {
        for (int i = 0; i < N; i++) {
                res[i] = val;
        }
}

void assignN(f64* res, const f64* v, u32 N) {
        if (res == v) {
                return;
        }
        memcpy(res, v, N * sizeof(f64));
}

f64 len3(const f64* a) {
        return sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}

f64 dot3(const f64* a, const f64* b) {
        return (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]);
}

void add3(f64* res, const f64* a, const f64* b) {
        res[0] = a[0] + b[0];
        res[1] = a[1] + b[1];
        res[2] = a[2] + b[2];
}

void scale3(f64* res, const f64* a, const f64 factor) {
        res[0] = a[0] * factor;
        res[1] = a[1] * factor;
        res[2] = a[2] * factor;
}

void norm3(f64* res, const f64* a) {
        f64 len = len3(a);
        scale3(res, a, 1 / len);
}

void cross3(f64* res, const f64* left, const f64* right) {
        f64 tmp[3];
        tmp[0] = left[1] * right[2] - left[2] * right[1];
        tmp[1] = left[2] * right[0] - left[0] * right[2];
        tmp[2] = left[0] * right[1] - left[1] * right[0];
        assignN(res, tmp, 3);
}

void mul4x4vec4(f64* res, const f64* LHS, const f64* v) {
        for (int i = 0; i < 4; i++) {
                res[i] = v[0] * LHS[MAT4IDX(i, 0)] + v[1] * LHS[MAT4IDX(i, 1)] +
                         v[2] * LHS[MAT4IDX(i, 2)] + v[3] * LHS[MAT4IDX(i, 3)];
        }
}

void assign_identity4x4(f64* M) {
        memset(M, 0, 16 * sizeof(f64));
        M[MAT4IDX(0, 0)] = 1;
        M[MAT4IDX(1, 1)] = 1;
        M[MAT4IDX(2, 2)] = 1;
        M[MAT4IDX(3, 3)] = 1;
}

void translate4x4(f64* Res, const f64* M, const f64* v) {
        assignN(Res, M, 16);
        Res[MAT4IDX(0, 3)] = M[MAT4IDX(0, 3)] + v[0];
        Res[MAT4IDX(1, 3)] = M[MAT4IDX(1, 3)] + v[1];
        Res[MAT4IDX(2, 3)] = M[MAT4IDX(2, 3)] + v[2];
}

void scale4x4(f64* Res, const f64* M, const f64 s) {
        assignN(Res, M, 16);
        Res[MAT4IDX(0, 0)] = M[MAT4IDX(0, 0)] * s;
        Res[MAT4IDX(1, 1)] = M[MAT4IDX(1, 1)] * s;
        Res[MAT4IDX(2, 2)] = M[MAT4IDX(2, 2)] * s;
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

void rotate4x4X_euler(f64* Res, const f64* M, f64 angle) {
        f64 s = sin(angle);
        f64 c = cos(angle);
        f64 m4_rotation[16];
        assign_identity4x4(m4_rotation);
        m4_rotation[MAT4IDX(1, 1)] = c;
        m4_rotation[MAT4IDX(1, 2)] = -s;
        m4_rotation[MAT4IDX(2, 1)] = s;
        m4_rotation[MAT4IDX(2, 2)] = c;
        mul4x4(Res, m4_rotation, M);
}

void rotate4x4Y_euler(f64* Res, const f64* M, f64 angle) {
        f64 s = sin(angle);
        f64 c = cos(angle);
        f64 m4_rotation[16];
        assign_identity4x4(m4_rotation);
        m4_rotation[MAT4IDX(0, 0)] = c;
        m4_rotation[MAT4IDX(0, 2)] = s;
        m4_rotation[MAT4IDX(2, 0)] = -s;
        m4_rotation[MAT4IDX(2, 2)] = c;
        mul4x4(Res, m4_rotation, M);
}

void rotate4x4Z_euler(f64* Res, const f64* M, f64 angle) {
        f64 s = sin(angle);
        f64 c = cos(angle);
        f64 m4_rotation[16];
        assign_identity4x4(m4_rotation);
        m4_rotation[MAT4IDX(0, 0)] = c;
        m4_rotation[MAT4IDX(0, 1)] = -s;
        m4_rotation[MAT4IDX(1, 0)] = s;
        m4_rotation[MAT4IDX(1, 1)] = c;
        mul4x4(Res, m4_rotation, M);
}

void get4x4rot_axis(f64* res, const f64* M) {}  // <<--------------

void rotate4x4_axis(f64* Res, const f64* M, const f64* axis) {}  // <<------

void transposeNxN(f64* Res, const f64* M, u32 N) {
        for (int row = 0; row < N; row++) {
                for (int col = 0; col < N; col++) {
                        Res[col * N + row] = M[row * N + col];
                }
        }
}

// debug
void printN(const f64* v, u32 N) {
        printf("{ ");
        for (int i = 0; i < N - 1; i++) {
                printf("%f, ", v[i]);
        }
        printf("%f }\n", v[N - 1]);
}

void printNxN(const f64* M, u32 rowN, u32 colN) {
        for (int row = 0; row < rowN; row++) {
                printf("{ ");
                for (int col = 0; col < colN - 1; col++) {
                        printf("%f, ", M[MATNIDX(row, col, rowN, colN)]);
                }
                printf("%f }\n", M[MATNIDX(row, colN - 1, rowN, colN)]);
        }
}

void printN_named(const f64* v, u32 N, const char* name) {
        printf("vec%i \"%s\":\n", N, name);
        printN(v, N);
}

void printNxN_named(const f64* M, u32 rowN, u32 colN, const char* name) {
        printf("mat%ix%i \"%s\":\n", rowN, colN, name);
        printNxN(M, rowN, colN);
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
        void* buffer = malloc(size);
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
