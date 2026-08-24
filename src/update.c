#include "field.h"
#include "update.h"

void update_E_serial(const em_field *restrict field, const float dt) {
    const float inv_dSx = 1 / field->dSx;
    const float inv_dSy = 1 / field->dSy;
    const float inv_dSz = 1 / field->dSz;

    const int sx = field->stride_x;
    const int sy = field->stride_y;
    const int sz = field->stride_z;

    for (int i = field->marigin_x_lo; i < field->Nx - field->marigin_x_hi; i++) {
        for (int j = field->marigin_y_lo; j < field->Ny - field->marigin_y_hi; j++) {
            int idx = i * sx + j * sy + 1 * sz;
            for (int k = field->marigin_z_lo; k < field->Nz - field->marigin_z_hi; k++) {
                const float inv_eps = field->inv_Eps[idx];
                const float a = dt * inv_eps;
                const float inv_denom = 1.0f / (1.0f + dt * field->Sigma[idx] * inv_eps);

                const float curl_x = (field->Hz[idx] - field->Hz[idx - sy]) * inv_dSy -
                                     (field->Hy[idx] - field->Hy[idx - sz]) * inv_dSz;

                const float curl_y = (field->Hx[idx] - field->Hx[idx - sz]) * inv_dSz -
                                     (field->Hz[idx] - field->Hz[idx - sx]) * inv_dSx;

                const float curl_z = (field->Hy[idx] - field->Hy[idx - sx]) * inv_dSx -
                                     (field->Hx[idx] - field->Hx[idx - sy]) * inv_dSy;

                field->Ex[idx] = (field->Ex[idx] + a * curl_x) * inv_denom;
                field->Ey[idx] = (field->Ey[idx] + a * curl_y) * inv_denom;
                field->Ez[idx] = (field->Ez[idx] + a * curl_z) * inv_denom;

                idx += sz;
            }
        }
    }
}

void update_H_serial(const em_field *restrict field, float dt) {
    const float inv_dSx = 1 / field->dSx;
    const float inv_dSy = 1 / field->dSy;
    const float inv_dSz = 1 / field->dSz;

    const int sx = field->stride_x;
    const int sy = field->stride_y;
    const int sz = field->stride_z;

    for (int i = field->marigin_x_lo; i < field->Nx - field->marigin_x_hi; i++) {
        for (int j = field->marigin_y_lo; j < field->Ny - field->marigin_y_hi; j++) {
            int idx = i * sx + j * sy + 1 * sz;
            for (int k = field->marigin_z_lo; k < field->Nz - field->marigin_z_hi; k++) {
                const float a = dt * field->inv_Mu[idx];

                const float curl_x = (field->Ez[idx + sy] - field->Ez[idx]) * inv_dSy -
                                     (field->Ey[idx + sz] - field->Ey[idx]) * inv_dSz;

                const float curl_y = (field->Ex[idx + sz] - field->Ex[idx]) * inv_dSz -
                                     (field->Ez[idx + sx] - field->Ez[idx]) * inv_dSx;

                const float curl_z = (field->Ey[idx + sx] - field->Ey[idx]) * inv_dSx -
                                     (field->Ex[idx + sy] - field->Ex[idx]) * inv_dSy;

                field->Hx[idx] -= a * curl_x;
                field->Hy[idx] -= a * curl_y;
                field->Hz[idx] -= a * curl_z;

                idx += sz;
            }
        }
    }
}

static void update_E_component_naive(
    const em_field *restrict field,
    float dt,
    float *E,
    const float *H1,
    float H1_diff,
    int H1_stride,
    const float *H2,
    float H2_diff,
    int H2_stride
) {
    for (int i = 1; i < field->Nx - 1; i++) {
        for (int j = 1; j < field->Ny - 1; j++) {
            for (int k = 1; k < field->Nz - 1; k++) {
                int idx = i * field->stride_x + j * field->stride_y + k * field->stride_z;
                float curl = (H1[idx] - H1[idx - H1_stride]) / H1_diff -
                             (H2[idx] - H2[idx - H2_stride]) / H2_diff;
                E[idx] = (E[idx] + (dt * field->inv_Eps[idx]) * curl) / (1.0f + (dt * field->Sigma[idx]) * field->inv_Eps[idx]);
            }
        }
    }
}

static void update_H_component_naive(
    const em_field *field,
    float dt,
    float *H,
    const float *E1,
    float E1_diff,
    int E1_stride,
    const float *E2,
    float E2_diff,
    int E2_stride
) {
    for (int i = 1; i < field->Nx - 1; i++) {
        for (int j = 1; j < field->Ny - 1; j++) {
            for (int k = 1; k < field->Nz - 1; k++) {
                int idx = i * field->stride_x + j * field->stride_y + k * field->stride_z;
                float curl = (E1[idx + E1_stride] - E1[idx]) / E1_diff -
                             (E2[idx + E2_stride] - E2[idx]) / E2_diff;
                H[idx] -= (dt * field->inv_Mu[idx]) * curl;
            }
        }
    }
}

void update_E_naive(const em_field *restrict field, float dt) {
    update_E_component_naive(field, dt, field->Ex, field->Hz, field->dSy, field->stride_y, field->Hy, field->dSz, field->stride_z);
    update_E_component_naive(field, dt, field->Ey, field->Hx, field->dSz, field->stride_z, field->Hz, field->dSx, field->stride_x);
    update_E_component_naive(field, dt, field->Ez, field->Hy, field->dSx, field->stride_x, field->Hx, field->dSy, field->stride_y);
}

void update_H_naive(const em_field *restrict field, float dt) {
    update_H_component_naive(field, dt, field->Hx, field->Ez, field->dSy, field->stride_y, field->Ey, field->dSz, field->stride_z);
    update_H_component_naive(field, dt, field->Hy, field->Ex, field->dSz, field->stride_z, field->Ez, field->dSx, field->stride_x);
    update_H_component_naive(field, dt, field->Hz, field->Ey, field->dSx, field->stride_x, field->Ex, field->dSy, field->stride_y);
}
