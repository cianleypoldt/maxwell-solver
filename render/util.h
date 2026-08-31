#ifndef LINALG_UTILITY_HEADER
#define LINALG_UTILITY_HEADER
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAT_IDX(row, col, rows, cols) ((col) * (rows) + (row))

#define FP_EPS 1e-12

void mat_identity(float *Res, int n) {
    memset(Res, 0, n * n * sizeof(float));
    for (int i = 0; i < n; i++) {
        Res[MAT_IDX(i, i, n, n)] = 1;
    }
}

void mat_mul(float *Res, const float *A, const float *B, int rowsA, int colsA, int colsB) {
    float *tmp = (Res == A || Res == B) ? malloc(rowsA * colsB * sizeof(float)) : Res;

    for (int row = 0; row < rowsA; row++) {
        for (int col = 0; col < colsB; col++) {
            float sum = 0;
            for (int i = 0; i < colsA; i++) {
                sum += A[MAT_IDX(row, i, rowsA, colsA)] *
                       B[MAT_IDX(i, col, colsA, colsB)];
            }
            tmp[MAT_IDX(row, col, rowsA, colsB)] = sum;
        }
    }
    if (tmp != Res) {
        memcpy(Res, tmp, rowsA * colsB * sizeof(float));
        free(tmp);
    }
}

void mat_print(const float *M, int rows, int cols) {
    for (int row = 0; row < rows; row++) {
        printf("{ ");
        for (int col = 0; col < cols - 1; col++) {
            printf("%f, ", (float)M[MAT_IDX(row, col, rows, cols)]);
        }
        printf("%f }\n", (float)M[MAT_IDX(row, cols - 1, rows, cols)]);
    }
}

void vec_print(const float *v, int n) {
    printf("{ ");
    for (int i = 0; i < n - 1; i++) {
        printf("%f, ", (float)v[i]);
    }
    printf("%f }\n", (float)v[n - 1]);
}

float vec_length(const float *v, float n) {
    float len2 = 0;
    for (int i = 0; i < n; i++)
        len2 += v[i] * v[i];
    return sqrt(len2);
}

void vec_add(float *Res, const float *a, const float *b, int n) {
    for (int i = 0; i < n; i++) {
        Res[i] = a[i] + b[i];
    }
}

void vec_sub(float *Res, const float *a, const float *b, int n) {
    for (int i = 0; i < n; i++) {
        Res[i] = a[i] - b[i];
    }
}

void vec_scale(float *Res, const float *v, float factor, int n) {
    for (int i = 0; i < n; i++) {
        Res[i] = v[i] * factor;
    }
}

void vec_normalize(float *Res, const float *v, int n) {
    float len = vec_length(v, n);
    if (fabs((double)len) <= FP_EPS) {
        memset(Res, 0, sizeof(float) * n);
        return;
    }
    vec_scale(Res, v, 1 / len, n);
}

void vec3_cross(float *Res, const float *a, const float *b) {
    float t0 = a[1] * b[2] - a[2] * b[1];
    float t1 = a[2] * b[0] - a[0] * b[2];
    float t2 = a[0] * b[1] - a[1] * b[0];

    Res[0] = t0;
    Res[1] = t1;
    Res[2] = t2;
}

void quat_norm(float *q_out, float *q_in) {
    float len = sqrt(q_in[0] * q_in[0] + q_in[1] * q_in[1] + q_in[2] * q_in[2] + q_in[3] * q_in[3]);
    if (len != 0) {
        q_out[0] = q_in[0] / len;
        q_out[1] = q_in[1] / len;
        q_out[2] = q_in[2] / len;
        q_out[3] = q_in[3] / len;
    } else {
        memset(q_out, 0, sizeof(float) * 4);
    }
}

void quat_conj(float *q_out, float *q_in) {
    q_out[0] = q_in[0];
    q_out[1] = -q_in[1];
    q_out[2] = -q_in[2];
    q_out[3] = -q_in[3];
}

void quat_mul(float *q, float *q1, float *q2) {
    float temp[4];
    temp[0] = q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2] - q1[3] * q2[3];
    temp[1] = q1[0] * q2[1] + q1[1] * q2[0] + q1[2] * q2[3] - q1[3] * q2[2];
    temp[2] = q1[0] * q2[2] - q1[1] * q2[3] + q1[2] * q2[0] + q1[3] * q2[1];
    temp[3] = q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1] + q1[3] * q2[0];
    memcpy(q, temp, sizeof(float) * 4);
}

void quat_sandwitch(float *q_out, float *q1, float *q2) {
    float tmp_res[4];
    quat_mul(tmp_res, q2, q1);
    float tmp[4];
    quat_conj(tmp, q2);
    quat_mul(q_out, tmp_res, tmp);
}

void mat4_from_quat(float *M, float *q) {
    float w = q[0];
    float x = q[1];
    float y = q[2];
    float z = q[3];

    M[0] = 1.0f - 2.0f * (y * y + z * z);
    M[1] = 2.0f * (x * y + w * z);
    M[2] = 2.0f * (x * z - w * y);
    M[3] = 0.0f;

    M[4] = 2.0f * (x * y - w * z);
    M[5] = 1.0f - 2.0f * (x * x + z * z);
    M[6] = 2.0f * (y * z + w * x);
    M[7] = 0.0f;

    M[8] = 2.0f * (x * z + w * y);
    M[9] = 2.0f * (y * z - w * x);
    M[10] = 1.0f - 2.0f * (x * x + y * y);
    M[11] = 0.0f;

    M[12] = 0.0f;
    M[13] = 0.0f;
    M[14] = 0.0f;
    M[15] = 1.0f;
}

struct file {
    char *buffer;
    int size;
};

static struct file load_file(const char *path) {
    FILE *fp = fopen(path, "rb");

    if (!fp) {
        return (struct file){NULL, 0};
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) {
        fclose(fp);
        return (struct file){NULL, 0};
    }
    void *buffer = malloc(size);
    if (!buffer) {
        fclose(fp);
        return (struct file){NULL, 0};
    }

    fread(buffer, 1, size, fp);
    fclose(fp);

    return (struct file){buffer, size};
}

static void free_file(struct file f) {
    free(f.buffer);
}

#endif
